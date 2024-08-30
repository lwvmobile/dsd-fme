//a new and simplified voice handling routine for NXDN based on lich signalling
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

 //Originally found at - https://github.com/LouisErigHerve/dsd
 //Modified for use in DSD-FME

#include "dsd.h"

const int nW[36] = { 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int nX[36] = { 23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

const int nY[36] = { 0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int nZ[36] = { 5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

void nxdn_voice (dsd_opts * opts, dsd_state * state, int voice, uint8_t dbuf[182])
{
  int i;
  int start, stop;
  char ambe_fr[4][24];

  const int *w, *x, *y, *z;

  //these conditions will determine our starting and stopping value for voice
  //i.e. voice in first, voice in second, or voice in both
  if (voice == 1 || voice == 3) start = 0;
  if (voice == 1) stop = 2;
  if (voice == 2) start = 2;
  if (voice == 2 || voice == 3) stop = 4;

  for (; start < stop; start++)
  {
    w = nW;
    x = nX;
    y = nY;
    z = nZ;

    for (i = 0; i < 36; i++)
    {

      //skip 8 lich and 30 sacch dibits already in buffer plus 36 times start position
      ambe_fr[*w][*x] = dbuf[i+38+start*36] >> 1; 
      ambe_fr[*y][*z] = dbuf[i+38+start*36] & 1; 
  
      w++;
      x++;
      y++;
      z++;

    }
    processMbeFrame (opts, state, NULL, ambe_fr, NULL);

    memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
    
    if (opts->floating_point == 0 )
      playSynthesizedVoiceMS(opts, state);
    if (opts->floating_point == 1)
      playSynthesizedVoiceFM(opts, state);
  }
  
  if (opts->payload == 1)
  {
    fprintf(stderr, "\n");
  }
  

}