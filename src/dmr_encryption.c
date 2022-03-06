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
#include "dmr_const.h"
#include "p25p1_check_hdu.h"


void ProcessDMREncryption (dsd_opts * opts, dsd_state * state)
{
  uint32_t i, j;
  uint32_t Frame;
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  int *errs;
  int *errs2;

  /*
   * Currently encryption is not supported in this public version...
   */

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  /* Apply encryption here
   *
   * A DMR superframe = 6 frames x 3 AMBE voice sample of 49 bits each
   * uint8_t KeyStream[6][3][49];
   *
   * 1�) Initialize the "KeyStream" buffer (total 882 bits) with correct bits (depending of
   *     encryption mode used, MotoTRBO BP, Hytera BP, MotoTRBO EP, MotoTRBO AES...
   *
   * 2�) Apply a XOR between "KeyStream" and "TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i][j]"
   *     like code :
   *     for(Frame = 0; Frame < 6; Frame++)
   *     {
   *       for(i = 0; i < 3; i++)
   *       {
   *         for(j = 0; j < 49; j++)
   *         {
   *           TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i] ^= KeyStream[Frame][i][j];
   *         }
   *       }
   *     }
   *
   * 3�) Play all AMBE decoded sample (see the "for" loop below)
   *
   * */

  /*
   * Play all AMBE voice samples
   * 1 DMR voice superframe = 6 DMR frames */
  for(Frame = 0; Frame < 6; Frame++)
  {
    /* 1 DMR frame contains 3 AMBE voice samples */
    for(i = 0; i < 3; i++)
    {
      errs  = (int*)&(TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs1[i]);
      errs2 = (int*)&(TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs2[i]);
      state->errs =  TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs1[i]; //correct placement
      state->errs2 = TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs2[i]; //correct placement

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, errs, errs2, state->err_str,
                                TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i],
                                state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      if (opts->mbe_out_f != NULL)
      {
        saveAmbe2450Data (opts, state, TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i]);
      }
      if (opts->errorbars == 1)
      {
        fprintf(stderr, "%s", state->err_str);
      }

      state->debug_audio_errors += *errs2;

      processAudio(opts, state);

      if (opts->wav_out_f != NULL)
      {
        writeSynthesizedVoice (opts, state);
      }

      if (opts->audio_out == 1 && opts->p25enc != 1) //play only if not equal to enc value, going to set p25enc to 1 if enc detected??
      {
        playSynthesizedVoice (opts, state);
      }
    } /* End for(i = 0; i < 3; i++) */
  } /* End for(Frame = 0; Frame < 6; Frame++) */
} /* End ProcessDMREncryption() */



/* End of file */
