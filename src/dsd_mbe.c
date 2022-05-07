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

static void DecipherData(char * Input, char * KeyStream, char * Output, int NbData);

void
playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv)
{

  int i;
  char imbe_d[88];
  char ambe_d[49];

  for (i = state->optind; i < argc; i++)
    {
      sprintf (opts->mbe_in_file, "%s", argv[i]);
      openMbeInFile (opts, state);
      mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
      fprintf (stderr, "playing %s\n", opts->mbe_in_file);
      while (feof (opts->mbe_in_f) == 0)
        {
          if (state->mbe_file_type == 0)
            {
              readImbe4400Data (opts, state, imbe_d);
              mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
              processAudio (opts, state);
              if (opts->wav_out_f != NULL)
                {
                  writeSynthesizedVoice (opts, state);
                }

              if (opts->audio_out == 1)
                {

                  playSynthesizedVoice (opts, state);

                }
            }
          else if (state->mbe_file_type == 1)
            {
              readAmbe2450Data (opts, state, ambe_d);
              mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
              processAudio (opts, state);
              if (opts->wav_out_f != NULL)
                {
                  writeSynthesizedVoice (opts, state);
                }
              if (opts->audio_out == 1)
                {
                  playSynthesizedVoice (opts, state);
                }
            }
          if (exitflag == 1)
            {
              cleanupAndExit (opts, state);
            }
        }
    }
}

void
processMbeFrame (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24])
{
  //comment out line below should return print back to normal
  //strncpy (state->err_buf, state->err_str, sizeof(state->err_str));  //right at the very top before err_str gets returned?
  int i;
  char imbe_d[88];
  char ambe_d[49];
  char ambe_d_str[50];
#ifdef AMBE_PACKET_OUT

#endif

  for (i = 0; i < 88; i++)
    {
      imbe_d[i] = 0;
    }


  if ((state->synctype == 0) || (state->synctype == 1))
    {
      //  0 +P25p1
      //  1 -P25p1

      mbe_processImbe7200x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe_fr, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      //mbe_processImbe7200x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe_fr, processed_block, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      if (opts->mbe_out_f != NULL)
        {
          saveImbe4400Data (opts, state, imbe_d);
        }
    }
  else if ((state->synctype == 14) || (state->synctype == 15))
    {
      mbe_processImbe7100x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe7100_fr, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      //mbe_processImbe7100x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe7100_fr, processed_block, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      if (opts->mbe_out_f != NULL)
        {
          saveImbe4400Data (opts, state, imbe_d);
        }
    }
  else if ((state->synctype == 6) || (state->synctype == 7))
    {
      mbe_processAmbe3600x2400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      if (opts->mbe_out_f != NULL)
        {
          saveAmbe2450Data (opts, state, ambe_d);
        }
    }
  else
    {
      //stereo slots and slot 0 (left slot)
      if (state->currentslot == 0 && opts->dmr_stereo == 1)
      {
        mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      }
      //stereo slots and slot 1 (right slot)
      if (state->currentslot == 1 && opts->dmr_stereo == 1)
      {
        mbe_processAmbe3600x2450Framef (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR, ambe_fr, ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);
        //just put this in here for troubleshooting for now
        if (opts->mbe_out_f != NULL)
        {
          saveAmbe2450Data (opts, state, ambe_d);
        }
      }
      if (opts->dmr_stereo == 0)
      {
        mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

        if (opts->mbe_out_f != NULL)
        {
          saveAmbe2450Data (opts, state, ambe_d);
        }
      }


    }

  if (opts->errorbars == 1)
    {
      //state->err_buf = state->err_str; //make comy to compare, and only print when comparison differs?? THIS HERE HERE
      //strncpy (state->err_buf, state->err_str, sizeof(state->err_str)); //is this the correct placement for this? want it just before err_str is set?
      //fprintf (stderr, "%s", state->err_str); //this the actual error 'bar' ==== printer, find way to keep this entire string from printing constantly unless err_str changes
      //fprintf (stderr, "%s", state->err_buf);
    }

  state->debug_audio_errors += state->errs2;
  state->debug_audio_errorsR += state->errs2R;
  if (opts->dmr_stereo == 1 && state->currentslot == 0)
  {
    processAudio (opts, state);
    if (opts->audio_out == 1)
    {
      playSynthesizedVoice (opts, state);
    }
  }
  if (opts->dmr_stereo == 1 && state->currentslot == 1)
  {
    processAudioR (opts, state);
    if (opts->audio_out == 1)
    {
      playSynthesizedVoiceR (opts, state);
    }
  }
  if (opts->dmr_stereo == 0)
  {
    processAudio (opts, state);
    if (opts->audio_out == 1)
    {
      playSynthesizedVoice (opts, state);
    }
  }
  if (opts->wav_out_f != NULL && opts->dmr_stereo == 0)
  {
    writeSynthesizedVoice (opts, state);
  }
  /*
  if (opts->audio_out == 1)
  {
    playSynthesizedVoice (opts, state);
  }
  */
}

/* This function decipher data */
static void DecipherData(char * Input, char * KeyStream, char * Output, int NbData)
{
  int i;

  for(i = 0; i < NbData; i++)
  {
    Output[i] = Input[i] ^ KeyStream[i];
  }
}
