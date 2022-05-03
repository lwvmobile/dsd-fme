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
  char cachdata[13] = {0};
  char cc[5] = {0};
  char ccAscii[5] = {0};
  char bursttype[5];
  unsigned int burst;
  char info[196];
  char SlotType[20];
  unsigned int SlotTypeOk;

  /* Remove warning compiler */
  //UNUSED_VARIABLE(cachdata[0]);
  //UNUSED_VARIABLE(cc[0]);
  //UNUSED_VARIABLE(ccAscii[0]);

#ifdef DMR_DUMP
  char syncbits[49];
  char cachbits[25];
  int k;
#endif

  ccAscii[4] = 0;
  bursttype[4] = 0;

  dibit_p = state->dibit_buf_p - 90;
  //using the estimate_symbol method for the dmr_payload_p buffer causes sync
  //issues with P25, so only do it when frame_p25p1 == 0, or -fr option
  if (opts->frame_p25p1 == 0) //opts->frame_p25p1 == 0
  {
    dibit_p = state->dmr_payload_p - 90;
  }

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
    //fprintf(stderr, "%X", dibit);
    cachdata[i] = dibit;
    if (i == 2)
    {
      /* Change the current slot only if we are in relayed mode (not in direct mode) */
      if(state->directmode == 0) state->currentslot = (1 & (dibit >> 1));      // bit 1

      if (state->currentslot == 0)
      {
        state->slot1light[0] = '[';
        state->slot1light[6] = ']';
        state->slot2light[0] = ' ';
        state->slot2light[6] = ' ';
      }
      else
      {
        state->slot2light[0] = '[';
        state->slot2light[6] = ']';
        state->slot1light[0] = ' ';
        state->slot1light[6] = ' ';
      }
      //method to only see data from one slot or the other when data occupies both slots or gets cooked by sync
      state->hardslot = 9; //0 to only listen to slot 0 voice, 1 for slot 1, 9 for both
      if(state->hardslot != 9 && state->hardslot != state->currentslot)
      //if(1==1)
      {
        fprintf (stderr, " Current Slot = %d", state->currentslot + 1);
        fprintf (stderr, "\n"); //line break after breaking out of jail
        //goto JUMP;
      }
    }
  }
  cachdata[12] = 0;

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
//  burst  = (unsigned int)((1 & (dibit >> 1)) << 3);
//  burst |= (unsigned int)((1 & (dibit >> 0)) << 2);
//  bursttype[0] = (1 & (dibit >> 1)) + 48;  // bit 1
//  bursttype[1] = (1 & dibit) + 48;         // bit 0
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
//  burst |= (unsigned int)((1 & (dibit >> 1)) << 1);
//  burst |= (unsigned int)((1 & (dibit >> 0)) << 0);
//  bursttype[2] = (1 & (dibit >> 1)) + 48;  // bit 1
//  bursttype[3] = (1 & dibit) + 48;         // bit 0
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
      dibit = (int)state->dmr_stereo_payload[i+66]; //double check these i+ values make sure we are on the right ones
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

  if((strcmp (sync, DMR_BS_DATA_SYNC) == 0) || (strcmp (sync, DMR_MS_DATA_SYNC) == 0))
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

  if (opts->errorbars == 1)
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
      dibit = (dibit ^ 2);
    }
    if (state->dmr_stereo == 1)
    {
      dibit = (int)state->dmr_stereo_payload[i+90]; //double checked these i+ values make sure we are on the right ones
    }
    SlotType[(i*2) + 10] = (1 & (dibit >> 1)); // bit 1
    SlotType[(i*2) + 11] = (1 & dibit);        // bit 0
  }

  /* Check and correct the SlotType (apply Golay(20,8) FEC check) */
  if(Golay_20_8_decode((unsigned char *)SlotType)) SlotTypeOk = 1;
  else SlotTypeOk = 0;

  /* Slot Type parity checked => Fill the color code */
  //state->color_code = (unsigned int)((cc[0] << 3) + (cc[1] << 2) + (cc[2] << 1) + cc[3]);
  state->color_code = (SlotType[0] << 3) + (SlotType[1] << 2) +(SlotType[2] << 1) + (SlotType[3] << 0);
  state->color_code_ok = SlotTypeOk;
  //fprintf(stderr, "| Color Code=%02d ", (int)state->color_code);

  /* Reconstitute the burst type */
  //consider assigning thsi only when slottypeok or similar check passes, eliminate bad burst types from decoding randomly
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

  /* //delete this chunk later
  if (state->dmr_stereo == 3) //may not need this any longer
  {
    burst = (state->dmr_stereo_payload[63] << 2) | (state->dmr_stereo_payload[64]);
    //this whole split for getting burst and burst type is makes no sense
    //why not just use burst
    bursttype[0] = (((state->dmr_stereo_payload[63] >> 1) & 1) + '0');
    bursttype[1] = (((state->dmr_stereo_payload[63] >> 0) & 1) + '0');
    bursttype[2] = (((state->dmr_stereo_payload[64] >> 1) & 1) + '0');
    bursttype[3] = (((state->dmr_stereo_payload[64] >> 0) & 1) + '0');
    bursttype[4] = '\0';
    //fprintf(stderr, "BURST1 = %04b ", burst);
  }
  //figure out why the sync is wrong when dumping the payload to processDMRdata for MS Data
  //I think its a conversion issue with the payload(chat) from the dibit buffer (int *)
  //actually, its the dibit buffer storing as 1 and 3 only (GFSK) when sync drops
  //made a seperate dmr buffer, but still, when sync loss and regain, takes a few syncs to calibrate center/umid/lmid properly
  //getting accurate MS data AND leading BS data may be impossible without saving the symbols, calibrating, then going back
  //and running the buffer payload, but I honestly don't know right now, probably easier to start from stratch than fix this
  if (state->dmr_ms_mode == 7)
  {
    burst = (unsigned int)((SlotType[4] << 3) | (SlotType[5] << 2) | (SlotType[6] << 1) | SlotType[7]);
    bursttype[0] = SlotType[4] + '0';
    bursttype[1] = SlotType[5] + '0';
    bursttype[2] = SlotType[6] + '0';
    bursttype[3] = SlotType[7] + '0';
    bursttype[4] = '\0';
    //fprintf(stderr, "BURST1 = %04b ", burst);
  }

  if (state->dmr_stereo == 3)
  {
    fprintf(stderr, "BURST2 = %b%b%b%b Slot Dump ", SlotType[4], SlotType[5], SlotType[6], SlotType[7] );
    fprintf(stderr, " Slot Dump = ");
    for (i = 0; i < 20; i++)
    {
      fprintf(stderr, "%b", SlotType[i]);
    }
    fprintf(stderr, "\n ");
  }
  */

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
      dibit = (dibit ^ 2);
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
      //ProcessMBChData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = MBC Continuation*/
    case 0b0101:
    {
      //ProcessMBCData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
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
      if(state->color_code_ok) state->dmr_color_code = state->color_code; //try setting this on idle if crc ok
      break;
    }

    /* Burst = Rate 1 DATA */
    case 0b1010:
    {
      //Process1Data(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Burst = Unified Single Block DATA */
    case 0b1101:
    {
      //ProcessUnifiedData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }

    /* Default */
    default:
    {
      /* Nothing to do */
      //ProcessWTFData(opts, state, (uint8_t *)info, (uint8_t *)syncdata, (uint8_t *)SlotType);
      break;
    }
  } /* End switch(burst) */

  if (opts->errorbars == 1)
  {
    fprintf(stderr, "\n");
  }
  //JUMP:
} /* End processDMRdata() */
