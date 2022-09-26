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

void closePulseOutput (dsd_opts * opts)
{
  pa_simple_free (opts->pulse_digi_dev_out);
  if (opts->dmr_stereo == 1)
  {
    pa_simple_free (opts->pulse_digi_dev_outR);
  }
}

void closePulseInput (dsd_opts * opts)
{
  pa_simple_free (opts->pulse_digi_dev_in);
}

void openPulseOutput(dsd_opts * opts)
{

  ss.format = PA_SAMPLE_S16NE;
  ss.channels = opts->pulse_raw_out_channels; //doing tests with 2 channels at 22050 for 44100 audio default in pulse
  ss.rate = opts->pulse_raw_rate_out; //48000

  tt.format = PA_SAMPLE_S16NE;
  tt.channels = opts->pulse_digi_out_channels; //doing tests with 2 channels at 22050 for 44100 audio default in pulse
  tt.rate = opts->pulse_digi_rate_out; //48000, switches to 8000 when using RTL dongle

  if (opts->monitor_input_audio == 1)
  {
    opts->pulse_raw_dev_out  = pa_simple_new(NULL, "DSD-FME3", PA_STREAM_PLAYBACK, NULL, "Raw Audio Out", &ss, NULL, NULL, NULL);
  }

//tt
  pa_channel_map* left = 0; //NULL and 0 are same in this context
  pa_channel_map* right = 0; //NULL and 0 are same in this context

  if (opts->dmr_stereo == 0)
  {
    opts->pulse_digi_dev_out = pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, NULL, opts->output_name, &tt, left, NULL, NULL);

  }

  if (opts->dmr_stereo == 1)
  {
    opts->pulse_digi_dev_out  = pa_simple_new(NULL, "DSD-FME1", PA_STREAM_PLAYBACK, NULL, "XDMA SLOT 1", &tt, left, NULL, NULL);
    opts->pulse_digi_dev_outR = pa_simple_new(NULL, "DSD-FME2", PA_STREAM_PLAYBACK, NULL, "XDMA SLOT 2", &tt, right, NULL, NULL);
  }

}

void openPulseInput(dsd_opts * opts)
{

  cc.format = PA_SAMPLE_S16NE;
  cc.channels = opts->pulse_digi_in_channels;
  cc.rate = opts->pulse_digi_rate_in; //48000

  opts->pulse_digi_dev_in  = pa_simple_new(NULL, "DSD-FME", PA_STREAM_RECORD, NULL, opts->output_name, &cc, NULL, NULL, NULL);

}

void
processAudio (dsd_opts * opts, dsd_state * state)
{

  int i, n;
  float aout_abs, max, gainfactor, gaindelta, maxbuf;

  if (opts->audio_gain == (float) 0)
    {
      // detect max level
      max = 0;
      //attempt to reduce crackle by overriding the max value when not using pulse input
      if (opts->audio_in_type != 0)
      {
        max = 3000;
      }
      state->audio_out_temp_buf_p = state->audio_out_temp_buf;
      for (n = 0; n < 160; n++)
        {
          aout_abs = fabsf (*state->audio_out_temp_buf_p);
          if (aout_abs > max)
            {
              max = aout_abs;
            }
          state->audio_out_temp_buf_p++;
        }
      *state->aout_max_buf_p = max;

      state->aout_max_buf_p++;

      state->aout_max_buf_idx++;

      if (state->aout_max_buf_idx > 24)
        {
          state->aout_max_buf_idx = 0;
          state->aout_max_buf_p = state->aout_max_buf;
        }

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
  //we only want to upsample when using sample rates greater than 8k for output,
  //hard set to 8k for RTL mono and MBE playback, otherwise crackling may occur.
  if (opts->pulse_digi_rate_out > 8000)
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
processAudioR (dsd_opts * opts, dsd_state * state)
{

  int i, n;
  float aout_abs, max, gainfactor, gaindelta, maxbuf;
  if (opts->audio_gainR == (float) 0)
    {
      // detect max level
      max = 0;
      //attempt to reduce crackle by overriding the max value when not using pulse input
      if (opts->audio_in_type != 0)
      {
        max = 3000;
      }
      state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
      for (n = 0; n < 160; n++)
        {
          aout_abs = fabsf (*state->audio_out_temp_buf_pR);
          if (aout_abs > max)
            {
              max = aout_abs;
            }
          state->audio_out_temp_buf_pR++;
        }
      *state->aout_max_buf_pR = max;

      state->aout_max_buf_pR++;

      state->aout_max_buf_idxR++;

      if (state->aout_max_buf_idxR > 24)
        {
          state->aout_max_buf_idxR = 0;
          state->aout_max_buf_pR = state->aout_max_bufR;
        }

      // lookup max history
      for (i = 0; i < 25; i++)
        {
          maxbuf = state->aout_max_bufR[i];
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
      if (gainfactor < state->aout_gainR)
        {
          state->aout_gainR = gainfactor;
          gaindelta = (float) 0;
        }
      else
        {
          if (gainfactor > (float) 50)
            {
              gainfactor = (float) 50;
            }
          gaindelta = gainfactor - state->aout_gainR;
          if (gaindelta > ((float) 0.05 * state->aout_gainR))
            {
              gaindelta = ((float) 0.05 * state->aout_gainR);
            }
        }
      gaindelta /= (float) 160;
    }
  else
    {
      gaindelta = (float) 0;
    }

  if(opts->audio_gainR >= 0)
    {
      // adjust output gain
      state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
      for (n = 0; n < 160; n++)
        {
          *state->audio_out_temp_buf_pR = (state->aout_gainR + ((float) n * gaindelta)) * (*state->audio_out_temp_buf_pR);
          state->audio_out_temp_buf_pR++;
        }
      state->aout_gainR += ((float) 160 * gaindelta);
    }

  // copy audio data to output buffer and upsample if necessary
  state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
  //we only want to upsample when using sample rates greater than 8k for output,
  //hard set to 8k for RTL mono and MBE playback, otherwise crackling may occur.
  if (opts->pulse_digi_rate_out > 8000)
    {
      for (n = 0; n < 160; n++)
        {
          upsample (state, *state->audio_out_temp_buf_pR);
          state->audio_out_temp_buf_pR++;
          state->audio_out_float_buf_pR += 6;
          state->audio_out_idxR += 6;
          state->audio_out_idx2R += 6;
        }
      state->audio_out_float_buf_pR -= (960 + opts->playoffsetR);
      // copy to output (short) buffer
      for (n = 0; n < 960; n++)
        {
          if (*state->audio_out_float_buf_pR >  32767.0F)
            {
              *state->audio_out_float_buf_pR = 32767.0F;
            }
          else if (*state->audio_out_float_buf_pR < -32768.0F)
            {
              *state->audio_out_float_buf_pR = -32768.0F;
            }
          *state->audio_out_buf_pR = (short) *state->audio_out_float_buf_pR;
          state->audio_out_buf_pR++;
          state->audio_out_float_buf_pR++;
        }
      state->audio_out_float_buf_pR += opts->playoffsetR;
    }
  else
    {
      for (n = 0; n < 160; n++)
        {
          if (*state->audio_out_temp_buf_pR > 32767.0F)
            {
              *state->audio_out_temp_buf_pR = 32767.0F;
            }
          else if (*state->audio_out_temp_buf_pR < -32768.0F)
            {
              *state->audio_out_temp_buf_pR = -32768.0F;
            }
          *state->audio_out_buf_pR = (short) *state->audio_out_temp_buf_pR;
          state->audio_out_buf_pR++;
          state->audio_out_temp_buf_pR++;
          state->audio_out_idxR++;
          state->audio_out_idx2R++;
        }
    }
}

void writeSynthesizedVoice (dsd_opts * opts, dsd_state * state)
{
  int n;
  short aout_buf[160];
  short *aout_buf_p;

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

}

void writeSynthesizedVoiceR (dsd_opts * opts, dsd_state * state)
{
  int n;
  short aout_buf[160];
  short *aout_buf_p;

  aout_buf_p = aout_buf;
  state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;

  for (n = 0; n < 160; n++)
  {
    if (*state->audio_out_temp_buf_pR > (float) 32767)
      {
        *state->audio_out_temp_buf_pR = (float) 32767;
      }
    else if (*state->audio_out_temp_buf_pR < (float) -32768)
      {
        *state->audio_out_temp_buf_pR = (float) -32768;
      }
      *aout_buf_p = (short) *state->audio_out_temp_buf_pR;
      aout_buf_p++;
      state->audio_out_temp_buf_pR++;
  }

  sf_write_short(opts->wav_out_fR, aout_buf, 160);

}

void writeRawSample (dsd_opts * opts, dsd_state * state, short sample)
{
  int n;
  short aout_buf[160];
  short *aout_buf_p;

  //sf_write_short(opts->wav_out_raw, aout_buf, 160);

  //only write if actual audio, truncate silence
  if (sample != 0)
  {
    sf_write_short(opts->wav_out_raw, &sample, 2); //2 to match pulseaudio input sample read
  }

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
      //go F yourself PA
		}
		else
    {
      //Test just sending it straight on since I think I've figured out the STDIN and RTL for Stereo
      pa_simple_write(opts->pulse_digi_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); //Yay! It works.
      state->audio_out_idx = 0;
    }


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

void
playSynthesizedVoiceR (dsd_opts * opts, dsd_state * state)
{
  ssize_t result;

  if (state->audio_out_idxR > opts->delay)
  {
    // output synthesized speech to sound card
		if(opts->audio_out_type == 2)
		{
      //go F yourself PA
		}
		else
    {
      pa_simple_write(opts->pulse_digi_dev_outR, (state->audio_out_buf_pR - state->audio_out_idxR), (state->audio_out_idxR * 2), NULL); //Yay! It works.
      state->audio_out_idxR = 0;
    }

  }

  if (state->audio_out_idx2R >= 800000)
  {
    state->audio_out_float_buf_pR = state->audio_out_float_bufR + 100;
    state->audio_out_buf_pR = state->audio_out_bufR + 100;
    memset (state->audio_out_float_bufR, 0, 100 * sizeof (float));
    memset (state->audio_out_bufR, 0, 100 * sizeof (short));
    state->audio_out_idx2R = 0;
  }
}

void
openAudioOutDevice (dsd_opts * opts, int speed)
{
  //converted to handle any calls to use portaudio
	if(strncmp(opts->audio_out_dev, "pa:", 3) == 0)
	{
		opts->audio_out_type = 0;
    fprintf (stderr,"Error, Port Audio is not supported by FME!\n");
    fprintf (stderr,"Using Pulse Audio Output Stream Instead! \n");
    sprintf (opts->audio_out_dev, "pulse");
	}
  if(strncmp(opts->audio_in_dev, "pulse", 5) == 0)
  {
    opts->audio_in_type = 0;
  }
	else
	{
		struct stat stat_buf;
  // if(stat(opts->audio_out_dev, &stat_buf) != 0 && strncmp(opts->audio_out_dev, "pulse", 5 != 0)) //HERE
  //   {
  //     fprintf (stderr,"Error, couldn't open %s\n", opts->audio_out_dev);
  //     exit(1);
  //   }

  // if( (!(S_ISCHR(stat_buf.st_mode) || S_ISBLK(stat_buf.st_mode))) && strncmp(opts->audio_out_dev, "pulse", 5 != 0))
  //   {
  //     // this is not a device
  //     fprintf (stderr,"Error, %s is not a device. use -w filename for wav output.\n", opts->audio_out_dev);
  //     exit(1);
  //   }
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
  //converted to handle any calls to use portaudio
	else if(strncmp(opts->audio_in_dev, "pa:", 2) == 0)
	{
		opts->audio_in_type = 0;
    fprintf (stderr,"Error, Port Audio is not supported by FME!\n");
    fprintf (stderr,"Using Pulse Audio Input Stream Instead! \n");
    sprintf (opts->audio_in_dev, "pulse");
	}
  else if(strncmp(opts->audio_in_dev, "rtl", 3) == 0)
  {
    opts->audio_in_type = 3;
  }
  else if(strncmp(opts->audio_in_dev, "pulse", 5) == 0)
  {
    opts->audio_in_type = 0;
  }
  //convert from opening wav files to OP25 symbol capture files since opening wav files is busted
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
      // opts->audio_in_type = 1;
      // opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
      // opts->audio_in_file_info->channels = 1;
      // opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);
      //
      // if(opts->audio_in_file == NULL)
      //   {
      //     fprintf (stderr,"Error, couldn't open file %s\n", opts->audio_in_dev);
      //     exit(1);
      //   }

      opts->symbolfile = fopen(opts->audio_in_dev, "r");
      opts->audio_in_type = 4; //symbol capture bin files
    }
  else
    {
      // this is a device, use old handling, pulse audio now
      opts->audio_in_type = 0;

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
