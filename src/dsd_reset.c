
#include "dsd.h"
#include "p25p1_heuristics.h"

//fixed the memory leak, but now random segfaults occur -- double free or corruption (out) or (!prev)
void resetState (dsd_state * state)
{

  int i;

  //Dibit Buffer -- Free Allocated Memory
  // free (state->dibit_buf);

  //Dibit Buffer -- Memset/Init/Allocate Memory
  // state->dibit_buf = malloc (sizeof (int) * 1000000);
  
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  state->repeat = 0; //repeat frame?

  //Audio Buffer -- Free Allocated Memory
  free (state->audio_out_buf);
  free (state->audio_out_float_buf);
  free (state->audio_out_bufR);
  free (state->audio_out_float_bufR);

  //Audio Buffer -- Memset/Init/Allocate Memory per slot

  //slot 1
  state->audio_out_float_buf = malloc (sizeof (float) * 1000000);
  state->audio_out_buf = malloc (sizeof (short) * 1000000);

  memset (state->audio_out_buf, 0, 100 * sizeof (short));
  state->audio_out_buf_p = state->audio_out_buf + 100;
  
  memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
  state->audio_out_float_buf_p = state->audio_out_float_buf + 100;

  state->audio_out_idx = 0;
  state->audio_out_idx2 = 0;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;

  //slot 2
  state->audio_out_bufR = malloc (sizeof (short) * 1000000);
  state->audio_out_float_bufR = malloc (sizeof (float) * 1000000);

  memset (state->audio_out_bufR, 0, 100 * sizeof (short));
  state->audio_out_buf_pR = state->audio_out_bufR + 100;
  
  memset (state->audio_out_float_bufR, 0, 100 * sizeof (float));
  state->audio_out_float_buf_pR = state->audio_out_float_bufR + 100;

  state->audio_out_idxR = 0;
  state->audio_out_idx2R = 0;
  state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;

  //Sync
  state->center = 0;
  state->jitter = -1;
  state->synctype = -1;
  state->min = -15000;
  state->max = 15000;
  state->lmid = 0;
  state->umid = 0;
  state->minref = -12000;
  state->maxref = 12000;

  state->lastsample = 0;
  for (i = 0; i < 128; i++)
  {
    state->sbuf[i] = 0;
  }
  state->sidx = 0;
  for (i = 0; i < 1024; i++)
  {
    state->maxbuf[i] = 15000;
  }
  for (i = 0; i < 1024; i++)
  {
    state->minbuf[i] = -15000;
  }

  state->midx = 0;
  state->symbolcnt = 0;

  state->numflips = 0;
  state->lastsynctype = -1;
  state->lastp25type = 0;
  state->offset = 0;
  state->carrier = 0;

  //Reset Voice Errors in C0 and C1 (or remaining Codewords in IMBE)
  state->errs = 0;
  state->errs2 = 0;
  state->errsR = 0;
  state->errs2R = 0;

  //Misc -- may not be needed
  state->optind = 0;
  state->numtdulc = 0;
  state->firstframe = 0;

  //unsure if these are still used or ever were used, 
  // memset (state->aout_max_buf, 0, sizeof (float) * 200);
  // state->aout_max_buf_p = state->aout_max_buf;
  // state->aout_max_buf_idx = 0;

  // //MBE Specific 
  // //free the memory before allocating it again -- may not use this
  // free (state->cur_mp);
  // free (state->prev_mp);
  // free (state->prev_mp_enhanced);

  // //memory allocation and init on mbe -- may not use this
  // state->cur_mp = malloc (sizeof (mbe_parms));
  // state->prev_mp = malloc (sizeof (mbe_parms));
  // state->prev_mp_enhanced = malloc (sizeof (mbe_parms));
  // mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);

  //rest the heurestics, we want to do this on each tune, each RF frequency can deviate quite a bit in strength
  initialize_p25_heuristics(&state->p25_heuristics);
  initialize_p25_heuristics(&state->inv_p25_heuristics);
}

//simple function to reset the dibit buffer
void reset_dibit_buffer(dsd_state * state)
{
	//Dibit Buffer -- Free Allocated Memory
  // free (state->dibit_buf);

  //Dibit Buffer -- Memset/Init/Allocate Memory
  // state->dibit_buf = malloc (sizeof (int) * 1000000);

  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
}
