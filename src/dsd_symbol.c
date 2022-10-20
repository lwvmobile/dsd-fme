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
      if(opts->audio_in_type == 0) // && state->menuopen == 0 still not quite working
      {
          pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
          //look into how processAudio handles playback, not sure if latency issues, or garbage sample values crash pulse when written
          // if (opts->monitor_input_audio == 1 && state->lastsynctype == -1 && sample < 32767 && sample > -32767)
          if (opts->monitor_input_audio == 1 && state->lastsynctype == -1 && sample != 0)
          {
            state->pulse_raw_out_buffer = sample; //steal raw out buffer sample here?
            pa_simple_write(opts->pulse_raw_dev_out, (void*)&state->pulse_raw_out_buffer, 2, NULL);
          }
          //playback is wrong, depends on samples_per_symbol, maybe make a buffer and store first?
          //making buffer might also fix raw audio monitoring as well
          if (opts->wav_out_file_raw[0] != 0) //if set for recording sample raw files
          {
            //writeRawSample (opts, state, sample);
          }

      }
      else if (opts->audio_in_type == 1) {
          result = sf_read_short(opts->audio_in_file, &sample, 1);
          //fprintf (stderr, "..");
          if (opts->monitor_input_audio == 1 && state->lastsynctype == -1 && sample < 32767 && sample > -32767)
          {
            state->pulse_raw_out_buffer = sample; //steal raw out buffer sample here?
            pa_simple_write(opts->pulse_raw_dev_out, (void*)&state->pulse_raw_out_buffer, 2, NULL);
          }
          if(result == 0) {
              cleanupAndExit (opts, state);
          }
      }
      else if (opts->audio_in_type == 2)
      {
#ifdef USE_PORTAUDIO
        PaError err = Pa_ReadStream( opts->audio_in_pa_stream, &sample, 1 );
        if( err != paNoError )
        {
              fprintf( stderr, "An error occured while using the portaudio input stream\n" );
              fprintf( stderr, "Error number: %d\n", err );
              fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        }


#endif
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

#endif
      }

#ifdef TRACE_DSD
      state->debug_sample_index++;
#endif

      // printf ("res: %zd\n, offset: %lld", result, sf_seek(opts->audio_in_file, 0, SEEK_CUR));
      if (opts->use_cosine_filter)
        {
          if ( (state->lastsynctype >= 10 && state->lastsynctype <= 13) || state->lastsynctype == 32 || state->lastsynctype == 33 || state->lastsynctype == 34)
          {
            sample = dmr_filter(sample);
          }

          else if (state->lastsynctype == 8  || state->lastsynctype == 9  ||
               state->lastsynctype == 16 || state->lastsynctype == 17 ||
               state->lastsynctype == 20 || state->lastsynctype == 21 ||
               state->lastsynctype == 22 || state->lastsynctype == 23 ||
               state->lastsynctype == 24 || state->lastsynctype == 25 ||
               state->lastsynctype == 26 || state->lastsynctype == 27 ) //||
               //state->lastsynctype == 35 || state->lastsynctype == 36) //phase 2
            {
              if(state->samplesPerSymbol == 20)
              {
                sample = nxdn_filter(sample);
              }
              else if (state->samplesPerSymbol == 8) //phase 2 cqpsk
              {
                //sample = dmr_filter(sample); //work on filter later
              }
              else // the 12.5KHz NXDN filter is the same as the DMR filter
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
      usleep(stime);
    }
    //fprintf(stderr, "%d", state->symbolc);
    if( feof(opts->symbolfile) )
    {
      opts->audio_in_type = 0; //switch to pulse after playback, ncurses terminal can initiate replay if wanted
      fclose(opts->symbolfile);
      openPulseInput(opts);
    }

    //assign symbol/dibit values based on modulation type
    if (state->rf_mod == 2) //GFSK
    {
      symbol = state->symbolc;
      if (state->symbolc == 0 && state->synctype >= 0)
      {
        symbol = -3; //-1
      }
      if (state->symbolc == 1 && state->synctype >= 0)
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
