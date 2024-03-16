/*-------------------------------------------------------------------------------
 * dsd_upsample.c
 * Simplified 8k to 48k Upsample Functions
 * Goodbye terrible ringing sound
 * 
 * 
 *
 * LWVMOBILE
 * 2024-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//produce 6 short samples (48k) for every 1 short sample (8k)
void upsampleS (short invalue, short prev, short outbuf[6])
{
  UNUSED(prev);
  for (int i = 0; i < 6; i++)
    outbuf[i] = invalue;
}

//legacy 6x simplified version for the float_buf_p
void upsample (dsd_state * state, float invalue)
{

  float *outbuf1;
  if (state->currentslot == 0 && state->dmr_stereo == 1)
    outbuf1 = state->audio_out_float_buf_p;

  else if (state->currentslot == 1 && state->dmr_stereo == 1)
    outbuf1 = state->audio_out_float_buf_pR;

  else if (state->dmr_stereo == 0)
    outbuf1 = state->audio_out_float_buf_p;

  *outbuf1 = invalue;
  outbuf1++;
  *outbuf1 = invalue;
  outbuf1++;
  *outbuf1 = invalue;
  outbuf1++;
  *outbuf1 = invalue;
  outbuf1++;
  *outbuf1 = invalue;
  outbuf1++;
  *outbuf1 = invalue;
}