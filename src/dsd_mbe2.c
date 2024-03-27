/*-------------------------------------------------------------------------------
 * 
 * 
 *
 * TODO: Fill me in'
 * 
 *
 * LWVMOBILE
 * 2023-07 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/
 
//new and simplified/organized ambe and imbe handling
//moving all audio handling and decryption to seperate files for simplicity (eventually)

#include "dsd.h"

//the initial functions will ONLY return demodulated ambe or imbe frames, THAT'S IT!
//decryption and audio handling etc will be handled at a different area

//using soft_demod will also allow for not compiling mbelib and also using a DVstick in the future

//P25p1 IMBE 7200 or AMBE+2 EFR
void soft_demod_imbe7200 (dsd_state * state, char imbe_fr7200[8][23], char imbe_d[88])
{
  state->errs = mbe_eccImbe7200x4400C0 (imbe_fr7200);
  state->errs2 = state->errs;
  mbe_demodulateImbe7200x4400Data (imbe_fr7200);
  state->errs2 += mbe_eccImbe7200x4400Data (imbe_fr7200, imbe_d);
  state->debug_audio_errors += state->errs2;

}

//ProVoice IMBE 7100
void soft_demod_imbe7100 (dsd_state * state, char imbe_fr7100[7][24], char imbe_d[88])
{
  state->errs = mbe_eccImbe7100x4400C0 (imbe_fr7100);
  state->errs2 = state->errs;
  mbe_demodulateImbe7100x4400Data (imbe_fr7100);
  state->errs2 += mbe_eccImbe7100x4400Data (imbe_fr7100, imbe_d);
  state->debug_audio_errors += state->errs2;

}

//AMBE+2 EHR
void soft_demod_ambe2_ehr(dsd_state * state, char ambe2_ehr[4][24], char ambe_d[49])
{
  state->errs = mbe_eccAmbe3600x2450C0 (ambe2_ehr);
  state->errs2 = state->errs;
  mbe_demodulateAmbe3600x2450Data (ambe2_ehr);
  state->errs2 += mbe_eccAmbe3600x2450Data (ambe2_ehr, ambe_d);

}

//AMBE One Shot (DSTAR)
void soft_demod_ambe_dstar(dsd_opts * opts, dsd_state * state, char ambe_fr[4][24], char ambe_d[49])
{
  mbe_processAmbe3600x2400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, 
    state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
  if (opts->floating_point == 1)
    memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
  else processAudio(opts, state);
}

//AMBE+2 One Shot (X2-TDMA)
void soft_demod_ambe_x2(dsd_opts * opts, dsd_state * state, char ambe_fr[4][24], char ambe_d[49])
{
  mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, 
    state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
  if (opts->floating_point == 1)
    memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
  else processAudio(opts, state);
}

void soft_mbe (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24])
{
  int i; 
  char ambe_d[49];
  char imbe_d[88];
  int slot = state->currentslot;
  memset (ambe_d, 0, sizeof(ambe_d));
  memset (imbe_d, 0, sizeof(imbe_d));

  UNUSED(i);

  //P25p1, YSF FR, NXDN EFR
  if (state->synctype == 0 || state->synctype == 1)
  {
    soft_demod_imbe7200(state, imbe_fr, imbe_d);
    //handle decryption here
    //print IMBE frame
    mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
      imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    //send to playback here
  }
    
  //ProVoice
  else if (state->synctype == 14 || state->synctype == 15)
  {
    soft_demod_imbe7100(state, imbe7100_fr, imbe_d);
    //nothing to do

    //convert to 7200
    mbe_convertImbe7100to7200(imbe_d);
    mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
      imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    //send to playback here

  }
    
  //D-STAR AMBE
  else if (state->synctype == 6 || state->synctype == 7)
  {
    soft_demod_ambe_dstar(opts, state, ambe_fr, ambe_d);
    if(opts->payload == 1)
      PrintAMBEData (opts, state, ambe_d);

    if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
      playSynthesizedVoiceMS(opts, state);
    if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
      playSynthesizedVoiceFM(opts, state);
    if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
      playSynthesizedVoiceSS(opts, state);
    if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
      playSynthesizedVoiceFS(opts, state);

    if (opts->wav_out_f != NULL)
      writeSynthesizedVoice (opts, state);

    if (opts->mbe_out_f != NULL)
      saveAmbe2450Data (opts, state, ambe_d);
  }

  //X2-TDMA AMBE
  else if (state->synctype >= 2 && state->synctype <= 5)
  {
    soft_demod_ambe_x2(opts, state, ambe_fr, ambe_d);
    if(opts->payload == 1)
      PrintAMBEData (opts, state, ambe_d);

    if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
      playSynthesizedVoiceMS(opts, state);
    if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
      playSynthesizedVoiceFM(opts, state);
    if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
      playSynthesizedVoiceSS(opts, state);
    if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
      playSynthesizedVoiceFS(opts, state);

    if (opts->wav_out_f != NULL)
      writeSynthesizedVoice (opts, state);

    if (opts->mbe_out_f != NULL)
      saveAmbe2450Data (opts, state, ambe_d);
  }

  //AMBE+2 EHR (NXDN, DMR, P25p2, YSF VD/1)
  else
  {
    soft_demod_ambe2_ehr (state, ambe_fr, ambe_d);
    //decrypt here
    if(opts->payload == 1)
      PrintAMBEData (opts, state, ambe_d);
    //make left or right channel decision
    if (slot == 0)
    {
      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, 
        ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    }
    if (slot == 1)
    {
      mbe_processAmbe2450Dataf (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR, 
        ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);
    }
    
  }
}

