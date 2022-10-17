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
processDMRdata (dsd_opts * opts, dsd_state * state)
{

  int i, dibit;
  int *dibit_p;
  char sync[25];
  char syncdata[48];
  char cachdata[25] = {0}; 
  char cc[5] = {0};
  char ccAscii[5] = {0};
  char bursttype[5];
  unsigned int burst;
  char info[196];

  unsigned char SlotType[20]; 
  unsigned int SlotTypeOk;
  int cachInterleave[24]   = {0, 7, 8, 9, 1, 10, 11, 12, 2, 13, 14, 15, 3, 16, 4, 17, 18, 19, 5, 20, 21, 22, 6, 23};

#ifdef DMR_DUMP
  char syncbits[49];
  char cachbits[25];
  int k;
#endif

  ccAscii[4] = 0;
  bursttype[4] = 0;

  dibit_p = state->dmr_payload_p - 90;
  // CACH
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

  cachdata[25] = 0; 

  //decode and correct cach and compare
  int cach_okay = 69;
  if ( Hamming_7_4_decode (cachdata) ) //is now de-interleaved appropriately
  {
    cach_okay = 1;
  }
  if (cach_okay == 1)
  {
    //fprintf (stderr, "CACH Okay ");
  }
  if (cach_okay != 1)
  {
    //fprintf (stderr, "CACH FEC Error %d", cach_okay);
    goto END;
  }

  state->currentslot = cachdata[1]; //still bit 2, or somewhere else? is it 8? 1? TC?

  if (state->currentslot == 0 && state->dmr_ms_mode == 0)
  {
    state->slot1light[0] = '[';
    state->slot1light[6] = ']';
    state->slot2light[0] = ' ';
    state->slot2light[6] = ' ';
  }
  //else
  if (state->currentslot == 1 && state->dmr_ms_mode == 0)
  {
    state->slot2light[0] = '[';
    state->slot2light[6] = ']';
    state->slot1light[0] = ' ';
    state->slot1light[6] = ' ';
  }
  //end correct cach and set slot

#ifdef DMR_DUMP
  k = 0;
  for (i = 0; i < 12; i++)
  {
    dibit = cachdata[i];
    cachbits[k] = (1 & (dibit >> 1)) + 48;    // bit 1
    k++;
    cachbits[k] = (1 & dibit) + 48;   // bit 0
    k++;
  }
  cachbits[24] = 0;
  fprintf(stderr, "%s ", cachbits);
#endif
  //trellis bits
  unsigned char trellisdibits[98];
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
    trellisdibits[i] = dibit;
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
    dibit = (int)state->dmr_stereo_payload[61]; //61, not i+61
  }
  cc[0] = (1 & (dibit >> 1)); // bit 1
  cc[1] = (1 & dibit);        // bit 0
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
  cc[2] = (1 & (dibit >> 1)); // bit 1
  cc[3] = (1 & dibit);        // bit 0
  SlotType[2] = (1 & (dibit >> 1)); // bit 1
  SlotType[3] = (1 & dibit);        // bit 0

  ccAscii[0] = cc[0] + '0';
  ccAscii[1] = cc[1] + '0';
  ccAscii[2] = cc[2] + '0';
  ccAscii[3] = cc[3] + '0';
  ccAscii[4] = cc[4] + '\0';

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
  {
    dibit = (dibit ^ 2);
  }
  if (state->dmr_stereo == 1) //state
  {
    dibit = (int)state->dmr_stereo_payload[63]; //(int)
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
    dibit = (int)state->dmr_stereo_payload[64]; //(int)
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

  // signalling data or sync
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

#ifdef DMR_DUMP
  k = 0;
  for (i = 0; i < 24; i++)
  {
    syncbits[k] = syncdata[(k*2)] + 48;    // bit 1
    k++;
    syncbits[k] = syncdata[(k*2)+1] + 48;   // bit 0
    k++;
  }
  syncbits[48] = 0;
  fprintf(stderr, "%s ", syncbits);
#endif

  if((strcmp (sync, DMR_BS_DATA_SYNC) == 0) )//|| (strcmp (sync, DMR_MS_DATA_SYNC) == 0))
  {
    if (state->currentslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
    }
    else
    {
      sprintf(state->slot2light, "[slot2]");
    }
  }

  else if(strcmp (sync, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0)
  {
    state->currentslot = 0;
    sprintf(state->slot1light, "[sLoT1]");
  }

  else if(strcmp (sync, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0)
  {
    state->currentslot = 1;
    sprintf(state->slot2light, "[sLoT2]");
  }

  if (opts->errorbars == 1 && state->dmr_ms_mode == 0)
  {
    fprintf(stderr, "%s %s ", state->slot1light, state->slot2light);
  }

  // Slot type - Second part - Parity bit
  for (i = 0; i < 5; i++)
  {
    if (state->dmr_stereo == 0) //only get dibits if not using dmr_stereo method
    {
      dibit = getDibit(opts, state);
    }
    if (opts->inverted_dmr == 1)
    {
      //dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+90]; //double checked these i+ values make sure we are on the right ones
    }
    SlotType[(i*2) + 10] = (1 & (dibit >> 1)); // bit 1
    SlotType[(i*2) + 11] = (1 & dibit);        // bit 0
  }

  /* Check and correct the SlotType (apply Golay(20,8) FEC check) */

  // golay (20,8) hamming-weight of 6 reliably corrects at most 2 bit-errors
  if( Golay_20_8_decode(SlotType) )
  {
    SlotTypeOk = 1;
    //fprintf (stderr, "Slot Okay ");
  }
  else
  {
    SlotTypeOk = 0;
    goto END;
  }

  /* Slot Type parity checked => Fill the color code */
  state->color_code = (SlotType[0] << 3) + (SlotType[1] << 2) +(SlotType[2] << 1) + (SlotType[3] << 0);
  state->color_code_ok = SlotTypeOk;

  /* Reconstitute the burst type */
  //consider assigning this only when slottypeok or similar check passes, eliminate bad burst types from decoding randomly
  burst = (unsigned int)((SlotType[4] << 3) + (SlotType[5] << 2) + (SlotType[6] << 1) + SlotType[7]);
  if (state->currentslot == 0)
  {
    state->dmrburstL = burst;
  }
  if (state->currentslot == 1)
  {
    state->dmrburstR = burst;
  }

  /* Reconstitute the burst type */
  bursttype[0] = SlotType[4] + '0';
  bursttype[1] = SlotType[5] + '0';
  bursttype[2] = SlotType[6] + '0';
  bursttype[3] = SlotType[7] + '0';
  bursttype[4] = '\0';

  if (strcmp (bursttype, "0000") == 0)
  {
    sprintf(state->fsubtype, " PI Header    ");
  }
  else if (strcmp (bursttype, "0001") == 0)
  {
    sprintf(state->fsubtype, " VOICE LC Header ");
  }
  else if (strcmp (bursttype, "0010") == 0)
  {
    sprintf(state->fsubtype, " TLC          ");
  }
  else if (strcmp (bursttype, "0011") == 0)
  {
    sprintf(state->fsubtype, " CSBK         ");
  }
  else if (strcmp (bursttype, "0100") == 0)
  {
    sprintf(state->fsubtype, " MBC Header   ");
  }
  else if (strcmp (bursttype, "0101") == 0)
  {
    sprintf(state->fsubtype, " MBC          ");
  }
  else if (strcmp (bursttype, "0110") == 0)
  {
    sprintf(state->fsubtype, " DATA Header  ");
  }
  else if (strcmp (bursttype, "0111") == 0)
  {
    sprintf(state->fsubtype, " RATE 1/2 DATA");
  }
  else if (strcmp (bursttype, "1000") == 0)
  {
    sprintf(state->fsubtype, " RATE 3/4 DATA");
  }
  else if (strcmp (bursttype, "1001") == 0)
  {
    sprintf(state->fsubtype, " Slot idle    ");
  }
  else if (strcmp (bursttype, "1010") == 0)
  {
    sprintf(state->fsubtype, " Rate 1 DATA  ");
  }
  else
  {
    sprintf(state->fsubtype, "              ");
  }

  // Current slot - First half - Data Payload - 1st part
  //trellis
  for (i = 0; i < 49; i++)
  {
    if (state->dmr_stereo == 0) //only get dibits if not using dmr_stereo method
    {
      dibit = getDibit(opts, state);
    }
    if (opts->inverted_dmr == 1)
    {
      //dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+95]; //double checked these i+ values make sure we are on the right ones
    }
    trellisdibits[i+49] = dibit;
    info[(2*i) + 98] = (1 & (dibit >> 1));  // bit 1
    info[(2*i) + 99] = (1 & dibit);         // bit 0
  }

  // Skip cach (24 bit = 12 dibit) and next slot 1st half (98 + 10 bit = 49 + 5 dibit)
  if (state->dmr_stereo == 0)
  {
    skipDibit (opts, state, 12 + 49 + 5);
  }

  if (opts->errorbars == 1)
  {
    /* Print the color code */
    //if (burst == 0b1001 && state->color_code_ok) //only print on idle for data types, otherwise prints on voice frames.
    if (SlotTypeOk == 1)
    {
      fprintf (stderr, "%s", KCYN);
      fprintf(stderr, "| Color Code=%02d ", (int)state->color_code);
      fprintf (stderr, "%s", KNRM);
      //state->dmr_color_code = state->color_code;
    }
    else fprintf(stderr, "|               ");

    //if(state->color_code_ok) fprintf(stderr, "(OK)      |");
    if(state->color_code_ok) fprintf(stderr, "| (CRC OK ) |"); //add line break
    else
    {
      fprintf (stderr, "%s", KRED);
      fprintf(stderr, "| (CRC ERR) |");
      fprintf (stderr, "%s", KNRM);
    }
    //fprintf (stderr, "\n"); //print line break
    if (strcmp (state->fsubtype, "              ") == 0)
    {
      //fprintf(stderr, " Unknown burst type: %s", bursttype);
      fprintf(stderr, " Unknown");
    }
    else
    {
      fprintf(stderr, "%s", state->fsubtype);
    }
  }

  switch(burst)
  {
    /* Burst = PI header */
    case 0b0000:
    {
      ProcessDmrPIHeader(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = Voice LC header */
    case 0b0001:
    {
      /* Extract data from Voice LC Header */
      ProcessDmrVoiceLcHeader(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = TLC */
    case 0b0010:
    {
      //Not sure if I really care about this in the context of what it shows
      ProcessDmrTerminaisonLC(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = CSBK */
    case 0b0011:
    {
      //going to disable until I can find more useful info to present
      ProcessCSBK(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = MBC Header */
    case 0b0100:
    {
      ProcessMBChData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = MBC Continuation*/
    case 0b0101:
    {
      ProcessMBCData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = DATA Header */
    case 0b0110:
    {
      ProcessDataData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = RATE 1/2 DATA */
    case 0b0111:
    {
      Process12Data(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = RATE 3/4 DATA */
    case 0b1000:
    {
      //need to keep working on improving the trellis decode, but is partially viable now
      Process34Data(opts, state, trellisdibits, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = Slot idle */
    case 0b1001:
    {
      if(state->color_code_ok && state->dmr_stereo == 0) state->dmr_color_code = state->color_code; //try setting this on idle if crc ok
      break;
    }

    /* Burst = Rate 1 DATA */
    case 0b1010:
    {
      Process1Data(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = Unified Single Block DATA */
    case 0b1101:
    {
      ProcessUnifiedData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Default */
    default:
    {
      /* Nothing to do */
      ProcessReservedData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }
  } /* End switch(burst) */

  if (opts->errorbars == 1)
  {
    fprintf(stderr, "\n");
  }
  END:
  if (SlotTypeOk == 0 || cach_okay == 0)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "| **CACH or Burst Type FEC ERR ** ");
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
  }

} /* End processDMRdata() */
