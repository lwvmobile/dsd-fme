#include "dsd.h"

void agf (dsd_opts * opts, dsd_state * state, float samp[160])
{
  int i, run;
  run = 1;
  float empty[160];
  memset (empty, 0.1f, sizeof(empty));

  if(opts->pulse_digi_out_channels == 2){}; //compiler warning garbo

  float max = 0.0f;
  float mmax = 0.75f;
  float mmin = -0.75f;
  float df; //decimation value
  df = (384.0f / (float)opts->pulse_digi_out_channels) * (50.0f - state->aout_gain);

  //this comparison is to determine whether or not to run gain on 'empty' samples (2v last 2, silent frames, etc)
  if (memcmp(empty, samp, sizeof(empty)) == 0) run = 0;
  if (run == 0) goto AGF_END;

  for (i = 0; i < 160; i++)
  {
    samp[i] = samp[i] / df;

    //simple clipping
    if (samp[i] > mmax)
      samp[i] = mmax;
    if (samp[i] < mmin)
      samp[i] = mmin; //this could have been an issue before "-mmin"?

    //max value
    if (fabs(samp[i]) > max)
      max = samp[i];

  }

  //crude auto gain for float values
  if (max < 0.351f && max > 0.1f && state->aout_gain < 42) state->aout_gain += 0.33f; //state->aout_gain++;
  if (max > 0.35f && state->aout_gain > 1) state->aout_gain -= 0.33f; //state->aout_gain--;

  AGF_END: ; //do nothing

}

//float stereo mix 3v2 DMR
void playSynthesizedVoiceFS3 (dsd_opts * opts, dsd_state * state)
{

  //NOTE: This runs once for every two timeslots, if we are in the BS voice loop
  //it doesn't matter if both slots have voice, or if one does, the slot without voice
  //will play silence while this runs if no voice present

  int i;
  uint8_t encL, encR;
  float stereo_samp1[320]; //8k 2-channel stereo interleave mix
  float stereo_samp2[320]; //8k 2-channel stereo interleave mix
  float stereo_samp3[320]; //8k 2-channel stereo interleave mix

  //note, always set a stereo float mix to a baseline value and not zero!
  memset (stereo_samp1, 0.1f, sizeof(stereo_samp1));
  memset (stereo_samp2, 0.1f, sizeof(stereo_samp2));
  memset (stereo_samp3, 0.1f, sizeof(stereo_samp3));

  //dmr enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 0;
  encL = (state->dmr_so  >> 6) & 0x1;
  encR = (state->dmr_soR >> 6) & 0x1;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encL = 0;
      }
    }
    else if (state->payload_algid == 0x21)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encR = 0;
      }
    }
    else if (state->payload_algidR == 0x21)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //TODO: add option to bypass enc with a toggle as well

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l4[0]);
  agf (opts, state, state->f_r4[0]);
  agf (opts, state, state->f_l4[1]);
  agf (opts, state, state->f_r4[1]);
  agf (opts, state, state->f_l4[2]);
  agf (opts, state, state->f_r4[2]);

  //interleave left and right channels from the temp (float) buffer with makeshift 'volume' decimation
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->f_l4[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = state->f_r4[0][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->f_l4[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->f_r4[1][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->f_l4[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->f_r4[2][i];
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto FS3_END;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4, NULL); //switch to sizeof(stereo_samp1) * 2?
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*4, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*4, NULL);
  }

  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1) //STDOUT (still need these seperated? or not really?)
  {
    write (opts->audio_out_fd, stereo_samp1, 320*4);
    write (opts->audio_out_fd, stereo_samp2, 320*4);
    write (opts->audio_out_fd, stereo_samp3, 320*4);
  }

  FS3_END:

  memset (state->audio_out_temp_buf, 0.1f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.1f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.1f, sizeof(state->f_l4));
  memset (state->f_r4, 0.1f, sizeof(state->f_r4));

  //if we don't run processAudio, then I don't think we really need any of the items below active
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//float stereo mix 3v2 DMR with 8k to 48k upsample
void playSynthesizedVoiceFS3_48k (dsd_opts * opts, dsd_state * state)
{

  //NOTE: The old version UpsampleF had a high pitch ring to it
  //new version has an odd 'pattering' or 'crackle' sound or clipping?

  int i, n;
  uint8_t encL, encR;
  // int j, k;
  // float prev;
  float stereo_samp1[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp2[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp3[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp4[320*6]; //48k 2-channel stereo interleave mix
  float out[6];
  float temp_l[4][160*6]; //temp storage
  float temp_r[4][160*6]; //temp storage
  float baseline = 0.0f;

  memset (stereo_samp1, baseline, sizeof(stereo_samp1));
  memset (stereo_samp2, baseline, sizeof(stereo_samp2));
  memset (stereo_samp3, baseline, sizeof(stereo_samp3));
  memset (stereo_samp4, baseline, sizeof(stereo_samp4));

  memset (temp_l, baseline, sizeof(temp_l));
  memset (temp_r, baseline, sizeof(temp_r));
  memset (out, baseline, sizeof(out));

  //dmr enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 0;
  encL = (state->dmr_so  >> 6) & 0x1;
  encR = (state->dmr_soR >> 6) & 0x1;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encL = 0;
      }
    }
    else if (state->payload_algid == 0x21)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encR = 0;
      }
    }
    else if (state->payload_algidR == 0x21)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }
  
  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto FS348_END;

  //TODO: Test Upsampling //void upsampleF (float invalue, float prev, float outbuf[6])
  // prev = 0.0f;
  // for (j = 0; j < 3; j++)
  // {
    
  //   for (i = 0; i < 160; i++)
  //   {
  //     upsampleF (state->f_l4[j][i] / ((float) 480*10), prev, out);
  //     // upsampleF (state->f_l4[j][i], prev, out);
  //     for (k = 0; k < 6; k++) temp_l[j][i*6+k] = out[k];
  //   }
  // }

  // prev = 0.0f; //test value
  // for (j = 0; j < 3; j++)
  // {
    
  //   for (i = 0; i < 160; i++)
  //   {
  //     upsampleF (state->f_r4[j][i] / ((float) 480*10), prev, out);
  //     // upsampleF (state->f_r4[j][i], prev, out);
  //     for (k = 0; k < 6; k++) temp_r[j][i*6+k] = out[k];
  //   }
  // }

  n = 160;
  fdmdv_8_to_48_float(temp_l[0], state->f_l4[0], n);
  fdmdv_8_to_48_float(temp_l[1], state->f_l4[1], n);
  fdmdv_8_to_48_float(temp_l[2], state->f_l4[2], n);
  fdmdv_8_to_48_float(temp_r[0], state->f_r4[0], n);
  fdmdv_8_to_48_float(temp_r[1], state->f_r4[1], n);
  fdmdv_8_to_48_float(temp_r[2], state->f_r4[2], n);
  
  //interleave left and right channels from the temp (float) buffer
  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = temp_l[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = temp_r[0][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = temp_l[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = temp_r[1][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = temp_l[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = temp_r[2][i];
  }

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*4*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*4*6, NULL);
  }

  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1) //STDOUT
  {
    write (opts->audio_out_fd, stereo_samp1, 320*4*6);
    write (opts->audio_out_fd, stereo_samp2, 320*4*6);
    write (opts->audio_out_fd, stereo_samp3, 320*4*6);
  }

  FS348_END:

  memset (state->audio_out_temp_buf, baseline, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, baseline, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, baseline, sizeof(state->f_l4));
  memset (state->f_r4, baseline, sizeof(state->f_r4));

  //if we don't run processAudio, then I don't think we really need any of the items below active
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//float stereo mix 4v2 P25p2
void playSynthesizedVoiceFS4 (dsd_opts * opts, dsd_state * state)
{

  //NOTE: This will run for every TS % 2, except on SACCH inverted slots (10 and 11)
  //WIP: Get the real TS number out of the P25p2 frame, and not our ts_counter values

  int i, encL, encR;
  float stereo_samp1[320]; //8k 2-channel stereo interleave mix
  float stereo_samp2[320]; //8k 2-channel stereo interleave mix
  float stereo_samp3[320]; //8k 2-channel stereo interleave mix
  float stereo_samp4[320]; //8k 2-channel stereo interleave mix

  float empty[320]; //this is used to see if we want to play a single 2v or double 2v or not

  //p25p2 enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 1;
  if (state->payload_algid == 0 || state->payload_algid == 0x80)
    encL = 0;
  if (state->payload_algidR == 0 || state->payload_algidR == 0x80)
    encR = 0;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0xAA)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0xAA)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //note, always set a stereo float mix to a baseline value and not zero!
  memset (stereo_samp1, 0.1f, sizeof(stereo_samp1));
  memset (stereo_samp2, 0.1f, sizeof(stereo_samp2));
  memset (stereo_samp3, 0.1f, sizeof(stereo_samp3));
  memset (stereo_samp4, 0.1f, sizeof(stereo_samp4));

  memset (empty, 0.1f, sizeof(empty));

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l4[0]);
  agf (opts, state, state->f_r4[0]);
  agf (opts, state, state->f_l4[1]);
  agf (opts, state, state->f_r4[1]);
  agf (opts, state, state->f_l4[2]); //doing 2 and 3 here will break the memcmp below, so we also do it on empty
  agf (opts, state, state->f_r4[2]);
  agf (opts, state, state->f_l4[3]);
  agf (opts, state, state->f_r4[3]);
  agf (opts, state, empty); //running empty may cause auto gain control issues, and also any auto gain applied here will invalidate the memcmp

  //interleave left and right channels from the temp (float) buffer with makeshift 'volume' decimation
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->f_l4[0][i]; // / ((float) 480.0f*10.0f)
    if (!encR)
      stereo_samp1[i*2+1] = state->f_r4[0][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->f_l4[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->f_r4[1][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->f_l4[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->f_r4[2][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp4[i*2+0] = state->f_l4[3][i];
    if (!encR)
      stereo_samp4[i*2+1] = state->f_r4[3][i];
  }

  if (encL && encR)
    goto END_FS4;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4, NULL); //switch to sizeof(stereo_samp1) * 2?
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*4, NULL);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*4, NULL);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp4, 320*4, NULL);
  }

  //No OSS, since we can't use float output, but the STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1) //STDOUT
  {
    write (opts->audio_out_fd, stereo_samp1, 320*4);
    write (opts->audio_out_fd, stereo_samp2, 320*4);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on  a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp3, 320*4);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp4, 320*4);
  }

  END_FS4:

  memset (state->audio_out_temp_buf, 0.1f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.1f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.1f, sizeof(state->f_l4));
  memset (state->f_r4, 0.1f, sizeof(state->f_r4));

  //if we don't run processAudio, then I don't think we really need any of the items below active
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//float stereo mix 4v2 P25p2 with 8k to 48k upsample
void playSynthesizedVoiceFS4_48k (dsd_opts * opts, dsd_state * state)
{

  //NOTE: The old version UpsampleF had a high pitch ring to it
  //new version has an odd 'pattering' or 'crackle' sound or clipping?

  int i, n, encL, encR;
  // int j, k;
  // float prev;
  float stereo_samp1[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp2[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp3[320*6]; //48k 2-channel stereo interleave mix
  float stereo_samp4[320*6]; //48k 2-channel stereo interleave mix
  float out[6];
  float temp_l[4][160*6]; //temp storage
  float temp_r[4][160*6]; //temp storage
  float empty[320*6];
  float baseline = 0.0f;

  memset (stereo_samp1, baseline, sizeof(stereo_samp1));
  memset (stereo_samp2, baseline, sizeof(stereo_samp2));
  memset (stereo_samp3, baseline, sizeof(stereo_samp3));
  memset (stereo_samp4, baseline, sizeof(stereo_samp4));

  memset (temp_l, baseline, sizeof(temp_l));
  memset (temp_r, baseline, sizeof(temp_r));
  memset (out, baseline, sizeof(out));
  memset (empty, baseline, sizeof(empty));

  //p25p2 enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 1;
  if (state->payload_algid == 0 || state->payload_algid == 0x80)
    encL = 0;
  if (state->payload_algidR == 0 || state->payload_algidR == 0x80)
    encR = 0;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0xAA)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0xAA)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto FS448_END;

  //TODO: Test Upsampling //void upsampleF (float invalue, float prev, float outbuf[6])
  // prev = 0.0f;
  // for (j = 0; j < 4; j++)
  // {
    
  //   for (i = 0; i < 160; i++)
  //   {
  //     upsampleF (state->f_l4[j][i] / ((float) 480*10), prev, out);
  //     // upsampleF (state->f_l4[j][i], prev, out);
  //     for (k = 0; k < 6; k++) temp_l[j][i*6+k] = out[k];
  //   }
  // }

  // prev = 0.0f; //test value
  // for (j = 0; j < 4; j++)
  // {
    
  //   for (i = 0; i < 160; i++)
  //   {
  //     upsampleF (state->f_r4[j][i] / ((float) 480*10), prev, out);
  //     // upsampleF (state->f_r4[j][i], prev, out);
  //     for (k = 0; k < 6; k++) temp_r[j][i*6+k] = out[k];
  //   }
  // }

  n = 160; //not sure why we are using 159 though
  fdmdv_8_to_48_float(temp_l[0], state->f_l4[0], n);
  fdmdv_8_to_48_float(temp_l[1], state->f_l4[1], n);
  fdmdv_8_to_48_float(temp_l[2], state->f_l4[2], n);
  fdmdv_8_to_48_float(temp_l[3], state->f_l4[3], n);
  fdmdv_8_to_48_float(temp_r[0], state->f_r4[0], n);
  fdmdv_8_to_48_float(temp_r[1], state->f_r4[1], n);
  fdmdv_8_to_48_float(temp_r[2], state->f_r4[2], n);
  fdmdv_8_to_48_float(temp_r[3], state->f_r4[3], n);
  
  //interleave left and right channels from the temp (float) buffer
  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = temp_l[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = temp_r[0][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = temp_l[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = temp_r[1][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = temp_l[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = temp_r[2][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp4[i*2+0] = temp_l[3][i];
    if (!encR)
      stereo_samp4[i*2+1] = temp_r[3][i];
  }

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*4*6, NULL);
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*4*6, NULL);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp4, 320*4*6, NULL);
  }

  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1) //STDOUT
  {
    write (opts->audio_out_fd, stereo_samp1, 320*4*6);
    write (opts->audio_out_fd, stereo_samp2, 320*4*6);
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp3, 320*4*6);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp4, 320*4*6);
  }

  FS448_END:

  memset (state->audio_out_temp_buf, baseline, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, baseline, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, baseline, sizeof(state->f_l4));
  memset (state->f_r4, baseline, sizeof(state->f_r4));

  //if we don't run processAudio, then I don't think we really need any of the items below active
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//float stereo mix -- when using P25p1 and P25p2, we need to send P25p1 to this, also DMR MS/Simplex and YSF
void playSynthesizedVoiceFS (dsd_opts * opts, dsd_state * state)
{

  int i;
  int encL;
  float stereo_samp1[320]; //8k 2-channel stereo interleave mix

  //note, always set a stereo float mix to a baseline value and not zero!
  memset (stereo_samp1, 0.1f, sizeof(stereo_samp1));

  //TODO: ENC Check on P25p1, NXDN, dPMR, etc
  encL = 0;

  //TODO: add option to bypass enc with a toggle as well

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l);

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL)
    goto FS_END;


  //interleave left and right channels from the temp (float) buffer with makeshift 'volume' decimation
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->f_l[i];

    //test loading right side with same
    if (!encL)
      stereo_samp1[i*2+1] = state->f_l[i];
  }


  if (opts->audio_out_type == 0) //Pulse Audio
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4, NULL); //switch to sizeof(stereo_samp1) * 2?


  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1)
    write (opts->audio_out_fd, stereo_samp1, 320*4);


  FS_END:

  memset (state->audio_out_temp_buf, 0.1f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.1f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.1f, sizeof(state->f_l4));
  memset (state->f_r4, 0.1f, sizeof(state->f_r4));

  //if we don't run processAudio, then I don't think we really need any of the items below active
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

void playSynthesizedVoiceFM (dsd_opts * opts, dsd_state * state)
{
  
  agf(opts, state, state->f_l);

  if (opts->audio_out_type == 0)
    pa_simple_write(opts->pulse_digi_dev_out, state->f_l, 160*4, NULL);

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5)
    write(opts->audio_out_fd, state->f_l, 160*4);

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
  }

  memset (state->audio_out_temp_buf, 0, sizeof(state->audio_out_temp_buf));

}

//Stereo Mix - Short (SB16LE) -- When Playing Short FDMA samples when setup for stereo output
void playSynthesizedVoiceSS (dsd_opts * opts, dsd_state * state)
{

  int i;
  int encL;
  short stereo_samp1[320]; //8k 2-channel stereo interleave mix
  memset (stereo_samp1, 1, sizeof(stereo_samp1));

  //dmr enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = 0;

  //TODO: Enc Checkdown

  //TODO: add option to bypass enc with a toggle as well

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l[i];

    //testing double left and right channel
    if (!encL)
      stereo_samp1[i*2+1] = state->s_l[i];

  }

  //at this point, if still flagged as enc, then we can skip all playback/writing functions
  if (encL)
    goto SSM_END;

  if (opts->audio_out_type == 0) //Pulse Audio
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2, NULL);

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5) //STDOUT or OSS
    write (opts->audio_out_fd, stereo_samp1, 320*2);


  SSM_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set float temp buffer to baseline
  memset (state->s_l4, 1, sizeof(state->s_l4));
  memset (state->s_r4, 1, sizeof(state->s_r4));

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//short stereo mix 3v2 DMR
void playSynthesizedVoiceSS3 (dsd_opts * opts, dsd_state * state)
{

  //NOTE: This runs once for every two timeslots, if we are in the BS voice loop
  //it doesn't matter if both slots have voice, or if one does, the slot without voice
  //will play silence while this runs if no voice present

  int i;
  uint8_t encL, encR;
  short stereo_samp1[320]; //8k 2-channel stereo interleave mix
  short stereo_samp2[320]; //8k 2-channel stereo interleave mix
  short stereo_samp3[320]; //8k 2-channel stereo interleave mix

  memset (stereo_samp1, 1, sizeof(stereo_samp1));
  memset (stereo_samp2, 1, sizeof(stereo_samp2));
  memset (stereo_samp3, 1, sizeof(stereo_samp3));

  //dmr enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 0;
  encL = (state->dmr_so  >> 6) & 0x1;
  encR = (state->dmr_soR >> 6) & 0x1;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encL = 0;
      }
    }
    else if (state->payload_algid == 0x21)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encR = 0;
      }
    }
    else if (state->payload_algidR == 0x21)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //TODO: add option to bypass enc with a toggle as well

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l4[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = state->s_r4[0][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->s_l4[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->s_r4[1][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->s_l4[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->s_r4[2][i];
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto SS3_END;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*2, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*2, NULL);
  }
  

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5) //STDOUT or OSS
  {
    write (opts->audio_out_fd, stereo_samp1, 320*2);
    write (opts->audio_out_fd, stereo_samp2, 320*2);
    write (opts->audio_out_fd, stereo_samp3, 320*2);
  }

  SS3_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set float temp buffer to baseline
  memset (state->s_l4, 1, sizeof(state->s_l4));
  memset (state->s_r4, 1, sizeof(state->s_r4));

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//short stereo mix 4v2 P25p2
void playSynthesizedVoiceSS4 (dsd_opts * opts, dsd_state * state)
{

  //NOTE: This runs once for every two timeslots, if we are in the BS voice loop
  //it doesn't matter if both slots have voice, or if one does, the slot without voice
  //will play silence while this runs if no voice present

  int i;
  uint8_t encL, encR;
  short stereo_samp1[320]; //8k 2-channel stereo interleave mix
  short stereo_samp2[320]; //8k 2-channel stereo interleave mix
  short stereo_samp3[320]; //8k 2-channel stereo interleave mix
  short stereo_samp4[320];

  short empty[320]; //this is used to see if we want to play a single 2v or double 2v or not

  memset (stereo_samp1, 1, sizeof(stereo_samp1));
  memset (stereo_samp2, 1, sizeof(stereo_samp2));
  memset (stereo_samp3, 1, sizeof(stereo_samp3));
  memset (stereo_samp4, 1, sizeof(stereo_samp4));

  memset (empty, 1, sizeof(empty));
  

  //p25p2 enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 1;
  if (state->payload_algid == 0 || state->payload_algid == 0x80)
    encL = 0;
  if (state->payload_algidR == 0 || state->payload_algidR == 0x80)
    encR = 0;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0xAA)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0xAA)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }


  //TODO: add option to bypass enc with a toggle as well

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l4[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = state->s_r4[0][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->s_l4[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->s_r4[1][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->s_l4[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->s_r4[2][i];
  }

  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp4[i*2+0] = state->s_l4[3][i];
    if (!encR)
      stereo_samp4[i*2+1] = state->s_r4[3][i];
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto SS4_END;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*2, NULL);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*2, NULL);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp4, 320*2, NULL);
  }
  

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5) //STDOUT or OSS
  {
    write (opts->audio_out_fd, stereo_samp1, 320*2);
    write (opts->audio_out_fd, stereo_samp2, 320*2);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp3, 320*2);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp4, 320*2);
  }

  SS4_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set float temp buffer to baseline
  memset (state->s_l4, 1, sizeof(state->s_l4));
  memset (state->s_r4, 1, sizeof(state->s_r4));

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//short stereo mix 3v2 DMR with tapped 8k to 48k upsample
void playSynthesizedVoiceSS3_48k (dsd_opts * opts, dsd_state * state)
{

  //NOTE: This runs once for every two timeslots, if we are in the BS voice loop
  //it doesn't matter if both slots have voice, or if one does, the slot without voice
  //will play silence while this runs if no voice present

  int i;
  uint8_t encL, encR;
  short stereo_samp1[320*6]; //48k 2-channel stereo interleave mix
  short stereo_samp2[320*6]; //48k 2-channel stereo interleave mix
  short stereo_samp3[320*6]; //48k 2-channel stereo interleave mix

  memset (stereo_samp1, 1, sizeof(stereo_samp1));
  memset (stereo_samp2, 1, sizeof(stereo_samp2));
  memset (stereo_samp3, 1, sizeof(stereo_samp3));

  //dmr enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 0;
  encL = (state->dmr_so  >> 6) & 0x1;
  encR = (state->dmr_soR >> 6) & 0x1;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encL = 0;
      }
    }
    else if (state->payload_algid == 0x21)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0)
    {
      if (state->K != 0 || state->K1 != 0)
      {
        encR = 0;
      }
    }
    else if (state->payload_algidR == 0x21)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //TODO: add option to bypass enc with a toggle as well

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l4u[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = state->s_r4u[0][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->s_l4u[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->s_r4u[1][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->s_l4u[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->s_r4u[2][i];
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto SS3u_END;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*2*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*2*6, NULL);
  }
  

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5) //STDOUT or OSS
  {
    write (opts->audio_out_fd, stereo_samp1, 320*2*6);
    write (opts->audio_out_fd, stereo_samp2, 320*2*6);
    write (opts->audio_out_fd, stereo_samp3, 320*2*6);
  }

  SS3u_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set short temp buffer to baseline
  memset (state->s_l4u, 1, sizeof(state->s_l4u));
  memset (state->s_r4u, 1, sizeof(state->s_r4u));

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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

//short stereo mix 4v2 P25p2 with tapped 8k to 48k upsample
void playSynthesizedVoiceSS4_48k (dsd_opts * opts, dsd_state * state)
{

  int i, encL, encR;
  short stereo_samp1[320*6]; //48k 2-channel stereo interleave mix
  short stereo_samp2[320*6]; //48k 2-channel stereo interleave mix
  short stereo_samp3[320*6]; //48k 2-channel stereo interleave mix
  short stereo_samp4[320*6]; //48k 2-channel stereo interleave mix

  memset (stereo_samp1, 1, sizeof(stereo_samp1));
  memset (stereo_samp2, 1, sizeof(stereo_samp2));
  memset (stereo_samp3, 1, sizeof(stereo_samp3));
  memset (stereo_samp4, 1, sizeof(stereo_samp4));

  float empty[320]; //this is used to see if we want to play a single 2v or double 2v or not

  memset (empty, 1, sizeof(empty));

  //p25p2 enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = encR = 1;
  if (state->payload_algid == 0 || state->payload_algid == 0x80)
    encL = 0;
  if (state->payload_algidR == 0 || state->payload_algidR == 0x80)
    encR = 0;

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0xAA)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  if (encR)
  {
    if (state->payload_algidR == 0xAA)
    {
      if (state->RR != 0)
      {
        encR = 0;
      }
    }
  }

  //TODO: add option to bypass enc with a toggle as well

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l4u[0][i];
    if (!encR)
      stereo_samp1[i*2+1] = state->s_r4u[0][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp2[i*2+0] = state->s_l4u[1][i];
    if (!encR)
      stereo_samp2[i*2+1] = state->s_r4u[1][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp3[i*2+0] = state->s_l4u[2][i];
    if (!encR)
      stereo_samp3[i*2+1] = state->s_r4u[2][i];
  }

  for (i = 0; i < 160*6; i++)
  {
    if (!encL)
      stereo_samp4[i*2+0] = state->s_l4u[3][i];
    if (!encR)
      stereo_samp4[i*2+1] = state->s_r4u[3][i];
  }

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL && encR)
    goto SS4u_END;

  if (opts->audio_out_type == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2*6, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp2, 320*2*6, NULL);
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp3, 320*2*6, NULL);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      pa_simple_write(opts->pulse_digi_dev_out, stereo_samp4, 320*2*6, NULL);
  }
  

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5) //STDOUT or OSS
  {
    write (opts->audio_out_fd, stereo_samp1, 320*2*6);
    write (opts->audio_out_fd, stereo_samp2, 320*2*6);
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp3, 320*2*6);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      write (opts->audio_out_fd, stereo_samp4, 320*2*6);
  }

  SS4u_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set short temp buffer to baseline
  memset (state->s_l4u, 1, sizeof(state->s_l4u));
  memset (state->s_r4u, 1, sizeof(state->s_r4u));

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
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