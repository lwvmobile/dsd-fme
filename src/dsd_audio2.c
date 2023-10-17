/*-------------------------------------------------------------------------------
 * 
 * 
 *
 * TODO: Fill me in'
 * 
 *
 * LWVMOBILE
 * 2023-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <math.h>

//TODO: WAV File saving (works fine on shorts, but on float, writing short to wav is not auto-gained, 
//so super quiet, either convert to float wav files, or run processAudio AFTER memcpy of the temp_buf)

//simple method -- produces cleaner results, but can be muted (or very loud) at times
//has manual control only, no auto gain
// void agf (dsd_opts * opts, dsd_state * state, float samp[160], int slot)
// {
//   int i;
//   float mmax = 0.75f;
//   float mmin = -0.75f;
//   float df = 3276.7f;

//   //Default gain value of 1.0f on audio_gain == 0
//   float gain = 1.0f;

//   //make it so that gain is 1.0f on 25.0f aout, and 2.0f on 50 aout
//   if (opts->audio_gain != 0 && slot == 0)
//     gain = state->aout_gain / 25.0f;

//   if (opts->audio_gain != 0 && slot == 1)
//     gain = state->aout_gainR / 25.0f;
  
//   //mono output handles slightly different, need to further decimate
//   if (opts->pulse_digi_out_channels == 1)
//     df *= 4.0f;

//   for (i = 0; i < 160; i++)
//   {
//     //simple decimation
//     samp[i] /= df;
//     samp[i] *= 0.65f;

//     //simple clipping
//     if (samp[i] > mmax)
//       samp[i] = mmax;
//     if (samp[i] < mmin)
//       samp[i] = mmin;

//     //user gain factor
//     samp[i] *= gain;
    
//   }

// }

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

  memset (stereo_samp1, 0.0f, sizeof(stereo_samp1));
  memset (stereo_samp2, 0.0f, sizeof(stereo_samp2));
  memset (stereo_samp3, 0.0f, sizeof(stereo_samp3));

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

  //CHEAT: Using the slot on/off, use that to set encL or encR back on
  //as a simple way to turn off voice synthesis in a particular slot
  //its not really 'disabled', we just aren't playing it
  if (opts->slot1_on == 0) encL = 1;
  if (opts->slot2_on == 0) encR = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");
  char modeR[8];
  sprintf (modeR, "%s", "");

  int TGL = state->lasttg;
  int TGR = state->lasttgR;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
  {
    sprintf (modeL, "%s", "B");
    sprintf (modeR, "%s", "B");
  }

  for (i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
    if (state->group_array[i].groupNumber == TGR)
    {
      strcpy (modeR, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;
  if (strcmp(modeR, "B") == 0) encR = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  if (state->tg_hold != 0 && state->tg_hold != TGR) encR = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;
  if (state->tg_hold != 0 && state->tg_hold == TGR) encR = 0;

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l4[0],0);
  agf (opts, state, state->f_r4[0],1);
  agf (opts, state, state->f_l4[1],0);
  agf (opts, state, state->f_r4[1],1);
  agf (opts, state, state->f_l4[2],0);
  agf (opts, state, state->f_r4[2],1);

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

  if (opts->audio_out_type == 8) //UDP Audio
  {
    udp_socket_blaster (opts, state, 320*4, stereo_samp1);
    udp_socket_blaster (opts, state, 320*4, stereo_samp2);
    udp_socket_blaster (opts, state, 320*4, stereo_samp3);
  }

  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1) //STDOUT (still need these seperated? or not really?)
  {
    write (opts->audio_out_fd, stereo_samp1, 320*4);
    write (opts->audio_out_fd, stereo_samp2, 320*4);
    write (opts->audio_out_fd, stereo_samp3, 320*4);
  }

  FS3_END:

  memset (state->audio_out_temp_buf, 0.0f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.0f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.0f, sizeof(state->f_l4));
  memset (state->f_r4, 0.0f, sizeof(state->f_r4));

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

//NOTE: On FS4 and SS4 voice, the longer the transmission, the more the function will start to lag
//the entire DSD-FME loop due to the skipping of playback on SACCH frames (causes noticeable skip when it does play them),
//this isn't a major problem, since the buffer can handle it, but it does delay return to CC until the end
//of the call on busy systems where both VCH slots are constantly busy with voice
//the longer the call, the more delayed until returning to the control channel

//NOTE: Disabling voice synthesis clears up the delay issue (obviosly since we aren't having to wait on it to play)
//disabling voice in only one slot will also fix most random stutter from the 4v in one slot, and 2v in the other slot

//NOTE: The same skip may be occurring on the main and v2.1b branches of DSD-FME as well, so that may be due to the 4v/2v and
//playing back immediately instead of buffering x number of samples or 4v/2v to get a smoother playback

//NOTE: When using capture bins for playback, this issue is not as observable compared to real time reception due to how fast
//we can blow through pure data on bin files compared to waiting for the real time reception

//its usually a lot more noticeable on dual voices than single (probably due to various arrangements of dual 4v/2v in each superframe)

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

  //CHEAT: Using the slot on/off, use that to set encL or encR back on
  //as a simple way to turn off voice synthesis in a particular slot
  //its not really 'disabled', we just aren't playing it
  if (opts->slot1_on == 0) encL = 1;
  if (opts->slot2_on == 0) encR = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");
  char modeR[8];
  sprintf (modeR, "%s", "");

  int TGL = state->lasttg;
  int TGR = state->lasttgR;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
  {
    sprintf (modeL, "%s", "B");
    sprintf (modeR, "%s", "B");
  }

  for (i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
    if (state->group_array[i].groupNumber == TGR)
    {
      strcpy (modeR, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;
  if (strcmp(modeR, "B") == 0) encR = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  if (state->tg_hold != 0 && state->tg_hold != TGR) encR = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;
  if (state->tg_hold != 0 && state->tg_hold == TGR) encR = 0;

  memset (stereo_samp1, 0.0f, sizeof(stereo_samp1));
  memset (stereo_samp2, 0.0f, sizeof(stereo_samp2));
  memset (stereo_samp3, 0.0f, sizeof(stereo_samp3));
  memset (stereo_samp4, 0.0f, sizeof(stereo_samp4));

  memset (empty, 0.0f, sizeof(empty));

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l4[0],0);
  agf (opts, state, state->f_r4[0],1);
  agf (opts, state, state->f_l4[1],0);
  agf (opts, state, state->f_r4[1],1);
  agf (opts, state, state->f_l4[2],0); //doing 2 and 3 here will break the memcmp below, so we also do it on empty
  agf (opts, state, state->f_r4[2],1);
  agf (opts, state, state->f_l4[3],0);
  agf (opts, state, state->f_r4[3],1);
  agf (opts, state, empty,2); //running empty may invalidate the memcmp, ideally both this empty and an empty f_ should come out with the same result anyways

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

  if (opts->audio_out_type == 8) //UDP Audio
  {
    udp_socket_blaster (opts, state, 320*4, stereo_samp1);
    udp_socket_blaster (opts, state, 320*4, stereo_samp2);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      udp_socket_blaster (opts, state, 320*4, stereo_samp3);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      udp_socket_blaster (opts, state, 320*4, stereo_samp4);
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

  memset (state->audio_out_temp_buf, 0.0f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.0f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.0f, sizeof(state->f_l4));
  memset (state->f_r4, 0.0f, sizeof(state->f_r4));

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

//float stereo mix -- when using Float Stereo Output, we need to send P25p1, DMR MS/Simplex, DStar, and YSF here
void playSynthesizedVoiceFS (dsd_opts * opts, dsd_state * state)
{

  int i;
  int encL;
  float stereo_samp1[320]; //8k 2-channel stereo interleave mix

  memset (stereo_samp1, 0.0f, sizeof(stereo_samp1));

  //TODO: ENC Check on P25p1, DMR MS, etc
  encL = 0;

  //Enc Checkdown -- P25p1 when run with -ft -y switch
  if (state->synctype == 0 || state->synctype == 1)
  {
    if (state->payload_algid != 0 && state->payload_algid != 0x80)
    {
      encL = 1;
    }
  }
  
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

  //TODO: add option to bypass enc with a toggle as well

  if (opts->slot1_on == 0) encL = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");

  int TGL = state->lasttg;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
    sprintf (modeL, "%s", "B");

  for (int i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      break;
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;

  //run autogain on the f_ buffers
  agf (opts, state, state->f_l,0);

  //at this point, if both channels are still flagged as enc, then we can skip all playback/writing functions
  if (encL)
    goto FS_END;

  //interleave left and right channels from the temp (float) buffer with makeshift 'volume' decimation
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->f_l[i] * 0.5f;

    //test loading right side with same
    if (!encL)
      stereo_samp1[i*2+1] = state->f_l[i] * 0.5f;
  }


  if (opts->audio_out_type == 0) //Pulse Audio
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*4, NULL); //switch to sizeof(stereo_samp1) * 2?

  if (opts->audio_out_type == 8) //UDP Audio
    udp_socket_blaster (opts, state, 320*4, stereo_samp1);

  //No OSS, since we can't use float output, but STDOUT can with play, aplay, etc

  if (opts->audio_out_type == 1)
    write (opts->audio_out_fd, stereo_samp1, 320*4);


  FS_END:

  memset (state->audio_out_temp_buf, 0.0f, sizeof(state->audio_out_temp_buf));
  memset (state->audio_out_temp_bufR, 0.0f, sizeof(state->audio_out_temp_bufR));

  //set float temp buffer to baseline
  memset (state->f_l4, 0.0f, sizeof(state->f_l4));
  memset (state->f_r4, 0.0f, sizeof(state->f_r4));

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
  
  agf(opts, state, state->f_l,0);

  //mono may need an additional decimation
  // for (int i = 0; i < 160; i++)
  //   state->f_l[i] *= 0.5f;

  int encL = 0;

  //Enc Checkdown -- P25p1 when run with -ft -y switch
  if (state->synctype == 0 || state->synctype == 1)
  {
    if (state->payload_algid != 0 && state->payload_algid != 0x80)
    {
      encL = 1;
    }
  }

  //NXDN
  if (state->nxdn_cipher_type != 0)
  {
    encL = 1;
  }

  //checkdown to see if we can lift the 'mute' if a key is available
  if (encL)
  {
    if (state->payload_algid == 0xAA || state->nxdn_cipher_type == 0x1)
    {
      if (state->R != 0)
      {
        encL = 0;
      }
    }
  }

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");

  int TGL = state->lasttg;
  if (opts->frame_nxdn48 == 1 || opts->frame_nxdn96 == 1)
    TGL = state->nxdn_last_tg;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
    sprintf (modeL, "%s", "B");

  for (int i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      break;
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;

  if (encL == 1)
    goto vfm_end;

  if (opts->slot1_on == 0)
    goto vfm_end;

  if (opts->audio_out_type == 0)
    pa_simple_write(opts->pulse_digi_dev_out, state->f_l, 160*4, NULL);

  if (opts->audio_out_type == 8) //UDP Audio
    udp_socket_blaster (opts, state, 160*4, state->f_l);

  if (opts->audio_out_type == 1 || opts->audio_out_type == 5)
    write(opts->audio_out_fd, state->f_l, 160*4);

  vfm_end:

  if (state->audio_out_idx2 >= 800000)
  {
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_buf_p = state->audio_out_buf + 100;
    memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
    memset (state->audio_out_buf, 0, 100 * sizeof (short));
    state->audio_out_idx2 = 0;
  }
  
  memset (state->f_l, 0.0f, sizeof(state->f_l));
  memset (state->audio_out_temp_buf, 0.0f, sizeof(state->audio_out_temp_buf));

}

//Stereo Mix - Short (SB16LE) -- When Playing Short FDMA samples when setup for stereo output
void playSynthesizedVoiceSS (dsd_opts * opts, dsd_state * state)
{

  int i;
  int encL;
  short stereo_samp1[320]; //8k 2-channel stereo interleave mix
  memset (stereo_samp1, 1, sizeof(stereo_samp1));

  //enc checkdown for whether or not to fill the stereo sample or not for playback or writing
  encL = 0;

  //Enc Checkdown -- P25p1 when run with -ft switch
  if (state->synctype == 0 || state->synctype == 1)
  {
    if (state->payload_algid != 0 && state->payload_algid != 0x80)
    {
      encL = 1;
    }
  }

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
  //TODO: add option to bypass enc with a toggle as well

  if (opts->slot1_on == 0) encL = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");

  int TGL = state->lasttg;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
    sprintf (modeL, "%s", "B");

  for (int i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      break;
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;

  //interleave left and right channels from the short storage area
  for (i = 0; i < 160; i++)
  {
    if (!encL)
      stereo_samp1[i*2+0] = state->s_l[i] / 2;

    //testing double left and right channel
    if (!encL)
      stereo_samp1[i*2+1] = state->s_l[i] / 2;

  }

  //at this point, if still flagged as enc, then we can skip all playback/writing functions
  if (encL)
    goto SSM_END;

  if (opts->audio_out_type == 0) //Pulse Audio
    pa_simple_write(opts->pulse_digi_dev_out, stereo_samp1, 320*2, NULL);

  if (opts->audio_out_type == 8) //UDP Audio
    udp_socket_blaster (opts, state, 320*2, stereo_samp1);

  if (opts->audio_out_type == 1 || opts->audio_out_type == 2) //STDOUT or OSS 8k/2
    write (opts->audio_out_fd, stereo_samp1, 320*2);


  SSM_END:

  //run cleanup since we pulled stuff from processAudio
  state->audio_out_idx = 0;
  state->audio_out_idxR = 0;

  //set float temp buffer to baseline
  memset (state->s_l, 1, sizeof(state->s_l));
  memset (state->s_r, 1, sizeof(state->s_r));

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

  //CHEAT: Using the slot on/off, use that to set encL or encR back on
  //as a simple way to turn off voice synthesis in a particular slot
  //its not really 'disabled', we just aren't playing it
  if (opts->slot1_on == 0) encL = 1;
  if (opts->slot2_on == 0) encR = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");
  char modeR[8];
  sprintf (modeR, "%s", "");

  int TGL = state->lasttg;
  int TGR = state->lasttgR;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
  {
    sprintf (modeL, "%s", "B");
    sprintf (modeR, "%s", "B");
  }

  for (i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
    if (state->group_array[i].groupNumber == TGR)
    {
      strcpy (modeR, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;
  if (strcmp(modeR, "B") == 0) encR = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  if (state->tg_hold != 0 && state->tg_hold != TGR) encR = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;
  if (state->tg_hold != 0 && state->tg_hold == TGR) encR = 0;

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

  if (opts->audio_out_type == 8) //UDP Audio
  {
    udp_socket_blaster (opts, state, 320*2, stereo_samp1);
    udp_socket_blaster (opts, state, 320*2, stereo_samp2);
    udp_socket_blaster (opts, state, 320*2, stereo_samp3);
  }

  if (opts->audio_out_type == 1 || opts->audio_out_type == 2) //STDOUT or OSS 8k/2channel
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

  //CHEAT: Using the slot on/off, use that to set encL or encR back on
  //as a simple way to turn off voice synthesis in a particular slot
  //its not really 'disabled', we just aren't playing it
  if (opts->slot1_on == 0) encL = 1;
  if (opts->slot2_on == 0) encR = 1;

  //WIP: Mute if on B list (or not W list)
  char modeL[8];
  sprintf (modeL, "%s", "");
  char modeR[8];
  sprintf (modeR, "%s", "");

  int TGL = state->lasttg;
  int TGR = state->lasttgR;

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1)
  {
    sprintf (modeL, "%s", "B");
    sprintf (modeR, "%s", "B");
  }

  for (i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == TGL)
    {
      strcpy (modeL, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
    if (state->group_array[i].groupNumber == TGR)
    {
      strcpy (modeR, state->group_array[i].groupMode);
      // break; //need to keep going to check other potential slot group
    }
  }

  //flag either left or right as 'enc' to mute if B
  if (strcmp(modeL, "B") == 0) encL = 1;
  if (strcmp(modeR, "B") == 0) encR = 1;

  //if TG Hold in place, mute anything but that TG
  if (state->tg_hold != 0 && state->tg_hold != TGL) encL = 1;
  if (state->tg_hold != 0 && state->tg_hold != TGR) encR = 1;
  //likewise, override and unmute if TG hold matches TG
  if (state->tg_hold != 0 && state->tg_hold == TGL) encL = 0;
  if (state->tg_hold != 0 && state->tg_hold == TGR) encR = 0;

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

  if (opts->audio_out_type == 8) //UDP Audio
  {
    udp_socket_blaster (opts, state, 320*2, stereo_samp1);
    udp_socket_blaster (opts, state, 320*2, stereo_samp2);
    //only play these two if not a single 2v or double 2v (minor skip can still occur on a 4v and 2v combo, but will probably only be perceivable if one is a tone)
    if (memcmp(empty, stereo_samp3, sizeof(empty)) != 0)
      udp_socket_blaster (opts, state, 320*2, stereo_samp3);
    if (memcmp(empty, stereo_samp4, sizeof(empty)) != 0)
      udp_socket_blaster (opts, state, 320*2, stereo_samp4);
  }
  

  if (opts->audio_out_type == 1 || opts->audio_out_type == 2) //STDOUT or OSS 8k/2channel
  {
    write (opts->audio_out_fd, stereo_samp1, 320*2);
    write (opts->audio_out_fd, stereo_samp2, 320*2);
    //only play these two if not a single 2v or double 2v
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

//largely borrowed from Boatbod OP25 (simplified single tone ID version)
void soft_tonef (float samp[160], int n, int ID, int AD)
{
  int i;
  float step1, step2, amplitude, freq1, freq2;

  float gain = 1.0f;

  //needs opts and state to work....don't feel like doing the changes
  // if (opts->audio_gain != 0 && slot == 0)
  //   gain = state->aout_gain / 25.0f;

  // Synthesize tones
  freq1 = 31.25 * ID; freq2 = freq1;
  step1 = 2 * M_PI * freq1 / 8000.0f;
  step2 = 2 * M_PI * freq2 / 8000.0f;
  amplitude = AD * 75.0f;

  for (i = 0; i < 160; i++)
  {
    samp[i] = (float) ( amplitude * (sin((n) * step1)/2 + sin((n) * step2)/2) );
    samp[i] /= 8000.0f;
    samp[i] *= gain;
    n++;
  }

}

//older version, does better at normalizing audio, but also sounds 'flatter' and 'muddier'
//probably too much compression and adjustments on the samples (tones have a slight tremolo effect)
//Remus, enable this one and disable the one above if you prefer
void agf (dsd_opts * opts, dsd_state * state, float samp[160], int slot)
{
  int i, j, run;
  run = 1;
  float empty[160];
  memset (empty, 0.0f, sizeof(empty));

  float mmax = 0.90f;
  float mmin = -0.90f;
  float aavg = 0.0f; //average of the absolute value
  float df; //decimation value
  df = 3277.0f; //test value

  //trying things
  float gain = 1.0f;

  //test increasing gain on DMR EP samples with degraded AMBE samples
  if (state->payload_algid == 0x21 || state->payload_algidR == 0x21)
    gain = 1.75f;

  if (opts->audio_gain != 0)
    gain = opts->audio_gain / 25.0f;

  //this comparison is to determine whether or not to run gain on 'empty' samples (2v last 2, silent frames, etc)
  if (memcmp(empty, samp, sizeof(empty)) == 0) run = 0;
  if (run == 0) goto AGF_END;

  for (j = 0; j < 8; j++)
  {

    if (slot == 0)
      df = 384.0f * (50.0f - state->aout_gain);
    if (slot == 1)
      df = 384.0f * (50.0f - state->aout_gainR);

    for (i = 0; i < 20; i++)
    {

      samp[(j*20)+i] = samp[(j*20)+i] / df;

      // samp[(j*20)+i] *= gain;
      // aavg += fabsf(samp[i]);

      //simple clipping
      if (samp[(j*20)+i] > mmax)
        samp[(j*20)+i] = mmax;
      if (samp[(j*20)+i] < mmin)
        samp[(j*20)+i] = mmin;

      aavg += fabsf(samp[i]);

      samp[(j*20)+i] *= gain * 0.8f;

    } //i loop

    aavg /= 20.0f; //average of the 20 samples

    //debug 
    // fprintf (stderr, "\nS%d - DF = %f AAVG = %f", slot, df, aavg);

    if (slot == 0)
    {
      if (aavg < 0.075f && state->aout_gain < 46.0f) state->aout_gain += 0.5f;
      if (aavg >= 0.075f && state->aout_gain > 1.0f) state->aout_gain -= 0.5f;
    }

    if (slot == 1)
    {
      if (aavg < 0.075f && state->aout_gainR < 46.0f) state->aout_gainR += 0.5f;
      if (aavg >= 0.075f && state->aout_gainR > 1.0f) state->aout_gainR -= 0.5f;
    }

    aavg = 0.0f; //reset

  } //j loop
  
  AGF_END: ; //do nothing

}

//the previous version, not quite as good at audio gain normalization, but keeping it just in case
// void agf (dsd_opts * opts, dsd_state * state, float samp[160], int slot)
// {
//   int i, run;
//   run = 1;
//   float empty[160];
//   memset (empty, 0.0f, sizeof(empty));

//   float mmax = 0.75f;
//   float mmin = -0.75f;
//   float aavg = 0.0f; //average of the absolute value
//   float df; //decimation value
//   df = 8000.0f; //test value -- this would be the ideal perfect value if everybody spoke directly into the mic at a reasonable volume

//   if (slot == 0)
//     df = (384.0f / (float)opts->pulse_digi_out_channels) * (50.0f - state->aout_gain);
//   if (slot == 1)
//     df = (384.0f / (float)opts->pulse_digi_out_channels) * (50.0f - state->aout_gainR);

//   //this comparison is to determine whether or not to run gain on 'empty' samples (2v last 2, silent frames, etc)
//   if (memcmp(empty, samp, sizeof(empty)) == 0) run = 0;
//   if (run == 0) goto AGF_END;

//   for (i = 0; i < 160; i++)
//   {

//     samp[i] = samp[i] / df;

//     aavg += fabsf(samp[i]);

//     //simple clipping
//     if (samp[i] > mmax)
//       samp[i] = mmax;
//     if (samp[i] < mmin)
//       samp[i] = mmin;

//     //may have been a tad too loud on the high end
//     // samp[i] *= 0.5f; //testing various values here

//   }

//   aavg /= 160.0f;

//   if (slot == 0)
//   {
//     if (aavg < 0.075f && state->aout_gain < 42.0f) state->aout_gain += 1.0f;
//     if (aavg >= 0.075f && state->aout_gain > 1.0f) state->aout_gain -= 1.0f;
//   }

//   if (slot == 1)
//   {
//     if (aavg < 0.075f && state->aout_gainR < 42.0f) state->aout_gainR += 1.0f;
//     if (aavg >= 0.075f && state->aout_gainR > 1.0f) state->aout_gainR -= 1.0f;
//   }

//   //debug 
//   // fprintf (stderr, "\nS%d - DF = %f AAVG = %f", slot, df, aavg);
  
//   AGF_END: ; //do nothing

// }