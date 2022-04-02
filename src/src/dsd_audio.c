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

pa_sample_spec ss;
pa_sample_spec tt;
pa_sample_spec zz;
pa_sample_spec cc;
//pa_channel_map channel_map;


void openPulseOutput(dsd_opts * opts)
{

  ss.format = PA_SAMPLE_S16NE;
  ss.channels = opts->pulse_raw_out_channels; //doing tests with 2 channels at 22050 for 44100 audio default in pulse
  ss.rate = opts->pulse_raw_rate_out; //48000

  tt.format = PA_SAMPLE_S16NE;
  tt.channels = opts->pulse_digi_out_channels; //doing tests with 2 channels at 22050 for 44100 audio default in pulse
  tt.rate = opts->pulse_digi_rate_out; //48000, switches to 8000 when using RTL dongle
  //fprintf (stderr,"digi rate out = %d\n", opts->pulse_digi_rate_out);
  //pa_channel_map_init_stereo(&channel_map);
  //ss
  if (opts->monitor_input_audio == 1)
  //if (opts->frame_dmr == 1  && opts->audio_in_type != 3)
  {
    opts->pulse_raw_dev_out  = pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, NULL, "Raw Audio", &ss, NULL, NULL, NULL);
    //opts->pulse_raw_dev_out  = pa_simple_new(NULL, "DSD FME", PA_STREAM_PLAYBACK, NULL, "DMR/MOTOTRBO Right", &ss, NULL, NULL, NULL);
  }

//tt
  pa_channel_map* left = 0; //NULL and 0 are same in this context
  pa_channel_map* right = 0; //NULL and 0 are same in this context
  //pa_channel_map* right;
  //pa_channel_map_init(right);
  opts->pulse_digi_dev_out = pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, NULL, opts->output_name, &tt, left, NULL, NULL);
  //opts->pulse_digi_dev_out = pa_simple_new(NULL, "DSD FME", PA_STREAM_PLAYBACK, NULL, "Digi Audio Out  LEFT", &tt, left, NULL, NULL);
  //opts->pulse_raw_dev_out =  pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, NULL, "Digi Audio Out RIGHT", &tt, right, NULL, NULL);
}

void openPulseInput(dsd_opts * opts)
{

  zz.format = PA_SAMPLE_S16NE;
  zz.channels = opts->pulse_raw_in_channels;
  zz.rate = opts->pulse_raw_rate_in; //48000

  cc.format = PA_SAMPLE_S16NE;
  cc.channels = opts->pulse_digi_in_channels;
  cc.rate = opts->pulse_digi_rate_in; //48000

  //zz
  if (opts->monitor_input_audio == 1)
  {
    //opts->pulse_raw_dev_in = pa_simple_new(NULL, "DSD-FME", PA_STREAM_RECORD, NULL, "Raw Audio In", &zz, NULL, NULL, NULL);
  }
  //cc
  opts->pulse_digi_dev_in  = pa_simple_new(NULL, "DSD-FME", PA_STREAM_RECORD, NULL, opts->output_name, &cc, NULL, NULL, NULL);

}
//playRawAudio may be obsoleted now that I can run these correctly right inside of dsd_symbol
void playRawAudio(dsd_opts * opts, dsd_state * state)
{
  short obuf, outl, i, sample2, something;
  something = state->samplesPerSymbol / 1; //1 for buffer steal loops, 5 for rtl loop
  //something = 1;
  if (opts->audio_in_type == 0 && opts->audio_out_type == 0){ //hack, but might as well for this particular type since its nearly perfect
    for (i=0; i < something; i++){
      //read (opts->audio_in_fd, &sample2, 2); //reading here seems to get same speed as gfsk modulation
      //pa_simple_drain(opts->pulse_raw_dev_out, NULL);
      //pa_simple_read(opts->pulse_raw_dev_in, &state->pulse_raw_out_buffer, 2, NULL );
      //obuf = state->pulse_raw_out_buffer / 4;  //dividing here 'quietens' it down a little bit, helps with clicking and clipping
      //obuf = state->input_sample_buffer;
      //obuf = state->pulse_raw_out_buffer;
      //write (opts->audio_out_fd, (void*)&obuf, 2);
      //pa_simple_write(opts->pulse_raw_dev_out, (void*)&obuf, 2, NULL);
      //pa_simple_write(opts->pulse_digi_dev_out, (void*)&obuf, 2, NULL);
      //pa_simple_drain(opts->pulse_raw_dev_out, NULL);
    }
  }
} //end playRawAudio


void
processAudio (dsd_opts * opts, dsd_state * state)
{

  int i, n;
  float aout_abs, max, gainfactor, gaindelta, maxbuf;

  if (opts->audio_gain == (float) 0)
    {
      // detect max level
      max = 0;
      state->audio_out_temp_buf_p = state->audio_out_temp_buf;
      //state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
      for (n = 0; n < 160; n++)
        {
          aout_abs = fabsf (*state->audio_out_temp_buf_p);
          if (aout_abs > max)
            {
              max = aout_abs;
            }
          state->audio_out_temp_buf_p++;
          //state->audio_out_temp_buf_pR++;
        }
      *state->aout_max_buf_p = max;
      //*state->aout_max_buf_pR = max;
      state->aout_max_buf_p++;
      //state->aout_max_buf_pR++;
      state->aout_max_buf_idx++;
      //state->aout_max_buf_idxR++;
      if (state->aout_max_buf_idx > 24)
        {
          state->aout_max_buf_idx = 0;
          state->aout_max_buf_p = state->aout_max_buf;
        }
      //if (state->aout_max_buf_idxR > 24)
      //  {
      //    state->aout_max_buf_idxR = 0;
      //    state->aout_max_buf_pR = state->aout_max_bufR;
      //  }

      // lookup max history
      for (i = 0; i < 25; i++)
        {
          maxbuf = state->aout_max_buf[i];
          if (maxbuf > max)
            {
              max = maxbuf;
            }
        }

      // determine optimal gain level
      if (max > (float) 0)
        {
          gainfactor = ((float) 30000 / max);
        }
      else
        {
          gainfactor = (float) 50;
        }
      if (gainfactor < state->aout_gain)
        {
          state->aout_gain = gainfactor;
          gaindelta = (float) 0;
        }
      else
        {
          if (gainfactor > (float) 50)
            {
              gainfactor = (float) 50;
            }
          gaindelta = gainfactor - state->aout_gain;
          if (gaindelta > ((float) 0.05 * state->aout_gain))
            {
              gaindelta = ((float) 0.05 * state->aout_gain);
            }
        }
      gaindelta /= (float) 160;
    }
  else
    {
      gaindelta = (float) 0;
    }

  if(opts->audio_gain >= 0)
    {
      // adjust output gain
      state->audio_out_temp_buf_p = state->audio_out_temp_buf;
      for (n = 0; n < 160; n++)
        {
          *state->audio_out_temp_buf_p = (state->aout_gain + ((float) n * gaindelta)) * (*state->audio_out_temp_buf_p);
          state->audio_out_temp_buf_p++;
        }
      state->aout_gain += ((float) 160 * gaindelta);
    }

  // copy audio data to output buffer and upsample if necessary
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;
  if (opts->split == 0)
    {
      for (n = 0; n < 160; n++)
        {
          upsample (state, *state->audio_out_temp_buf_p);
          state->audio_out_temp_buf_p++;
          state->audio_out_float_buf_p += 6;
          state->audio_out_idx += 6;
          state->audio_out_idx2 += 6;
        }
      state->audio_out_float_buf_p -= (960 + opts->playoffset);
      // copy to output (short) buffer
      for (n = 0; n < 960; n++)
        {
          if (*state->audio_out_float_buf_p >  32767.0F)
            {
              *state->audio_out_float_buf_p = 32767.0F;
            }
          else if (*state->audio_out_float_buf_p < -32768.0F)
            {
              *state->audio_out_float_buf_p = -32768.0F;
            }
          *state->audio_out_buf_p = (short) *state->audio_out_float_buf_p;
          state->audio_out_buf_p++;
          state->audio_out_float_buf_p++;
        }
      state->audio_out_float_buf_p += opts->playoffset;
    }
  else
    {
      for (n = 0; n < 160; n++)
        {
          if (*state->audio_out_temp_buf_p > 32767.0F)
            {
              *state->audio_out_temp_buf_p = 32767.0F;
            }
          else if (*state->audio_out_temp_buf_p < -32768.0F)
            {
              *state->audio_out_temp_buf_p = -32768.0F;
            }
          *state->audio_out_buf_p = (short) *state->audio_out_temp_buf_p;
          state->audio_out_buf_p++;
          state->audio_out_temp_buf_p++;
          state->audio_out_idx++;
          state->audio_out_idx2++;
        }
    }
}

void
writeSynthesizedVoice (dsd_opts * opts, dsd_state * state)
{
  int n;
  short aout_buf[160];
  short *aout_buf_p;

//  for(n=0; n<160; n++)
//    fprintf (stderr,"%d ", ((short*)(state->audio_out_temp_buf))[n]);
//  fprintf (stderr,"\n");

  aout_buf_p = aout_buf;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;

  for (n = 0; n < 160; n++)
  {
    if (*state->audio_out_temp_buf_p > (float) 32767)
      {
        *state->audio_out_temp_buf_p = (float) 32767;
      }
    else if (*state->audio_out_temp_buf_p < (float) -32768)
      {
        *state->audio_out_temp_buf_p = (float) -32768;
      }
      *aout_buf_p = (short) *state->audio_out_temp_buf_p;
      aout_buf_p++;
      state->audio_out_temp_buf_p++;
  }

  sf_write_short(opts->wav_out_f, aout_buf, 160);

  /*

  int n;
  short aout_buf[160];
  short *aout_buf_p;
  ssize_t result;

  aout_buf_p = aout_buf;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;
  for (n = 0; n < 160; n++)
    {
      if (*state->audio_out_temp_buf_p > (float) 32760)
        {
          *state->audio_out_temp_buf_p = (float) 32760;
        }
      else if (*state->audio_out_temp_buf_p < (float) -32760)
        {
          *state->audio_out_temp_buf_p = (float) -32760;
        }
      *aout_buf_p = (short) *state->audio_out_temp_buf_p;
      aout_buf_p++;
      state->audio_out_temp_buf_p++;
    }

  result = write (opts->wav_out_fd, aout_buf, 320);
  fflush (opts->wav_out_f);
  state->wav_out_bytes += 320;
  */
}

void
playSynthesizedVoice (dsd_opts * opts, dsd_state * state)
{
  ssize_t result;

  if (state->audio_out_idx > opts->delay)
    {
      // output synthesized speech to sound card
		if(opts->audio_out_type == 2)
		{
#ifdef USE_PORTAUDIO
			PaError err = paNoError;
			do
			{
				long available = Pa_GetStreamWriteAvailable( opts->audio_out_pa_stream );
				if(available < 0)
					err = available;
				//fprintf (stderr,"Frames available: %d\n", available);
				if( err != paNoError )
					break;
				if(available > SAMPLE_RATE_OUT * PA_LATENCY_OUT)
				{
					//It looks like this might not be needed for very small latencies. However, it's definitely needed for a bit larger ones.
					//When PA_LATENCY_OUT == 0.500 I get output buffer underruns if I don't use this. With PA_LATENCY_OUT <= 0.100 I don't see those happen.
					//But with PA_LATENCY_OUT < 0.100 I run the risk of choppy audio and stream errors.
					fprintf (stderr,"\nSyncing voice output stream\n");
					err = Pa_StopStream( opts->audio_out_pa_stream );
					if( err != paNoError )
						break;
				}

				err = Pa_IsStreamActive( opts->audio_out_pa_stream );
				if(err == 0)
				{
					fprintf (stderr,"Start voice output stream\n");
					err = Pa_StartStream( opts->audio_out_pa_stream );
				}
				else if(err == 1)
				{
					err = paNoError;
				}
				if( err != paNoError )
					break;

				err = Pa_WriteStream( opts->audio_out_pa_stream, (state->audio_out_buf_p - state->audio_out_idx), state->audio_out_idx );
				if( err != paNoError )
					break;
			} while(0);

			if( err != paNoError )
			{
				fprintf (stderr, "An error occured while using the portaudio output stream\n" );
        fprintf (stderr, "Error number: %d\n", err );
				fprintf (stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
			}

#endif
		}
		else
      if (opts->monitor_input_audio == 1){
        //pa_simple_flush(opts->pulse_raw_dev_in, NULL);
        //pa_simple_flush(opts->pulse_raw_dev_out, NULL);
        //state->pulse_raw_out_buffer = 0;
        //pa_simple_write(opts->pulse_raw_dev_out, (void*)&state->pulse_raw_out_buffer, 2, NULL);

      }

      //two slot audio testing, still need to seperate channels first internally, but this will play them out of different streams
      /*
      if(state->currentslot == 0 && opts->audio_in_type != 3)
      {
        pa_simple_write(opts->pulse_digi_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); //Yay! It works.
      }
      if(state->currentslot == 1 && opts->audio_in_type != 3)
      {
        pa_simple_write(opts->pulse_raw_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); //Yay! It works.
        //pa_simple_write(opts->pulse_raw_dev_out, (state->audio_out_buf_pR - state->audio_out_idxR), (state->audio_out_idxR * 2), NULL); //Yay! It works.
      }

      if (opts->audio_in_type == 3)
      {
        pa_simple_write(opts->pulse_digi_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); //Yay! It works.
      }
      */

      pa_simple_write(opts->pulse_digi_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); //Yay! It works.
      state->audio_out_idx = 0;
  }

  if (state->audio_out_idx2 >= 800000)
    {
      state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
      state->audio_out_buf_p = state->audio_out_buf + 100;
      memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
      memset (state->audio_out_buf, 0, 100 * sizeof (short));
      state->audio_out_idx2 = 0;
    }
}

#ifdef USE_PORTAUDIO
int getPADevice(char* dev, int input, PaStream** stream)
{
	int devnum = atoi(dev + 3);
	fprintf (stderr,"Using portaudio device %d.\n", devnum);

    PaError err;

	int numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 )
    {
        fprintf (stderr, "ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices );
        err = numDevices;
        goto error;
    }
	if( devnum >= numDevices)
    {
        fprintf (stderr, "ERROR: Requested device %d is larger than number of devices.\n", devnum );
        return(1);
    }

	const   PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo( devnum );

    /* print device name */
#ifdef WIN32
    {   /* Use wide char on windows, so we can show UTF-8 encoded device names */
        wchar_t wideName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, deviceInfo->name, -1, wideName, MAX_PATH-1);
        wprintf ( L"Name                        = %s\n", wideName );
    }
#else
    fprintf (stderr, "Name                        = %s\n", deviceInfo->name );
#endif
	if((input == 1) && (deviceInfo->maxInputChannels == 0))
	{
		fprintf (stderr, "ERROR: Requested device %d is not an input device.\n", devnum );
		return(1);
	}
	if((input == 0) && (deviceInfo->maxOutputChannels == 0))
	{
		fprintf (stderr, "ERROR: Requested device %d is not an output device.\n", devnum );
		return(1);
	}

	//Create stream parameters
	PaStreamParameters parameters;
    parameters.device = devnum;
    parameters.channelCount = 1;       /* mono */
    parameters.sampleFormat = paInt16; //Shorts
    parameters.suggestedLatency = (input == 1) ? PA_LATENCY_IN : PA_LATENCY_OUT;
    parameters.hostApiSpecificStreamInfo = NULL;

	//Open stream
	err = Pa_OpenStream(
            stream,
            (input == 1) ? &parameters : NULL,
            (input == 0) ? &parameters : NULL,
            (input == 1) ? SAMPLE_RATE_IN : SAMPLE_RATE_OUT,
            PA_FRAMES_PER_BUFFER,
            paClipOff,
            NULL /*callback*/,
            NULL );
    if( err != paNoError ) goto error;

	return 0;

error:
	  fprintf (stderr, "An error occured while initializing a portaudio stream\n" );
    fprintf (stderr, "Error number: %d\n", err );
    fprintf (stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
#endif

void
openAudioOutDevice (dsd_opts * opts, int speed)
{
  // get info of device/file
  /*
  if(strncmp(opts->audio_out_dev, "pulse", 5) == 0)
  {
    opts->audio_out_type == 0;
  }
  */
	if(strncmp(opts->audio_out_dev, "pa:", 3) == 0)
	{
		opts->audio_out_type = 2;
#ifdef USE_PORTAUDIO
		int err = getPADevice(opts->audio_out_dev, 0, &opts->audio_out_pa_stream);
		if(err != 0)
			exit(err);
#else
		fprintf (stderr,"Error, Portaudio support not compiled.\n");
		exit(1);
#endif
	}
  if(strncmp(opts->audio_in_dev, "pulse", 5) == 0)
  {
    opts->audio_in_type = 0;
  }
	else
	{
		struct stat stat_buf;
  if(stat(opts->audio_out_dev, &stat_buf) != 0 && strncmp(opts->audio_out_dev, "pulse", 5 != 0)) //HERE
    {
      fprintf (stderr,"Error, couldn't open %s\n", opts->audio_out_dev);
      exit(1);
    }

  if( (!(S_ISCHR(stat_buf.st_mode) || S_ISBLK(stat_buf.st_mode))) && strncmp(opts->audio_out_dev, "pulse", 5 != 0))
    {
      // this is not a device
      fprintf (stderr,"Error, %s is not a device. use -w filename for wav output.\n", opts->audio_out_dev);
      exit(1);
    }
/*
#ifdef SOLARIS
  sample_info_t aset, aget;

  opts->audio_out_fd = open (opts->audio_out_dev, O_WRONLY);
  if (opts->audio_out_fd == -1)
    {
      fprintf (stderr,"Error, couldn't open %s\n", opts->audio_out_dev);
      //exit (1);
    }

  // get current
  ioctl (opts->audio_out_fd, AUDIO_GETINFO, &aset);
  aset.record.sample_rate = speed;
  aset.play.sample_rate = speed;
  aset.record.channels = 1;
  aset.play.channels = 1;
  aset.record.precision = 16;
  aset.play.precision = 16;
  aset.record.encoding = AUDIO_ENCODING_LINEAR;
  aset.play.encoding = AUDIO_ENCODING_LINEAR;

  if (ioctl (opts->audio_out_fd, AUDIO_SETINFO, &aset) == -1)
    {
      fprintf (stderr,"Error setting sample device parameters\n");
      exit (1);
    }
#endif

#if defined(BSD) && !defined(__APPLE__)

  int fmt;

  opts->audio_out_fd = open (opts->audio_out_dev, O_WRONLY);
  if (opts->audio_out_fd == -1)
    {
      fprintf (stderr,"Error, couldn't open %s\n", opts->audio_out_dev);
      opts->audio_out = 0;
      //exit(1);
    }

  fmt = 0;
  if (ioctl (opts->audio_out_fd, SNDCTL_DSP_RESET) < 0)
    {
      fprintf (stderr,"ioctl reset error \n");
    }
  fmt = speed;
  if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SPEED, &fmt) < 0)
    {
      fprintf (stderr,"ioctl speed error \n");
    }
  fmt = 0;
  if (ioctl (opts->audio_out_fd, SNDCTL_DSP_STEREO, &fmt) < 0)
    {
      fprintf (stderr,"ioctl stereo error \n");
    }
  fmt = AFMT_S16_LE;
  if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
    {
      fprintf (stderr,"ioctl setfmt error \n");
    }

#endif
*/
	}
  fprintf (stderr,"Audio Out Device: %s\n", opts->audio_out_dev);
}

void
openAudioInDevice (dsd_opts * opts)
{
  // get info of device/file
	if(strncmp(opts->audio_in_dev, "-", 1) == 0)
	{
		opts->audio_in_type = 1;
		opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
		opts->audio_in_file_info->samplerate=48000;
    opts->pulse_digi_rate_out = 8000; //set out rate to 8000 for stdin input
		opts->audio_in_file_info->channels=1;
		opts->audio_in_file_info->seekable=0;
		opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
		opts->audio_in_file = sf_open_fd(fileno(stdin), SFM_READ, opts->audio_in_file_info, 0);

		if(opts->audio_in_file == NULL) {
			fprintf (stderr,"Error, couldn't open stdin with libsndfile: %s\n", sf_strerror(NULL));
			exit(1); //had this one disabled, re-enabling it now
		}
	}
	else if(strncmp(opts->audio_in_dev, "pa:", 2) == 0)
	{
		opts->audio_in_type = 2;
#ifdef USE_PORTAUDIO
		int err = getPADevice(opts->audio_in_dev, 1, &opts->audio_in_pa_stream);
		if(err != 0)
			exit(err);

		if (opts->split == 0)
		{
			int err = getPADevice(opts->audio_in_dev, 0, &opts->audio_out_pa_stream);
			if(err != 0)
				exit(err);
		}

#else
		fprintf (stderr,"Error, Portaudio support not compiled.\n");
		exit(1);
#endif
	}
  else if(strncmp(opts->audio_in_dev, "rtl", 3) == 0)
  {
    opts->audio_in_type = 3;
  }
  else if(strncmp(opts->audio_in_dev, "pulse", 5) == 0)
  {
    opts->audio_in_type = 0;
  }
	else
	{
  struct stat stat_buf;
  if (stat(opts->audio_in_dev, &stat_buf) != 0)
    {
      fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
      exit(1);
    }
  if (S_ISREG(stat_buf.st_mode))
    {
      // is this a regular file? then process with libsndfile.
      //opts->pulse_digi_rate_out = 8000; //this for wav files input?
      opts->audio_in_type = 1;
      opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
      opts->audio_in_file_info->channels = 1;
      opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);

      if(opts->audio_in_file == NULL)
        {
          fprintf (stderr,"Error, couldn't open file %s\n", opts->audio_in_dev);
          exit(1);
        }
    }
  else
    {
      // this is a device, use old handling, pulse audio now
      opts->audio_in_type = 0;
/*
#ifdef SOLARIS
    sample_info_t aset, aget;
    int rgain;

    rgain = 64;

    if (opts->split == 1)
      {
        opts->audio_in_fd = open (opts->audio_in_dev, O_RDONLY);
      }
    else
      {
        opts->audio_in_fd = open (opts->audio_in_dev, O_RDWR);
      }
    if (opts->audio_in_fd == -1)
      {
        fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
        //exit(1);
      }

    // get current
    ioctl (opts->audio_in_fd, AUDIO_GETINFO, &aset);

    aset.record.sample_rate = SAMPLE_RATE_IN;
    aset.play.sample_rate = SAMPLE_RATE_IN;
    aset.record.channels = 1;
    aset.play.channels = 1;
    aset.record.precision = 16;
    aset.play.precision = 16;
    aset.record.encoding = AUDIO_ENCODING_LINEAR;
    aset.play.encoding = AUDIO_ENCODING_LINEAR;
    aset.record.port = AUDIO_LINE_IN;
    aset.record.gain = rgain;

    if (ioctl (opts->audio_in_fd, AUDIO_SETINFO, &aset) == -1)
      {
        fprintf (stderr,"Error setting sample device parameters\n");
        exit (1);
      }
#endif
*/
/*
#if defined(BSD) && !defined(__APPLE__)
    int fmt;

    if (opts->split == 1)
      {
        opts->audio_in_fd = open (opts->audio_in_dev, O_RDONLY);
      }
    else
      {
        opts->audio_in_fd = open (opts->audio_in_dev, O_RDWR);
      }

    if (opts->audio_in_fd == -1)
      {
        fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
        opts->audio_out = 0;
      }

    fmt = 0;
    if (ioctl (opts->audio_in_fd, SNDCTL_DSP_RESET) < 0)
      {
        fprintf (stderr,"ioctl reset error \n");
      }
    fmt = SAMPLE_RATE_IN;
    if (ioctl (opts->audio_in_fd, SNDCTL_DSP_SPEED, &fmt) < 0)
      {
        fprintf (stderr,"ioctl speed error \n");
      }
    fmt = 0;
    if (ioctl (opts->audio_in_fd, SNDCTL_DSP_STEREO, &fmt) < 0)
      {
        fprintf (stderr,"ioctl stereo error \n");
      }
    fmt = AFMT_S16_LE;
    if (ioctl (opts->audio_in_fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
      {
        fprintf (stderr,"ioctl setfmt error \n");
      }
#endif
*/
  }
  }

  if (opts->split == 1)
    {
      fprintf (stderr,"Audio In Device: %s\n", opts->audio_in_dev);
    }
  else
    {
      fprintf (stderr,"Audio In/Out Device: %s\n", opts->audio_in_dev);
    }
}
