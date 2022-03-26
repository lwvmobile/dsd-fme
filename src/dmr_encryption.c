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
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrameL = NULL;
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrameR = NULL;

  int *errs;
  int *errs2;
  int *errsR;
  int *errs2R;
  unsigned long long int k;
  k = 0;

  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  //TSVoiceSupFrameL = &state->TS1SuperFrame;
  //TSVoiceSupFrameR = &state->TS2SuperFrame;
//
//
//
  for(Frame = 0; Frame < 6; Frame++)
  {
    /* 1 DMR frame contains 3 AMBE voice samples */
    //fprintf (stderr, "\n 1AMBE ");
    for(i = 0; i < 3; i++)
    {

      errs  = (int*)&(TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs1[i]);
      //fprintf (stderr, "[%02X] ", TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i]);
      errs2 = (int*)&(TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs2[i]);
      state->errs =  TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs1[i]; //correct placement
      state->errs2 = TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].errs2[i]; //correct placement

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, errs, errs2, state->err_str,
                                TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i],
                                state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      /*
      if (state->currentslot == 0)
      {
      errs  = (int*)&(TSVoiceSupFrameL->TimeSlotAmbeVoiceFrame[Frame].errs1[i]);
      errs2 = (int*)&(TSVoiceSupFrameL->TimeSlotAmbeVoiceFrame[Frame].errs2[i]);
      state->errs =  TSVoiceSupFrameL->TimeSlotAmbeVoiceFrame[Frame].errs1[i]; //correct placement
      state->errs2 = TSVoiceSupFrameL->TimeSlotAmbeVoiceFrame[Frame].errs2[i]; //correct placement

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, errs, errs2, state->err_str,
                                TSVoiceSupFrameL->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i],
                                state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      state->debug_audio_errors += *errs2;
      //state->debug_audio_errors += *errs2R;

      processAudio(opts, state);
      playSynthesizedVoice (opts, state);
      }
      if (state->currentslot == 1)
      {
      errsR  = (int*)&(TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].errs1[i]);
      errs2R = (int*)&(TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].errs2[i]);
      state->errs =  TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].errs1[i]; //correct placement
      state->errs2 = TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].errs2[i]; //correct placement
      //state->audio_out_temp_bufR = state->audio_out_temp_buf;

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, errsR, errs2R, state->err_strR,
                                TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i],
                                state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      state->debug_audio_errors += *errs2R;
      processAudio(opts, state);
      playSynthesizedVoice (opts, state);
      }

      mbe_processAmbe2450Dataf (state->audio_out_temp_bufR, errsR, errs2R, state->err_strR,
                                TSVoiceSupFrameR->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i],
                                state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      */

      if (opts->mbe_out_f != NULL)
      {
        saveAmbe2450Data (opts, state, TSVoiceSupFrame->TimeSlotAmbeVoiceFrame[Frame].AmbeBit[i]);
      }
      if (opts->errorbars == 1)
      {
        fprintf(stderr, "%s", state->err_str);
      }

      state->debug_audio_errors += *errs2;
      //state->debug_audio_errors += *errs2R;

      processAudio(opts, state);
      //playSynthesizedVoice (opts, state);

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
