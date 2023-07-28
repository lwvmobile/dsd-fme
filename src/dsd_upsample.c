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

void
upsample (dsd_state * state, float invalue)
{

  int i, j, sum;
  float *outbuf1, c, d;
  if (state->currentslot == 0 && state->dmr_stereo == 1)
  {
    outbuf1 = state->audio_out_float_buf_p;
  }
  if (state->currentslot == 1 && state->dmr_stereo == 1)
  {
    outbuf1 = state->audio_out_float_buf_pR;
  }
  if (state->dmr_stereo == 0)
  {
    outbuf1 = state->audio_out_float_buf_p;
  }
  //outbuf1 = state->audio_out_float_buf_p;
  outbuf1--;
  c = *outbuf1;
  d = invalue;
  // basic triangle interpolation
  outbuf1++;
  *outbuf1 = ((invalue * (float) 0.166) + (c * (float) 0.834));
  outbuf1++;
  *outbuf1 = ((invalue * (float) 0.332) + (c * (float) 0.668));
  outbuf1++;
  *outbuf1 = ((invalue * (float) 0.5) + (c * (float) 0.5));
  outbuf1++;
  *outbuf1 = ((invalue * (float) 0.668) + (c * (float) 0.332));
  outbuf1++;
  *outbuf1 = ((invalue * (float) 0.834) + (c * (float) 0.166));
  outbuf1++;
  *outbuf1 = d;
  outbuf1++;

  //only if enabled, produces crackling/buzzing sounds when input type is not pulse audio (reason still unknown)
  //also, could be improved to provide better audio in the future
  if (state->audio_smoothing == 1) 
  {
    if (state->currentslot == 1 && state->dmr_stereo == 1)
    {
      if (state->audio_out_idx2R > 24)
      {
        // smoothing
        outbuf1 -= 16;
        for (j = 0; j < 4; j++)
        {
            for (i = 0; i < 6; i++)
            {
              sum = 0;
              outbuf1 -= 2;
              sum += (int)*outbuf1;
              outbuf1 += 2;
              sum += (int)*outbuf1;
              outbuf1 += 2;
              sum += (int)*outbuf1;
              outbuf1 -= 2;
              *outbuf1 = (sum / (float) 3);
              outbuf1++;
            }
          outbuf1 -= 8;
        }
      }
    }
    if (state->currentslot == 0 && state->dmr_stereo == 1)
    {
      if (state->audio_out_idx2 > 24)
      {
        // smoothing
        outbuf1 -= 16;
        for (j = 0; j < 4; j++)
          {
            for (i = 0; i < 6; i++)
              {
                sum = 0;
                outbuf1 -= 2;
                sum += (int)*outbuf1;
                outbuf1 += 2;
                sum += (int)*outbuf1;
                outbuf1 += 2;
                sum += (int)*outbuf1;
                outbuf1 -= 2;
                *outbuf1 = (sum / (float) 3);
                outbuf1++;
              }
            outbuf1 -= 8;
          }
      }
    }
    if (state->dmr_stereo == 0)
    {
      if (state->audio_out_idx2 > 24)
      {
        // smoothing
        outbuf1 -= 16;
        for (j = 0; j < 4; j++)
          {
            for (i = 0; i < 6; i++)
              {
                sum = 0;
                outbuf1 -= 2;
                sum += (int)*outbuf1;
                outbuf1 += 2;
                sum += (int)*outbuf1;
                outbuf1 += 2;
                sum += (int)*outbuf1;
                outbuf1 -= 2;
                *outbuf1 = (sum / (float) 3);
                outbuf1++;
              }
            outbuf1 -= 8;
          }
      }
    }
  }

}

//produce 6 short samples (48k) for every 1 short sample (8k)
void upsampleS (short invalue, short prev, short outbuf[6])
{

  float c, d;

  c = prev;
  d = invalue;

  // basic triangle interpolation
  outbuf[0] = c = ((invalue * (float) 0.166) + (c * (float) 0.834));
  outbuf[1] = c = ((invalue * (float) 0.332) + (c * (float) 0.668));
  outbuf[2] = c = ((invalue * (float) 0.500) + (c * (float) 0.5));
  outbuf[3] = c = ((invalue * (float) 0.668) + (c * (float) 0.332));
  outbuf[4] = c = ((invalue * (float) 0.834) + (c * (float) 0.166));
  outbuf[5] = c = d;
  prev = d; //shift prev to the last d value for the next repitition

}