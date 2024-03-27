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
#include "p25p1_const.h"

#include "p25p1_ldu.h"
#include "p25p1_check_ldu.h"
#include "p25p1_hdu.h"


void
processLDU1 (dsd_opts* opts, dsd_state* state)
{
  // extracts IMBE frames from LDU frame
  int i;
  uint8_t lcformat[9], mfid[9], lcinfo[57];
  char lsd1[9], lsd2[9];
  uint8_t lsd_hex1, lsd_hex2; uint8_t lowspeeddata[32]; memset (lowspeeddata, 0, sizeof(lowspeeddata));
  int status_count; int lsd1_okay, lsd2_okay = 0;

  char hex_data[12][6];    // Data in hex-words (6 bit words). A total of 12 hex words.
  char hex_parity[12][6];  // Parity of the data, again in hex-word format. A total of 12 parity hex words.

  int irrecoverable_errors;

  AnalogSignal analog_signal_array[12*(3+2)+12*(3+2)];
  int analog_signal_index;


  analog_signal_index = 0;

  // we skip the status dibits that occur every 36 symbols
  // the first IMBE frame starts 14 symbols before next status
  // so we start counter at 36-14-1 = 21
  status_count = 21;

  if (opts->errorbars == 1)
    {
      //fprintf (stderr, "e:");
    }

  // IMBE 1
#ifdef TRACE_DSD
  state->debug_prefix_2 = '0';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // IMBE 2
#ifdef TRACE_DSD
  state->debug_prefix_2 = '1';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 2
  read_and_correct_hex_word (opts, state, &(hex_data[11][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[10][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 9][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 8][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[0*5].sequence_broken = 1;

  // IMBE 3
#ifdef TRACE_DSD
  state->debug_prefix_2 = '2';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 3
  read_and_correct_hex_word (opts, state, &(hex_data[ 7][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 6][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 5][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 4][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[4*5].sequence_broken = 1;

  // IMBE 4
#ifdef TRACE_DSD
  state->debug_prefix_2 = '3';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 4
  read_and_correct_hex_word (opts, state, &(hex_data[ 3][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 2][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 1][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_data[ 0][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[8*5].sequence_broken = 1;

  // IMBE 5
#ifdef TRACE_DSD
  state->debug_prefix_2 = '4';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 5
  read_and_correct_hex_word (opts, state, &(hex_parity[11][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[10][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 9][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 8][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[12*5].sequence_broken = 1;

  // IMBE 6
#ifdef TRACE_DSD
  state->debug_prefix_2 = '5';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 6
  read_and_correct_hex_word (opts, state, &(hex_parity[ 7][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 6][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 5][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 4][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[16*5].sequence_broken = 1;

  // IMBE 7
#ifdef TRACE_DSD
  state->debug_prefix_2 = '6';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 7
  read_and_correct_hex_word (opts, state, &(hex_parity[ 3][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 2][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 1][0]), &status_count, analog_signal_array, &analog_signal_index);
  read_and_correct_hex_word (opts, state, &(hex_parity[ 0][0]), &status_count, analog_signal_array, &analog_signal_index);
  analog_signal_array[20*5].sequence_broken = 1;

  // IMBE 8
#ifdef TRACE_DSD
  state->debug_prefix_2 = '7';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  // Read data after IMBE 8: LSD (low speed data)
  {
    char lsd[8];
    char cyclic_parity[8];

    for (i=0; i<=6; i+=2)
      {
        read_dibit(opts, state, lsd+i, &status_count, NULL, NULL);
      }
    for (i=0; i<=6; i+=2)
      {
        read_dibit(opts, state, cyclic_parity+i, &status_count, NULL, NULL);
      }
      lsd_hex1 = 0;
    for (i=0; i<8; i++)
      {
        lsd_hex1 = lsd_hex1 << 1;
        lsd1[i] = lsd[i] + '0';
        lsd_hex1 |= (uint8_t)lsd[i];
        lowspeeddata[i+0] = (uint8_t)lsd1[i];
        lowspeeddata[i+8] = (uint8_t)cyclic_parity[i];
      }

    for (i=0; i<=6; i+=2)
      {
        read_dibit(opts, state, lsd+i, &status_count, NULL, NULL);
      }
    for (i=0; i<=6; i+=2)
      {
        read_dibit(opts, state, cyclic_parity+i, &status_count, NULL, NULL);
      }
      lsd_hex2 = 0;
    for (i=0; i<8; i++)
      {
        lsd_hex2 = lsd_hex2 << 1;
        lsd2[i] = lsd[i] + '0';
        lsd_hex2 |= (uint8_t)lsd[i];
        lowspeeddata[i+16] = (uint8_t)lsd2[i];
        lowspeeddata[i+24] = (uint8_t)cyclic_parity[i];
      }

    // TODO: error correction of the LSD bytes... <--THIS!
    // TODO: do something useful with the LSD bytes... <--THIS!

    state->dropL += 2; //need to skip 2 here for the LSD bytes
  }

  // IMBE 9
#ifdef TRACE_DSD
  state->debug_prefix_2 = '8';
#endif
  process_IMBE (opts, state, &status_count);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 0 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceSS(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 1)
    playSynthesizedVoiceFM(opts, state);
  if (opts->floating_point == 1 && opts->pulse_digi_out_channels == 2)
    playSynthesizedVoiceFS(opts, state);

  if (opts->errorbars == 1)
    {
      fprintf (stderr, "\n");
    }

  if (opts->p25status == 1)
    {
      fprintf (stderr, "lsd1: %s lsd2: %s\n", lsd1, lsd2);
    }

  // trailing status symbol
  {
      int status;
      status = getDibit (opts, state) + '0';
      // TODO: do something useful with the status bits...
      UNUSED(status);
  }

  // Error correct the hex_data using Reed-Solomon hex_parity
  irrecoverable_errors = check_and_fix_reedsolomon_24_12_13((char*)hex_data, (char*)hex_parity);
  if (irrecoverable_errors == 1)
    {
      state->debug_header_critical_errors++;

      // We can correct (13-1)/2 = 6 errors. If we failed, it means that there were more than 6 errors in
      // these 12+12 words. But take into account that each hex word was already error corrected with
      // Hamming(10,6,3), which can correct 1 bits on each sequence of (6+4) bits. We could say that there
      // were 7 errors of 2 bits.
      update_error_stats(&state->p25_heuristics, 12*6+12*6, 7*2);
    }
  else
    {
      // Same comments as in processHDU. See there.

      char fixed_parity[12*6];

      // Correct the dibits that we read according with hex_data values
      correct_hamming_dibits((char*)hex_data, 12, analog_signal_array);

      // Generate again the Reed-Solomon parity
      encode_reedsolomon_24_12_13((char*)hex_data, fixed_parity);

      // Correct the dibits that we read according with the fixed parity values
      correct_hamming_dibits(fixed_parity, 12, analog_signal_array+12*(3+2));

      // Once corrected, contribute this information to the heuristics module
      contribute_to_heuristics(state->rf_mod, &(state->p25_heuristics), analog_signal_array, 12*(3+2)+12*(3+2));
    }

#ifdef HEURISTICS_DEBUG
  fprintf (stderr, "(audio errors, header errors, critical header errors) (%i,%i,%i)\n",
          state->debug_audio_errors, state->debug_header_errors, state->debug_header_critical_errors);
#endif

  // Now put the corrected data into the DSD structures

  lcformat[8] = 0;
  mfid[8] = 0;
  lcinfo[56] = 0;
  lsd1[8] = 0;
  lsd2[8] = 0;

  lcformat[0] = hex_data[11][0] + '0';
  lcformat[1] = hex_data[11][1] + '0';
  lcformat[2] = hex_data[11][2] + '0';
  lcformat[3] = hex_data[11][3] + '0';
  lcformat[4] = hex_data[11][4] + '0';
  lcformat[5] = hex_data[11][5] + '0';

  lcformat[6] = hex_data[10][0] + '0';
  lcformat[7] = hex_data[10][1] + '0';
  mfid[0]     = hex_data[10][2] + '0';
  mfid[1]     = hex_data[10][3] + '0';
  mfid[2]     = hex_data[10][4] + '0';
  mfid[3]     = hex_data[10][5] + '0';

  mfid[4]     = hex_data[ 9][0] + '0';
  mfid[5]     = hex_data[ 9][1] + '0';
  mfid[6]     = hex_data[ 9][2] + '0';
  mfid[7]     = hex_data[ 9][3] + '0';
  lcinfo[0]   = hex_data[ 9][4] + '0';
  lcinfo[1]   = hex_data[ 9][5] + '0';

  lcinfo[2]   = hex_data[ 8][0] + '0';
  lcinfo[3]   = hex_data[ 8][1] + '0';
  lcinfo[4]   = hex_data[ 8][2] + '0';
  lcinfo[5]   = hex_data[ 8][3] + '0';
  lcinfo[6]   = hex_data[ 8][4] + '0';
  lcinfo[7]   = hex_data[ 8][5] + '0';

  lcinfo[8]   = hex_data[ 7][0] + '0';
  lcinfo[9]   = hex_data[ 7][1] + '0';
  lcinfo[10]  = hex_data[ 7][2] + '0';
  lcinfo[11]  = hex_data[ 7][3] + '0';
  lcinfo[12]  = hex_data[ 7][4] + '0';
  lcinfo[13]  = hex_data[ 7][5] + '0';

  lcinfo[14]  = hex_data[ 6][0] + '0';
  lcinfo[15]  = hex_data[ 6][1] + '0';
  lcinfo[16]  = hex_data[ 6][2] + '0';
  lcinfo[17]  = hex_data[ 6][3] + '0';
  lcinfo[18]  = hex_data[ 6][4] + '0';
  lcinfo[19]  = hex_data[ 6][5] + '0';

  lcinfo[20]  = hex_data[ 5][0] + '0';
  lcinfo[21]  = hex_data[ 5][1] + '0';
  lcinfo[22]  = hex_data[ 5][2] + '0';
  lcinfo[23]  = hex_data[ 5][3] + '0';
  lcinfo[24]  = hex_data[ 5][4] + '0';
  lcinfo[25]  = hex_data[ 5][5] + '0';

  lcinfo[26]  = hex_data[ 4][0] + '0';
  lcinfo[27]  = hex_data[ 4][1] + '0';
  lcinfo[28]  = hex_data[ 4][2] + '0';
  lcinfo[29]  = hex_data[ 4][3] + '0';
  lcinfo[30]  = hex_data[ 4][4] + '0';
  lcinfo[31]  = hex_data[ 4][5] + '0';

  lcinfo[32]  = hex_data[ 3][0] + '0';
  lcinfo[33]  = hex_data[ 3][1] + '0';
  lcinfo[34]  = hex_data[ 3][2] + '0';
  lcinfo[35]  = hex_data[ 3][3] + '0';
  lcinfo[36]  = hex_data[ 3][4] + '0';
  lcinfo[37]  = hex_data[ 3][5] + '0';

  lcinfo[38]  = hex_data[ 2][0] + '0';
  lcinfo[39]  = hex_data[ 2][1] + '0';
  lcinfo[40]  = hex_data[ 2][2] + '0';
  lcinfo[41]  = hex_data[ 2][3] + '0';
  lcinfo[42]  = hex_data[ 2][4] + '0';
  lcinfo[43]  = hex_data[ 2][5] + '0';

  lcinfo[44]  = hex_data[ 1][0] + '0';
  lcinfo[45]  = hex_data[ 1][1] + '0';
  lcinfo[46]  = hex_data[ 1][2] + '0';
  lcinfo[47]  = hex_data[ 1][3] + '0';
  lcinfo[48]  = hex_data[ 1][4] + '0';
  lcinfo[49]  = hex_data[ 1][5] + '0';

  lcinfo[50]  = hex_data[ 0][0] + '0';
  lcinfo[51]  = hex_data[ 0][1] + '0';
  lcinfo[52]  = hex_data[ 0][2] + '0';
  lcinfo[53]  = hex_data[ 0][3] + '0';
  lcinfo[54]  = hex_data[ 0][4] + '0';
  lcinfo[55]  = hex_data[ 0][5] + '0';

  int j;
  uint8_t LCW_bytes[9];
  uint8_t LCW_bits[72];
  memset (LCW_bytes, 0, sizeof(LCW_bytes));
  memset (LCW_bits, 0, sizeof(LCW_bits));

  //load array above into byte form first
  LCW_bytes[0] = (uint8_t) ConvertBitIntoBytes(&lcformat[0], 8);
  LCW_bytes[1] = (uint8_t) ConvertBitIntoBytes(&mfid[0], 8);
  for (i = 0; i < 7; i++) LCW_bytes[i+2] = (uint8_t) ConvertBitIntoBytes(&lcinfo[i*8], 8);

  //load as bit array next (easier to process data in bit form)
  for(i = 0, j = 0; i < 9; i++, j+=8)
  {
    LCW_bits[j + 0] = (LCW_bytes[i] >> 7) & 0x01;
    LCW_bits[j + 1] = (LCW_bytes[i] >> 6) & 0x01;
    LCW_bits[j + 2] = (LCW_bytes[i] >> 5) & 0x01;
    LCW_bits[j + 3] = (LCW_bytes[i] >> 4) & 0x01;
    LCW_bits[j + 4] = (LCW_bytes[i] >> 3) & 0x01;
    LCW_bits[j + 5] = (LCW_bytes[i] >> 2) & 0x01;
    LCW_bits[j + 6] = (LCW_bytes[i] >> 1) & 0x01;
    LCW_bits[j + 7] = (LCW_bytes[i] >> 0) & 0x01;
  }
 
  //send to new P25 LCW function
  if (irrecoverable_errors == 0)
  {
    fprintf (stderr, "%s", KYEL);
    p25_lcw (opts, state, LCW_bits, 0);
    fprintf (stderr, "%s", KNRM);
  }
  else
  {
    fprintf (stderr, "%s",KRED);
    fprintf (stderr, " LCW FEC ERR ");
    fprintf (stderr, "%s\n", KNRM);
  }

  //NOTE: LSD is also encrypted if voice is encrypted, so let's just zip it for now
  if (state->payload_algid != 0x80)
  {
    lsd_hex1 = 0;
    lsd_hex2 = 0;
  }

  //WIP: LSD FEC
  lsd1_okay = p25p1_lsd_fec (lowspeeddata+0);
  lsd2_okay = p25p1_lsd_fec (lowspeeddata+16);

  if (opts->payload == 1)
  {
    fprintf (stderr, "%s",KCYN);
    fprintf (stderr, " P25 LCW Payload ");
    for (i = 0; i < 9; i++)
    {
      fprintf (stderr, "[%02X]", LCW_bytes[i]);
    }

    #ifdef SOFTID
    //view Low Speed Data
    fprintf (stderr, "%s",KCYN);
    fprintf (stderr, "    LSD: %02X %02X ", lsd_hex1, lsd_hex2);
    if ( (lsd_hex1 > 0x19) && (lsd_hex1 < 0x7F) && (lsd1_okay == 1) ) fprintf (stderr, "(%c", lsd_hex1);
    else fprintf (stderr, "( ");
    if ( (lsd_hex2 > 0x19) && (lsd_hex2 < 0x7F) && (lsd2_okay == 1) ) fprintf (stderr, "%c)", lsd_hex2);
    else fprintf (stderr, " )");
    if (lsd1_okay == 0) fprintf (stderr, " L1 ERR");
    if (lsd2_okay == 0) fprintf (stderr, " L2 ERR");
    #endif
    fprintf (stderr, "%s\n", KNRM);
  }
  #ifdef SOFTID
  //TEST: Store LSD into array if 0x02 0x08 (opcode and len?)
  int k = 0;
  if (state->dmr_alias_format[0] == 0x02)
  {
    k = state->data_block_counter[0];
    if ( (lsd_hex1 > 0x19) && (lsd_hex1 < 0x7F) && (lsd1_okay == 1) )
      state->dmr_alias_block_segment[0][0][k/4][k%4] = (char)lsd_hex1;
    // else state->dmr_alias_block_segment[0][0][k/4][k%4] = 0x20;
    k++;
    if ( (lsd_hex2 > 0x19) && (lsd_hex2 < 0x7F) && (lsd2_okay == 1) )
      state->dmr_alias_block_segment[0][0][k/4][k%4] = (char)lsd_hex2;
    // else state->dmr_alias_block_segment[0][0][k/4][k%4] = 0x20;
    k++;
    state->data_block_counter[0] = k;
  }

  //reset format, len, counter.
  if (lsd_hex1 == 0x02 && lsd1_okay == 1 && lsd2_okay == 1)
  {
    state->dmr_alias_format[0] = 0x02;
    if (lsd_hex2 > 8) lsd_hex2 = 8; //sanity check
    state->dmr_alias_len[0] = lsd_hex2;
    state->data_block_counter[0] = 0;
  }

  if ( (k >= state->dmr_alias_len[0]) && (state->dmr_alias_format[0] == 0x02) )
  {
    //storage for completed string
    char str[16]; int wr = 0; int tsrc = state->lastsrc; int z = 0; k = 0;
    for (i = 0; i < 16; i++) str[i] = 0;

    //print out what we've gathered
    fprintf (stderr, "%s",KCYN);
    fprintf (stderr, " LSD Soft ID: ");
    for (i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        fprintf (stderr, "%c", state->dmr_alias_block_segment[0][0][i][j]);
        if (state->dmr_alias_block_segment[0][0][i][j] != 0) str[k++] = state->dmr_alias_block_segment[0][0][i][j];
      }
    }

    //debug
    // fprintf (stderr, " STR: %s", str);

    //assign to tg name string, but only if not trunking (this could clash with ENC LO functionality)
    if (tsrc != 0) //&& opts->p25_trunk == 0 //should never get here if enc, should be zeroed out, but could potentially slip if HDU is missed and offchance of 02 opcode
    {
      for (int x = 0; x < state->group_tally; x++)
      {
        if (state->group_array[x].groupNumber == tsrc)
        {
          wr = 1; //already in there, so no need to assign it
          z = x;
          break;
        }
      }

      //if not already in there, so save it there now
      if (wr == 0)
      {
        state->group_array[state->group_tally].groupNumber = tsrc;
        if (state->payload_algid != 0x80 && opts->trunk_tune_enc_calls == 0 && state->R == 0)
          sprintf (state->group_array[state->group_tally].groupMode, "%s", "DE");
        else
         sprintf (state->group_array[state->group_tally].groupMode, "%s", "D");
        sprintf (state->group_array[state->group_tally].groupName, "%s", str);
        state->group_tally++;
      }

      //if its in there, but doesn't match (bad/partial decode)
      else if (strcmp(str, state->group_array[z].groupName) != 0)
      {
        state->group_array[z].groupNumber = tsrc;
        if (state->payload_algid != 0x80 && opts->trunk_tune_enc_calls == 0 && state->R == 0)
          sprintf (state->group_array[state->group_tally].groupMode, "%s", "DE");
        else
         sprintf (state->group_array[state->group_tally].groupMode, "%s", "D");
        sprintf (state->group_array[z].groupName, "%s", str);
      }
      
    }

    fprintf (stderr, "%s",KNRM);
    fprintf (stderr, "\n");
  }
  #else
  UNUSED2(lsd1_okay, lsd2_okay);
  #endif //SOFTID

}
