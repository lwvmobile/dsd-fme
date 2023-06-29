/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "dsd.h"

int
getSymbol (dsd_opts * opts, dsd_state * state, int have_sync)
{
  short sample, sample2;
  int i, sum, symbol, count;
  ssize_t result;

  sum = 0;
  count = 0;
  sample = 0; //init sample with a value of 0...see if this was causing issues with raw audio monitoring

  for (i = 0; i < state->samplesPerSymbol; i++) //right HERE
    {

      // timing control
      if ((i == 0) && (have_sync == 0))
        {
          if (state->samplesPerSymbol == 20)
            {
              if ((state->jitter >= 7) && (state->jitter <= 10))
                {
                  i--;
                }
              else if ((state->jitter >= 11) && (state->jitter <= 14))
                {
                  i++;
                }
            }
          else if (state->rf_mod == 1)
            {
              if ((state->jitter >= 0) && (state->jitter < state->symbolCenter))
                {
                  i++;          // fall back
                }
              else if ((state->jitter > state->symbolCenter) && (state->jitter < 10))
                {
                  i--;          // catch up
                }
            }
          else if (state->rf_mod == 2)
            {
              if ((state->jitter >= state->symbolCenter - 1) && (state->jitter <= state->symbolCenter))
                {
                  i--;
                }
              else if ((state->jitter >= state->symbolCenter + 1) && (state->jitter <= state->symbolCenter + 2))
                {
                  i++;
                }
            }
          else if (state->rf_mod == 0)
            {
              if ((state->jitter > 0) && (state->jitter <= state->symbolCenter))
                {
                  i--;          // catch up
                }
              else if ((state->jitter > state->symbolCenter) && (state->jitter < state->samplesPerSymbol))
                {
                  i++;          // fall back
                }
            }
          state->jitter = -1;
        }

      // Read the new sample from the input
      if(opts->audio_in_type == 0) //pulse audio
      {
        pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
      }

      else if (opts->audio_in_type == 5) //OSS
      {
        read (opts->audio_in_fd, &sample, 2);
      }

      //stdin only, wav files moving to new number
      else if (opts->audio_in_type == 1) //won't work in windows, needs posix pipe (mintty)
      {
        result = sf_read_short(opts->audio_in_file, &sample, 1);
        if(result == 0)
        {
          sf_close(opts->audio_in_file);
          cleanupAndExit (opts, state);
        }
      }
      //wav files, same but using seperate value so we can still manipulate ncurses menu
      //since we can not worry about getch/stdin conflict
      else if (opts->audio_in_type == 2)
      {
        result = sf_read_short(opts->audio_in_file, &sample, 1);
        if(result == 0)
        {

          sf_close(opts->audio_in_file);
          fprintf (stderr, "\n\nEnd of .wav file.\n");
          //open pulse input if we are pulse output AND using ncurses terminal
          if (opts->audio_out_type == 0 && opts->use_ncurses_terminal == 1)
          {
            opts->audio_in_type = 0; //set input type
            openPulseInput(opts); //open pulse input
          } 
          //else cleanup and exit
          else
          {
            if (opts->use_ncurses_terminal == 1) ncursesClose(opts);
            cleanupAndExit(opts, state);     
          }        
        }
      }
      else if (opts->audio_in_type == 3)
      {
#ifdef USE_RTLSDR
        // TODO: need to read demodulated stream here
        // get_rtlsdr_sample(&sample);
        get_rtlsdr_sample(&sample, opts, state);
        if (opts->monitor_input_audio == 1 && state->lastsynctype == -1 && sample < 32767 && sample > -32767)
        {
          state->pulse_raw_out_buffer = sample; //steal raw out buffer sample here?
          pa_simple_write(opts->pulse_raw_dev_out, (void*)&state->pulse_raw_out_buffer, 2, NULL);
        }
        opts->rtl_rms = rtl_return_rms();

#endif
      }

      //tcp socket input from SDR++ -- now with 1 retry if connection is broken
      else if (opts->audio_in_type == 8)
      {
        #ifdef AERO_BUILD
        result = sf_read_short(opts->tcp_file_in, &sample, 1);
        if(result == 0) {
          fprintf (stderr, "\nConnection to TCP Server Interrupted. Trying again in 3 seconds.\n");
          sample = 0;
          sf_close(opts->tcp_file_in); //close current connection on this end
          sleep (3); //halt all processing and wait 3 seconds

          //attempt to reconnect to socket
          opts->tcp_sockfd = 0;  
          opts->tcp_sockfd = Connect(opts->tcp_hostname, opts->tcp_portno);
          if (opts->tcp_sockfd != 0)
          {
            //reset audio input stream
            opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
            opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
            opts->audio_in_file_info->channels=1;
            opts->audio_in_file_info->seekable=0;
            opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
            opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);

            if(opts->tcp_file_in == NULL)
            {
              fprintf(stderr, "Error, couldn't Reconnect to TCP with libsndfile: %s\n", sf_strerror(NULL));
            }
            else fprintf (stderr, "TCP Socket Reconnected Successfully.\n");
          }
          else fprintf (stderr, "TCP Socket Connection Error.\n");          

          //now retry reading sample
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
          if (result == 0) {
            sf_close(opts->tcp_file_in);
            opts->audio_in_type = 0; //set input type
            opts->tcp_sockfd = 0; //added this line so we will know if it connected when using ncurses terminal keyboard shortcut
            //openPulseInput(opts); //open pulse inpput
            sample = 0; //zero sample on bad result, keep the ball rolling
            //open pulse input if we are pulse output AND using ncurses terminal
            if (opts->audio_out_type == 0 && opts->use_ncurses_terminal == 1)
            {
              fprintf (stderr, "Connection to TCP Server Disconnected.\n");
              fprintf (stderr, "Opening Pulse Audio Input.\n");
              opts->audio_in_type = 0; //set input type
              openPulseInput(opts); //open pulse input
            } 
            //else cleanup and exit
            else 
            {
              fprintf (stderr, "Connection to TCP Server Disconnected.\n");
              fprintf (stderr, "Closing DSD-FME.\n");
              if (opts->use_ncurses_terminal == 1) ncursesClose(opts);
              cleanupAndExit(opts, state);
            }
            
          }
          
        }
        #else
        result = sf_read_short(opts->tcp_file_in, &sample, 1);
        if(result == 0) {
          fprintf (stderr, "\nConnection to TCP Server Interrupted. Trying again in 3 seconds.\n");
          sample = 0;
          sf_close(opts->tcp_file_in); //close current connection on this end
          sleep (3); //halt all processing and wait 3 seconds

          //attempt to reconnect to socket
          opts->tcp_sockfd = 0;  
          opts->tcp_sockfd = Connect(opts->tcp_hostname, opts->tcp_portno);
          if (opts->tcp_sockfd != 0)
          {
            //reset audio input stream
            opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
            opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
            opts->audio_in_file_info->channels=1;
            opts->audio_in_file_info->seekable=0;
            opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
            opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);

            if(opts->tcp_file_in == NULL)
            {
              fprintf(stderr, "Error, couldn't Reconnect to TCP with libsndfile: %s\n", sf_strerror(NULL));
            }
            else fprintf (stderr, "TCP Socket Reconnected Successfully.\n");
          }
          else fprintf (stderr, "TCP Socket Connection Error.\n");          

          //now retry reading sample
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
          if (result == 0) {
            sf_close(opts->tcp_file_in);
            opts->audio_in_type = 0; //set input type
            opts->tcp_sockfd = 0; //added this line so we will know if it connected when using ncurses terminal keyboard shortcut
            openPulseInput(opts); //open pulse inpput
            sample = 0; //zero sample on bad result, keep the ball rolling
            fprintf (stderr, "Connection to TCP Server Disconnected.\n");
          }
          
        }
        #endif
        
      }

      //tcp socket input from SDR++ -- old method to open pulse or quit right away
      // else if (opts->audio_in_type == 8)
      // {
      //   result = sf_read_short(opts->tcp_file_in, &sample, 1);
      //   if(result == 0) {

      //     fprintf (stderr, "Connection to TCP Server Disconnected.\n");
      //     //open pulse input if we are pulse output AND using ncurses terminal
      //     if (opts->audio_out_type == 0 && opts->use_ncurses_terminal == 1)
      //     {
      //       opts->audio_in_type = 0; //set input type
      //       openPulseInput(opts); //open pulse input
      //     } 
      //     //else cleanup and exit
      //     else cleanupAndExit(opts, state);
          
      //   }
      // }

      //UDP Socket input...not working correct. Reads samples, but no sync
      // else if (opts->audio_in_type == 6)
      // {
        //I think this doesn't get the entire dgram when we run sf_read_short on the udp dgram
        // result = sf_read_short(opts->udp_file_in, &sample, 1); 
        // if (sample != 0)
        //   fprintf (stderr, "Result = %d Sample = %d \n", result, sample);
      // }

      if (opts->use_cosine_filter)
        {
          if ( (state->lastsynctype >= 10 && state->lastsynctype <= 13) || state->lastsynctype == 32 || state->lastsynctype == 33 
                || state->lastsynctype == 34 || state->lastsynctype == 30 || state->lastsynctype == 31)
          {
            sample = dmr_filter(sample);
          }

          else if (state->lastsynctype == 8  || state->lastsynctype == 9  ||
               state->lastsynctype == 16 || state->lastsynctype == 17 ||
               state->lastsynctype == 20 || state->lastsynctype == 21 ||
               state->lastsynctype == 22 || state->lastsynctype == 23 ||
               state->lastsynctype == 24 || state->lastsynctype == 25 ||
               state->lastsynctype == 26 || state->lastsynctype == 27 ||
               state->lastsynctype == 28 || state->lastsynctype == 29  ) //||
               //state->lastsynctype == 35 || state->lastsynctype == 36) //phase 2 C4FM disc tap input
            {
              //if(state->samplesPerSymbol == 20)
              if(opts->frame_nxdn48 == 1)
              {
                sample = nxdn_filter(sample);
              }
              //else if (state->lastsynctype >= 20 && state->lastsynctype <=27) //this the right range?
              else if (opts->frame_dpmr == 1)
              {
                sample = dpmr_filter(sample);
              }
              else if (state->samplesPerSymbol == 8) //phase 2 cqpsk
              {
                //sample = dmr_filter(sample); //work on filter later
              }
              else // the 12.5KHz NXDN filter is the same as the DMR filter...hopefully
              {
                sample = dmr_filter(sample);
              }
            }
        }

      if ((sample > state->max) && (have_sync == 1) && (state->rf_mod == 0))
        {
          sample = state->max;
        }
      else if ((sample < state->min) && (have_sync == 1) && (state->rf_mod == 0))
        {
          sample = state->min;
        }

      if (sample > state->center)
        {
          if (state->lastsample < state->center)
            {
              state->numflips += 1;
            }
          if (sample > (state->maxref * 1.25))
            {
              if (state->lastsample < (state->maxref * 1.25))
                {
                  state->numflips += 1;
                }
              if ((state->jitter < 0) && (state->rf_mod == 1))
                {               // first spike out of place
                  state->jitter = i;
                }
              if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                  fprintf (stderr, "O");
                }
            }
          else
            {
              if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                  fprintf (stderr, "+");
                }
              if ((state->jitter < 0) && (state->lastsample < state->center) && (state->rf_mod != 1))
                {               // first transition edge
                  state->jitter = i;
                }
            }
        }
      else
        {                       // sample < 0
          if (state->lastsample > state->center)
            {
              state->numflips += 1;
            }
          if (sample < (state->minref * 1.25))
            {
              if (state->lastsample > (state->minref * 1.25))
                {
                  state->numflips += 1;
                }
              if ((state->jitter < 0) && (state->rf_mod == 1))
                {               // first spike out of place
                  state->jitter = i;
                }
              if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                  fprintf (stderr, "X");
                }
            }
          else
            {
              if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                  fprintf (stderr, "-");
                }
              if ((state->jitter < 0) && (state->lastsample > state->center) && (state->rf_mod != 1))
                {               // first transition edge
                  state->jitter = i;
                }
            }
        }

      if (state->samplesPerSymbol == 20) //nxdn 4800 baud 2400 symbol rate
        {
          if ((i >= 9) && (i <= 11))
            {
              sum += sample;
              count++;
            }
        }
      if (state->samplesPerSymbol == 5) //provoice or gfsk
        {
          if (i == 2)
            {
              sum += sample;
              count++;
            }
        }
      else
        {
          if (state->rf_mod == 0)
            {
              // 0: C4FM modulation

              if ((i >= state->symbolCenter - 1) && (i <= state->symbolCenter + 2))
                {
                  sum += sample;
                  count++;
                }

#ifdef TRACE_DSD
              if (i == state->symbolCenter - 1) {
                  state->debug_sample_left_edge = state->debug_sample_index - 1;
              }
              if (i == state->symbolCenter + 2) {
                  state->debug_sample_right_edge = state->debug_sample_index - 1;
              }
#endif
            }
          else
            {
              // 1: QPSK modulation
              // 2: GFSK modulation
              // Note: this has been changed to use an additional symbol to the left
              // On the p25_raw_unencrypted.flac it is evident that the timing
              // comes one sample too late.
              // This change makes a significant improvement in the BER, at least for
              // this file.
              //if ((i == state->symbolCenter) || (i == state->symbolCenter + 1))
              if ((i == state->symbolCenter - 1) || (i == state->symbolCenter + 1))
                {
                  sum += sample;
                  count++;
                }

#ifdef TRACE_DSD
              //if (i == state->symbolCenter) {
              if (i == state->symbolCenter - 1) {
                  state->debug_sample_left_edge = state->debug_sample_index - 1;
              }
              if (i == state->symbolCenter + 1) {
                  state->debug_sample_right_edge = state->debug_sample_index - 1;
              }
#endif
            }
        }


      state->lastsample = sample;

    }

  symbol = (sum / count);

  if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
    {
      if (state->jitter >= 0)
        {
          fprintf (stderr, " %i\n", state->jitter);
        }
      else
        {
          fprintf (stderr, "\n");
        }
    }

#ifdef TRACE_DSD
  if (state->samplesPerSymbol == 10) {
      float left, right;
      if (state->debug_label_file == NULL) {
          state->debug_label_file = fopen ("pp_label.txt", "w");
      }
      left = state->debug_sample_left_edge / SAMPLE_RATE_IN;
      right = state->debug_sample_right_edge / SAMPLE_RATE_IN;
      if (state->debug_prefix != '\0') {
          if (state->debug_prefix == 'I') {
              fprintf(state->debug_label_file, "%f\t%f\t%c%c %i\n", left, right, state->debug_prefix, state->debug_prefix_2, symbol);
          } else {
              fprintf(state->debug_label_file, "%f\t%f\t%c %i\n", left, right, state->debug_prefix, symbol);
          }
      } else {
          fprintf(state->debug_label_file, "%f\t%f\t%i\n", left, right, symbol);
      }
  }
#endif

  //test throttle on wav input files
	if (opts->audio_in_type == 2)
	{
		if (state->use_throttle == 1) usleep(.003); //very environment specific, tuning to cygwin
	}

  //read op25/fme symbol bin files
  if (opts->audio_in_type == 4)
  {
    //use fopen and read in a symbol, check op25 for clues
    if(opts->symbolfile == NULL)
    {
      fprintf(stderr, "Error Opening File %s\n", opts->audio_in_dev); //double check this
      return(-1);
    }

    state->symbolc = fgetc(opts->symbolfile);

    //experimental throttle
    useconds_t stime = state->symbol_throttle;
    if (state->use_throttle == 1)
    {
      // usleep(stime);
      usleep(.003); //very environment specific, tuning to cygwin
    }
    //fprintf(stderr, "%d", state->symbolc);
    if( feof(opts->symbolfile) )
    {
      // opts->audio_in_type = 0; //switch to pulse after playback, ncurses terminal can initiate replay if wanted
      fclose(opts->symbolfile);
      fprintf (stderr, "\n\nEnd of .bin file\n");
      //open pulse input if we are pulse output AND using ncurses terminal
      if (opts->audio_out_type == 0 && opts->use_ncurses_terminal == 1)
      {
        opts->audio_in_type = 0; //set input type
        openPulseInput(opts); //open pulse input
      } 
      //else cleanup and exit
      else
      {
        if (opts->use_ncurses_terminal == 1) ncursesClose(opts);
        cleanupAndExit(opts, state);
      }
    }

    //assign symbol/dibit values based on modulation type
    if (state->rf_mod == 2) //GFSK
    {
      symbol = state->symbolc;
      if (state->symbolc == 0 ) 
      {
        symbol = -3; //-1
      }
      if (state->symbolc == 1 ) 
      {
        symbol = -1; //-3
      }
    }
    else //everything else
    {
      if (state->symbolc == 0)
      {
        symbol = 1; //-1
      }
      if (state->symbolc == 1)
      {
        symbol = 3; //-3
      }
      if (state->symbolc == 2)
      {
        symbol = -1; //1
      }
      if (state->symbolc == 3)
      {
        symbol = -3; //3
      }
    }

  }

  state->symbolcnt++;
  return (symbol);
}
