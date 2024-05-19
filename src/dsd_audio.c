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
pa_sample_spec ff; //float

//pulse audio buffer attributes (input latency)
pa_buffer_attr lt;

void closePulseOutput (dsd_opts * opts)
{
  pa_simple_free (opts->pulse_digi_dev_out);
  if (opts->frame_provoice == 1 || opts->monitor_input_audio == 1) //EDACS analog calls and/or monitoring source analog audio
    pa_simple_free (opts->pulse_raw_dev_out);
}

void closePulseInput (dsd_opts * opts)
{
  pa_simple_free (opts->pulse_digi_dev_in);
}

void openPulseOutput(dsd_opts * opts)
{

  char * dev = NULL;
  if (opts->pa_output_idx[0] != 0)
    dev = opts->pa_output_idx;

  int err = 0;

  ss.format = PA_SAMPLE_S16NE;
  ss.channels = opts->pulse_raw_out_channels;
  ss.rate = opts->pulse_raw_rate_out;

  tt.format = PA_SAMPLE_S16NE;
  tt.channels = opts->pulse_digi_out_channels;
  tt.rate = opts->pulse_digi_rate_out;

  ff.format = PA_SAMPLE_FLOAT32NE;
  ff.channels = opts->pulse_digi_out_channels;
  ff.rate = opts->pulse_digi_rate_out;

  //reconfigured to open when using edacs or raw analog monitor so we can have a analog audio out that runs at 48k1 and not 8k1 float/short
  if (opts->frame_provoice == 1 || opts->monitor_input_audio == 1)
    opts->pulse_raw_dev_out  = pa_simple_new(NULL, "DSD-FME3", PA_STREAM_PLAYBACK, dev, "Analog", &ss, 0, NULL, &err);

  if (err != 0)
  {
    fprintf (stderr, "Err: %d; %s; ", err, pa_strerror(err));
    #ifdef __CYGWIN__
    fprintf (stderr, "Please make sure the Pulse Audio Server Backend is running first.");
    #endif
    exit(0);
  }

  pa_channel_map* fl = 0; //NULL and 0 are same in this context
  pa_channel_map* ss = 0; //NULL and 0 are same in this context

  if (opts->floating_point == 0)
  {
    opts->pulse_digi_dev_out = pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, dev, opts->output_name, &tt, ss, NULL, &err);

    if (err != 0)
    {
      fprintf (stderr, "Err: %d; %s; ", err, pa_strerror(err));
      #ifdef __CYGWIN__
      fprintf (stderr, "Please make sure the Pulse Audio Server Backend is running first.");
      #endif
      exit(0);
    }
  }

  if (opts->floating_point == 1)
  {
    opts->pulse_digi_dev_out = pa_simple_new(NULL, "DSD-FME", PA_STREAM_PLAYBACK, dev, opts->output_name, &ff, fl, NULL, &err);

    if (err != 0)
    {
      fprintf (stderr, "Err: %d; %s; ", err, pa_strerror(err));
      #ifdef __CYGWIN__
      fprintf (stderr, "Please make sure the Pulse Audio Server Backend is running first.");
      #endif
      exit(0);
    }
  }

}

void openPulseInput(dsd_opts * opts)
{

  char * dev = NULL;
  if (opts->pa_input_idx[0] != 0)
    dev = opts->pa_input_idx;

  int err = 0;

  cc.format = PA_SAMPLE_S16NE;
  cc.channels = opts->pulse_digi_in_channels;
  cc.rate = opts->pulse_digi_rate_in; //48000

  //adjust input latency settings (defaults are all -1 to let server set these automatically, but without pavucontrol open, this is approx 2s)
  //setting fragsize to 960 for 48000 input seems to do the trick to allow a much faster latency without underrun, may need adjusting if
  //modifying the input rate (will only apply IF using pulse audio input, and not any other input method)

  //TODO: set fragsize vs expected input rate if required in the future
  //NOTE: If users report any underrun conditions, then either change 960 back to -1, or pass NULL instead of &lt

  //https://freedesktop.org/software/pulseaudio/doxygen/structpa__buffer__attr.html

  //for now, only going to modify the fragsize if using the encoder, else, let the pa server set it automatically
  // if (opts->m17encoder == 1)
  //   lt.fragsize = 960*5;
  // else lt.fragsize = -1;

  //test doing it universally, fall back to above if needed
  lt.fragsize = 960*5;
  lt.maxlength = -1;
  lt.prebuf = -1;
  lt.tlength = -1;
  if (opts->m17encoder == 1)
    opts->pulse_digi_dev_in  = pa_simple_new(NULL, "DSD-FME4", PA_STREAM_RECORD, dev, "M17 Voice Input", &cc, NULL, &lt, &err);
  else opts->pulse_digi_dev_in  = pa_simple_new(NULL, "DSD-FME", PA_STREAM_RECORD, dev, opts->output_name, &cc, NULL, &lt, &err);

  if (err != 0)
  {
    fprintf (stderr, "Err: %d; %s; ", err, pa_strerror(err));
    #ifdef __CYGWIN__
    fprintf (stderr, "Please make sure the Pulse Audio Server Backend is running first.");
    #endif
    exit(0);
  }

  //debug
  // if (opts->m17encoder == 1)
  // {
  //   unsigned long long int latency = pa_simple_get_latency (opts->pulse_digi_dev_in, NULL);
  //   fprintf (stderr, "Pulse Audio Input Latency: %05lld;", latency);
  // }

}

void parse_pulse_input_string  (dsd_opts * opts, char * input)
{
  char * curr;
  curr = strtok(input, ":");
  if (curr != NULL)
  {
    strncpy (opts->pa_input_idx, curr, 99);
    opts->pa_input_idx[99] = 0;
    fprintf (stderr, "\n");
    fprintf (stderr, "Pulse Input Device: %s; ", opts->pa_input_idx);
    fprintf (stderr, "\n");
  }
}

void parse_pulse_output_string (dsd_opts * opts, char * input)
{
  char * curr;
  curr = strtok(input, ":");
  if (curr != NULL)
  {
    strncpy (opts->pa_output_idx, curr, 99);
    opts->pa_output_idx[99] = 0;
    fprintf (stderr, "\n");
    fprintf (stderr, "Pulse Output Device: %s; ", opts->pa_output_idx);
    fprintf (stderr, "\n");
  }
}

void openOSSOutput (dsd_opts * opts)
{
  int fmt; 
  int speed = 48000;
  if (opts->audio_in_type == 5) //if((strncmp(opts->audio_in_dev, "/dev/dsp", 8) == 0)) or 'split' == 0
  {

    if((strncmp(opts->audio_out_dev, "/dev/dsp", 8) == 0))
    {
      fprintf (stderr, "OSS Output %s.\n", opts->audio_out_dev);
      opts->audio_out_fd = open (opts->audio_out_dev, O_RDWR);
      if (opts->audio_out_fd == -1)
      {
        fprintf (stderr, "Error, couldn't open #1 %s\n", opts->audio_out_dev);
        opts->audio_out = 0;
        exit(1);
      }

      fmt = 0;
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_RESET) < 0)
      {
        fprintf (stderr, "ioctl reset error \n");
      }

      fmt = speed;
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SPEED, &fmt) < 0)
      {
        fprintf (stderr, "ioctl speed error \n");
      }

      fmt = 0; //this seems okay to be 1 or 0, not sure what the difference really is (works in stereo on 0)
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_STEREO, &fmt) < 0)
      {
        fprintf (stderr, "ioctl stereo error \n");
      }

      fmt = AFMT_S16_LE;
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
      {
        fprintf (stderr, "ioctl setfmt error \n");
      }

      opts->audio_out_type = 5; //5 for 1 channel - 48k OSS 16-bit short output (matching with input) 
      opts->pulse_digi_rate_out = 48000; //this is used to force to upsample and also allow source audio monitor conditional check
      opts->pulse_digi_out_channels = 1; //this is used to allow source audio monitor conditional check
      opts->audio_gain = 0;
    }
  }

  if (opts->audio_in_type != 5) //split == 1
  {

    if((strncmp(opts->audio_out_dev, "/dev/dsp", 8) == 0))
    {
      fprintf (stderr, "OSS Output %s.\n", opts->audio_out_dev);
      opts->audio_out_fd = open (opts->audio_out_dev, O_WRONLY);
      if (opts->audio_out_fd == -1)
      {
        fprintf (stderr, "Error, couldn't open %s\n", opts->audio_out_dev);
        opts->audio_out = 0;
        exit(1);
      }

      //Setup the device. Note that it's important to set the sample format, number of channels and sample rate exactly in this order. Some devices depend on the order. 

      fmt = 0;
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_RESET) < 0)
      {
        fprintf (stderr, "ioctl reset error \n");
      }

      fmt = AFMT_S16_LE; //Sample Format
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
      {
        fprintf (stderr, "ioctl setfmt error \n");
      }

      fmt = opts->pulse_digi_out_channels; //number of channels //was 2
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_CHANNELS, &fmt) < 0)
      {
        fprintf (stderr, "ioctl channel error \n");
      }

      //if using split with OSS output, and using EDACS w/ Analog, we have to use 48k
      if (opts->frame_provoice == 1)
        opts->pulse_digi_rate_out = 48000;

      speed = opts->pulse_digi_rate_out; //since we have split input/output, we want to mirror pulse rate out
      fmt = speed; //output rate
      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_SPEED, &fmt) < 0)
      {
        fprintf (stderr, "ioctl speed error \n");
      }
      if (opts->pulse_digi_out_channels == 2)
        fmt = 1;
      else fmt = 0; 

      if (ioctl (opts->audio_out_fd, SNDCTL_DSP_STEREO, &fmt) < 0)
      {
        fprintf (stderr, "ioctl stereo error \n");
      }

      //TODO: Multiple output returns based on 8k/1, 8k/2, or maybe 48k/1? (2,3,5)??
      if (opts->pulse_digi_out_channels == 2)
        opts->audio_out_type = 2; //2 for 2 channel 8k OSS 16-bit short output
      else if (opts->frame_m17 == 1)
        opts->audio_out_type = 2;
      else opts->audio_out_type = 5;

      //debug 
      fprintf (stderr, "Using OSS Output with %dk/%d channel configuration.\n", opts->pulse_digi_rate_out, opts->pulse_digi_out_channels);
    }
  }
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
  //we only want to upsample when using sample rates greater than 8k for output
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
          //tap the pointer here and store the short upsample buffer samples
          state->s_lu[n] = (short) *state->audio_out_float_buf_p;
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
          //tap the pointer here and store the short buffer samples
          state->s_l[n] = (short) *state->audio_out_temp_buf_p;
          //debug
          // fprintf (stderr, " %d", state->s_l[n]);
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
          //tap the pointer here and store the short upsample buffer samples
          state->s_ru[n] = (short) *state->audio_out_float_buf_pR;
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
          //tap the pointer here and store the short buffer samples
          state->s_r[n] = (short) *state->audio_out_temp_buf_pR;
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
  UNUSED(state);

  //short aout_buf[160];
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
  UNUSED(result);

  //don't synthesize voice if slot is turned off
  if (opts->slot1_on == 0)
  {
    //clear any previously buffered audio
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
    state->audio_out_idx = 0;
    goto end_psv;
  } 

  if (state->audio_out_idx > opts->delay)
  {
    if (opts->audio_out_type == 5 || opts->audio_out_type == 1) //OSS
    {
      //OSS 48k/1
      result = write (opts->audio_out_fd, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2));
      state->audio_out_idx = 0;
    }
		else if (opts->audio_out_type == 0)
    {
      pa_simple_write(opts->pulse_digi_dev_out, (state->audio_out_buf_p - state->audio_out_idx), (state->audio_out_idx * 2), NULL); 
      state->audio_out_idx = 0;
    }
    else if (opts->audio_out_type == 8) //UDP Audio Out -- Forgot some things still use this for now
    {
      udp_socket_blaster (opts, state, (state->audio_out_idx * 2), (state->audio_out_buf_p - state->audio_out_idx));
      state->audio_out_idx = 0;
    }
    else state->audio_out_idx = 0; //failsafe for audio_out == 0


  }

  end_psv:

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
  UNUSED(result);

  if (state->audio_out_idxR > opts->delay)
  {
    // output synthesized speech to sound card
		if (opts->audio_out_type == 5) //OSS
    {
      //OSS 48k/1
      result = write (opts->audio_out_fd, (state->audio_out_buf_pR - state->audio_out_idxR), (state->audio_out_idxR * 2));
      state->audio_out_idxR = 0;
    }
		else if (opts->audio_out_type == 0)
    {
      pa_simple_write(opts->pulse_digi_dev_outR, (state->audio_out_buf_pR - state->audio_out_idxR), (state->audio_out_idxR * 2), NULL); 
      state->audio_out_idxR = 0;
    }
    else if (opts->audio_out_type == 8) //UDP Audio Out -- Not sure how this would handle, but R never gets called anymore, so just here for symmetry
    {
      udp_socket_blaster (opts, state, (state->audio_out_idxR * 2), (state->audio_out_buf_pR - state->audio_out_idxR));
      state->audio_out_idxR = 0;
    }
    else state->audio_out_idxR = 0; //failsafe for audio_out == 0

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
  UNUSED(speed);

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
  // struct stat stat_buf;
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
  char * extension;
  const char ch = '.';
  extension = strrchr(opts->audio_in_dev, ch); //return extension if this is a .wav or .bin file

  //if no extension set, give default of .wav -- bugfix for github issue #105
  // if (extension == NULL) extension = ".wav";

  // get info of device/file
	if (strncmp(opts->audio_in_dev, "-", 1) == 0)
	{
    opts->audio_in_type = 1;
    opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
    opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
    opts->audio_in_file_info->channels=1;
    opts->audio_in_file_info->seekable=0;
    opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
    opts->audio_in_file = sf_open_fd(fileno(stdin), SFM_READ, opts->audio_in_file_info, 0);

    if(opts->audio_in_file == NULL)
    {
      fprintf(stderr, "Error, couldn't open stdin with libsndfile: %s\n", sf_strerror(NULL));
      exit(1);
    }
	}

  else if (strncmp(opts->audio_in_dev, "m17udp", 6) == 0)
  {
    opts->audio_in_type = 9; //NULL audio device
  }

  else if (strncmp(opts->audio_in_dev, "tcp", 3) == 0)
  {
    opts->audio_in_type = 8;
    opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
    opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
    opts->audio_in_file_info->channels=1;
    opts->audio_in_file_info->seekable=0;
    opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
    opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);
    if(opts->tcp_file_in == NULL)
    {
      fprintf(stderr, "Error, couldn't open TCP with libsndfile: %s\n", sf_strerror(NULL));
      exit(1);
    }
  }

  // else if (strncmp(opts->audio_in_dev, "udp", 3) == 0)
  // {
  //   opts->audio_in_type = 6;
  //   opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
  //   opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
  //   opts->audio_in_file_info->channels=1;
  //   opts->audio_in_file_info->seekable=0;
  //   opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
  //   opts->udp_file_in = sf_open_fd(opts->udp_sockfd, SFM_READ, opts->audio_in_file_info, 0);

  //   if(opts->udp_file_in == NULL)
  //   {
  //     fprintf(stderr, "Error, couldn't open UDP with libsndfile: %s\n", sf_strerror(NULL));
  //     exit(1);
  //   }
  // }

  else if(strncmp(opts->audio_in_dev, "rtl", 3) == 0)
  {
    #ifdef USE_RTLSDR
    opts->audio_in_type = 3;
    #elif AERO_BUILD
    opts->audio_in_type = 5;
    sprintf (opts->audio_in_dev, "/dev/dsp");
    #else
    opts->audio_in_type = 0;
    sprintf (opts->audio_in_dev, "pulse");
    #endif
  }
  else if(strncmp(opts->audio_in_dev, "pulse", 5) == 0)
  {
    opts->audio_in_type = 0;
  }
  else if((strncmp(opts->audio_in_dev, "/dev/dsp", 8) == 0))
  {
    opts->audio_in_type = 5;
  }

  //if no extension set, treat as named pipe or extensionless wav file -- bugfix for github issue #105
  else if (extension == NULL)
  {
    opts->audio_in_type = 2;
    opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
    opts->audio_in_file_info->samplerate = opts->wav_sample_rate;
    opts->audio_in_file_info->channels = 1;
    opts->audio_in_file_info->seekable = 0;
    opts->audio_in_file_info->format = SF_FORMAT_RAW | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
    opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);

    if (opts->audio_in_file == NULL)
    {
      fprintf(stderr, "Error, couldn't open file/pipe with libsndfile: %s\n", sf_strerror(NULL));
      exit(1);
    }
  }

  //test .rrc files with hardset wav file settings
  else if (strncmp(extension, ".rrc", 3) == 0)
	{
    //debug
    fprintf (stderr, "Opening M17 .rrc headless wav file\n");
    
    opts->audio_in_type = 2;
    opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
    opts->audio_in_file_info->samplerate = 48000;
    opts->audio_in_file_info->channels = 1;
    opts->audio_in_file_info->seekable = 0;
    opts->audio_in_file_info->format = SF_FORMAT_RAW | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
    opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);

    if(opts->audio_in_file == NULL)
    {
      fprintf(stderr, "Error, couldn't open %s with libsndfile: %s\n", opts->audio_in_dev, sf_strerror(NULL));
      exit(1);
    }
	}

  //TODO: test .sym files as symbol capture .bin files
  else if (strncmp(extension, ".sym", 3) == 0)
	{
    struct stat stat_buf;
    if (stat(opts->audio_in_dev, &stat_buf) != 0)
    {
      fprintf (stderr,"Error, couldn't open bin file %s\n", opts->audio_in_dev);
      exit(1);
    }
    if (S_ISREG(stat_buf.st_mode))
    {
      opts->symbolfile = fopen(opts->audio_in_dev, "r");
      opts->audio_in_type = 4; //symbol capture bin files
    }
    else
    {
      opts->audio_in_type = 0;
    }
  }

  else if (strncmp(extension, ".bin", 3) == 0)
	{
    struct stat stat_buf;
    if (stat(opts->audio_in_dev, &stat_buf) != 0)
    {
      fprintf (stderr,"Error, couldn't open bin file %s\n", opts->audio_in_dev);
      exit(1);
    }
    if (S_ISREG(stat_buf.st_mode))
    {
      opts->symbolfile = fopen(opts->audio_in_dev, "r");
      opts->audio_in_type = 4; //symbol capture bin files
    }
    else
    {
      opts->audio_in_type = 0;
    }
  }
  //open as wav file as last resort, wav files subseptible to sample rate issues if not 48000
	else
	{
    struct stat stat_buf;
    if (stat(opts->audio_in_dev, &stat_buf) != 0)
    {
      fprintf (stderr,"Error, couldn't open wav file %s\n", opts->audio_in_dev);
      exit(1);
    }
    if (S_ISREG(stat_buf.st_mode))
    {
      opts->audio_in_type = 2; //two now, seperating STDIN and wav files
      opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
      opts->audio_in_file_info->samplerate=opts->wav_sample_rate; 
      opts->audio_in_file_info->channels=1; 
      opts->audio_in_file_info->channels = 1;
      opts->audio_in_file_info->seekable=0;
      opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
      //
      opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);

      if(opts->audio_in_file == NULL)
      {
        fprintf(stderr, "Error, couldn't open wav file %s\n", opts->audio_in_dev);
        exit(1);
      }

    }
    //open pulse audio if no bin or wav
    else //seems this condition is never met
    {
      //opts->audio_in_type = 5; //not sure if this works or needs to openPulse here
      fprintf(stderr, "Error, couldn't open input file.\n");
      exit(1);
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
