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
dmr_data_sync (dsd_opts * opts, dsd_state * state)
{

  int i, dibit;
  int *dibit_p;
  char sync[25];
  char syncdata[48];
  char cachdata[25];

  char bursttype[5];
  uint8_t burst;
  char info[196];
  unsigned char SlotType[20]; 
  unsigned int SlotTypeOk;
  uint8_t cach_err = 0;

  int cachInterleave[24] = 
  {0, 7, 8, 9, 1, 10,
   11, 12, 2, 13, 14,
   15, 3, 16, 4, 17, 18,
   19, 5, 20, 21, 22, 6, 23
  };

  dibit_p = state->dmr_payload_p - 90;
  
  //collect cach and de-interleave
  for (i = 0; i < 12; i++)
  {
    dibit = *dibit_p;
    dibit_p++;
    if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i];
    }

    cachdata[cachInterleave[(i*2)]]   = (1 & (dibit >> 1)); // bit 1
    cachdata[cachInterleave[(i*2)+1]] = (1 & dibit);       // bit 0

  }

  //seperate tact bits from cach
  uint8_t tact_bits[7];
  for (i = 0; i < 7; i++)
  {
    tact_bits[i] = cachdata[i];
  }

  //run hamming on tact bits
  int cach_okay = -1;
  if ( Hamming_7_4_decode (tact_bits) ) cach_okay = 1;
  else
  {
    cach_okay = -1;
    if (opts->aggressive_framesync == 2)
    {
      SlotTypeOk = 0;
      goto END;
    }
  }

  state->currentslot = tact_bits[1];

  //in the future, maybe we will remove the hard set on this
  if (state->dmr_ms_mode == 1)
  {
    state->currentslot = 0;
  }

  // Current slot - First half - Data Payload - 1st part
  for (i = 0; i < 49; i++)
  {
    dibit = *dibit_p;
    dibit_p++;
    if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+12];
    }
    info[2*i]     = (1 & (dibit >> 1));  // bit 1
    info[(2*i)+1] = (1 & dibit);         // bit 0
  }

  // slot type
  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1)
  {
    dibit = (int)state->dmr_stereo_payload[61]; 
  }

  SlotType[0] = (1 & (dibit >> 1)); // bit 1
  SlotType[1] = (1 & dibit);        // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1)
  {
    dibit = (int)state->dmr_stereo_payload[62];
  }

  SlotType[2] = (1 & (dibit >> 1)); // bit 1
  SlotType[3] = (1 & dibit);        // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1) //state
  {
    dibit = (int)state->dmr_stereo_payload[63]; 
  }

  SlotType[4]  = (1 & (dibit >> 1)); // bit 1
  SlotType[5]  = (1 & dibit);        // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1) //state
  {
    dibit = (int)state->dmr_stereo_payload[64]; 
  }

  SlotType[6]  = (1 & (dibit >> 1)); // bit 1
  SlotType[7]  = (1 & dibit);        // bit 0

  // Parity bit
  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1)
  {
    dibit = (int)state->dmr_stereo_payload[65];
  }
  SlotType[8] = (1 & (dibit >> 1)); // bit 1
  SlotType[9] = (1 & dibit);        // bit 0

  // signaling data or sync
  for (i = 0; i < 24; i++)
  {
    dibit = *dibit_p;
    dibit_p++;
    if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+66]; 
    }
    syncdata[2*i]     = (1 & (dibit >> 1));  // bit 1
    syncdata[(2*i)+1] = (1 & dibit);         // bit 0
    sync[i] = (dibit | 1) + 48;
  }
  sync[24] = 0; 

  if((strcmp (sync, DMR_BS_DATA_SYNC) == 0) )
  {
    if (state->currentslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");
    }
    else
    {
      sprintf(state->slot1light, " slot1 ");
      sprintf(state->slot2light, "[slot2]");
    }
  }

  else if(strcmp (sync, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0)
  {
    state->currentslot = 0;
    sprintf(state->slot1light, "[sLoT1]");
    sprintf(state->slot2light, "[DMODE]");
  }

  else if(strcmp (sync, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0)
  {
    state->currentslot = 1;
    sprintf(state->slot1light, "[DMODE]");
    sprintf(state->slot2light, "[sLoT2]");
  }

  if (state->dmr_ms_mode == 0)
  {
    fprintf(stderr, "%s %s ", state->slot1light, state->slot2light);
  }

  // Slot type - Second part - Parity bit
  for (i = 0; i < 5; i++)
  {
    if (state->dmr_stereo == 0) 
    {
      dibit = getDibit(opts, state);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+90]; 
    }
    SlotType[(i*2) + 10] = (1 & (dibit >> 1)); // bit 1
    SlotType[(i*2) + 11] = (1 & dibit);        // bit 0
  }

  /* Check and correct the SlotType (apply Golay(20,8) FEC check) */
  
  // golay (20,8) hamming-weight of 6 reliably corrects at most 2 bit-errors
  if( Golay_20_8_decode(SlotType) ) SlotTypeOk = 1;
  else
  {
    SlotTypeOk = -1;
    if (opts->aggressive_framesync == 1)
    {
      SlotTypeOk = 0;
      goto END;
    }
  }

  state->color_code = (SlotType[0] << 3) + (SlotType[1] << 2) +(SlotType[2] << 1) + (SlotType[3] << 0);
  state->color_code_ok = SlotTypeOk;

  //not sure why I still have two variables for this, need to look and see what state->color_code still ties into
  if (SlotTypeOk == 1) state->dmr_color_code = state->color_code;

  /* Reconstitute the burst type */
  burst = (uint8_t)((SlotType[4] << 3) + (SlotType[5] << 2) + (SlotType[6] << 1) + SlotType[7]);
  if (state->currentslot == 0) state->dmrburstL = burst;
  if (state->currentslot == 1) state->dmrburstR = burst;


  // Current slot - Second Half - Data Payload - 2nd part
  for (i = 0; i < 49; i++)
  {
    if (state->dmr_stereo == 0) 
    {
      dibit = getDibit(opts, state);
    }

    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+95]; 
    }

    info[(2*i) + 98] = (1 & (dibit >> 1));  // bit 1
    info[(2*i) + 99] = (1 & dibit);         // bit 0
  }
  
  dmr_data_burst_handler(opts, state, (uint8_t *)info, burst);

  //don't run cach on simplex or mono
  if (state->dmr_ms_mode == 0 || opts->dmr_mono == 0)
  {
    cach_err = dmr_cach (opts, state, cachdata);
  } 

  //ending line break
  fprintf(stderr, "\n");

  END:
  if (SlotTypeOk == 0 || cach_okay != 1)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "| CACH/Burst FEC ERR");
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
  }

  // Skip cach (24 bit = 12 dibit) and next slot 1st half (98 + 10 bit = 49 + 5 dibit)
  if (state->dmr_stereo == 0) 
  {
    skipDibit (opts, state, 12 + 49 + 5);
  }

} 
