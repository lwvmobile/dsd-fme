/*-------------------------------------------------------------------------------
 * p25p1_mpdu.c
 * P25p1 Multi Block PDU Assembly
 *
 * LWVMOBILE
 * 2025-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

uint32_t crc32mbf(uint8_t * buf, int len)
{
  uint32_t g = 0x04c11db7;
  uint64_t crc = 0;
  for (int i = 0; i < len; i++)
  {
    crc <<= 1;
    int b = ( buf [i / 8] >> (7 - (i % 8)) ) & 1;
    if (((crc >> 32) ^ b) & 1) crc ^= g;
  }
  crc = (crc & 0xffffffff) ^ 0xffffffff;
  return crc;
}

void processMPDU(dsd_opts * opts, dsd_state * state)
{

  //p25p2 18v reset counters and buffers
  state->voice_counter[0] = 0; //reset
  state->voice_counter[1] = 0; //reset
  memset (state->s_l4, 0, sizeof(state->s_l4));
  memset (state->s_r4, 0, sizeof(state->s_r4));
  opts->slot_preference = 2;

  //push current slot to 0, just in case swapping p2 to p1
  //or stale slot value from p2 and then decoding a pdu
  state->currentslot = 0;

  //reset some strings when returning from a call in case they didn't get zipped already
  sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  sprintf (state->call_string[1], "%s", "                     "); //21 spaces

  //clear stale Active Channel messages here
  if ( (time(NULL) - state->last_active_time) > 3 )
  {
    memset (state->active_channel, 0, sizeof(state->active_channel));
  }

  uint8_t tsbk_dibit[98];
  memset (tsbk_dibit, 0, sizeof(tsbk_dibit));

  int dibit = 0;
  int r34 = 0;

  uint8_t tsbk_byte[12]; //12 byte return from p25_12
  memset (tsbk_byte, 0, sizeof(tsbk_byte));

  uint8_t r34byte_b[18];
  memset (r34byte_b, 0, sizeof(r34byte_b));

  //TODO: Expand storage to be able to hold 127 blocks + 1 header of data at 144 bits (18 octets
  uint8_t r34bytes[18*129]; //18 octets at 129 blocks (a little extra padding)
  memset (r34bytes, 0, sizeof(r34bytes));

  unsigned long long int PDU[18*129];
  memset (PDU, 0, 18*129*sizeof(unsigned long long int));

  int tsbk_decoded_bits[18*129*8]; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  memset (tsbk_decoded_bits, 0, sizeof(tsbk_decoded_bits));

  uint8_t mpdu_decoded_bits[18*129*8];
  memset (mpdu_decoded_bits, 0, sizeof(mpdu_decoded_bits));

  uint8_t mpdu_crc_bits[18*129*8];
  memset (mpdu_crc_bits, 0, sizeof(mpdu_crc_bits));

  uint8_t mpdu_crc9_bits[18*129*8];
  memset (mpdu_crc9_bits, 0, sizeof(mpdu_crc9_bits));

  uint8_t mpdu_crc_bytes[18*129];
  memset (mpdu_crc_bytes, 0, sizeof(mpdu_crc_bytes));

  int i, j, k, x, z, l; z = 0; l = 0;
  int ec[129]; //error value returned from p25_12 or 34 trellis decoder
  int err[2]; //error value returned from crc16 on header and crc32 on full message
  memset (ec, -2, sizeof(ec));
  memset (err, -2, sizeof(err));

  uint8_t mpdu_byte[18*129];
  memset (mpdu_byte, 0, sizeof(mpdu_byte));

  uint8_t an = 0;
  uint8_t io = 0;
  uint8_t fmt = 0;
  uint8_t sap = 0;
  uint8_t blks = 0;
  int end = 3; //ending value for data gathering repetitions (default at 3)

  //CRC32
  uint32_t CRCComputed = 0;
  uint32_t CRCExtracted = 0;

  int stop = 101;
  int start = 0;

  //now using modulus on skipdibit values
  int skipdibit = 36-14; //when we arrive here, we are at this position in the counter after reading FS, NID, DUID, and Parity dibits
  int status_count = 1; //we have already skipped the Status 1 dibit before arriving here
  int dibit_count = 0; //number of gathered dibits
  UNUSED(status_count); //debug counter

  //collect x-len reps of 100 or 101 dibits (98 valid dibits with two or three status dibits sprinkled in)
  for (j = 0; j < end; j++)
  {
    k = 0;
    dibit_count = 0;
    for (i = start; i < stop; i++)
    {
      dibit = getDibit(opts, state);
      if ( (skipdibit / 36) == 0)
      {
        dibit_count++;
        tsbk_dibit[k++] = dibit;
      }
      else
      {
        skipdibit = 0;
        status_count++;
      }

      skipdibit++; //increment

      //this is used to skip gathering one dibit as well since we only will end up skipping 2 status dibits (getting 99 instead of 98, throwing alignment off)
      if (dibit_count == 98) //this could cause an issue though if the next bit read is supposed to be a status dibit (unsure) it may not matter, should be handled as first read in next rep
        break;
    }

    //send header to p25_12 and return tsbk_byte
    if (j == 0) ec[j] = p25_12 (tsbk_dibit, tsbk_byte);

    else if (r34)
    {
      //debug
      // fprintf (stderr, " J:%d;", j); //use this with the P_ERR inside of 34 rate decoder to see where the failures occur

      ec[j] = viterbi_r34 (tsbk_dibit, r34byte_b);

      //shuffle 34 rate data into array
      if (j != 0) //should never happen, but just in case
        memcpy (r34bytes+((j-1)*18), r34byte_b, sizeof(r34byte_b));

      for (i = 2; i < 18; i++)
      {
        for (x = 0; x < 8; x++)
          mpdu_crc_bits[z++] = ((r34byte_b[i] << x) & 0x80) >> 7;
      }

      //arrangement for confirmed data crc9 check
      //unlike DMR, the first 7 bits of this arrangement are the DBSN, not the last 7 bits
      for (x = 0; x < 7; x++)
        mpdu_crc9_bits[l++] = ((r34byte_b[0] << x) & 0x80) >> 7;
      for (i = 2; i < 18; i++)
      {
        for (x = 0; x < 8; x++)
          mpdu_crc9_bits[l++] = ((r34byte_b[i] << x) & 0x80) >> 7;
      }

    }
    else ec[j] = p25_12 (tsbk_dibit, tsbk_byte);

    //too many bit manipulations!
    k = 0;
    for (i = 0; i < 12; i++)
    {
      for (x = 0; x < 8; x++)
      {
        tsbk_decoded_bits[k] = ((tsbk_byte[i] << x) & 0x80) >> 7;
        k++;
      }
    }

    //CRC16 on the header
    if (j == 0) err[0] = crc16_lb_bridge(tsbk_decoded_bits, 80);

    //load into bit array for storage (easier decoding for future PDUs)
    for (i = 0; i < 96; i++) mpdu_decoded_bits[i+(j*96)] = (uint8_t)tsbk_decoded_bits[i];

    //shuffle corrected bits back into tsbk_byte
    k = 0;
    for (i = 0; i < 12; i++)
    {
      int byte = 0;
      for (x = 0; x < 8; x++)
      {
        byte = byte << 1;
        byte = byte | tsbk_decoded_bits[k];
        k++;
      }
      tsbk_byte[i] = byte;
      mpdu_byte[i+(j*12)] = byte; //add to completed MBF format 12 rate bytes
    }

    //check header data to see if this is a 12 rate, or 34 rate packet data unit
    if ( (j == 0 && err[0] == 0) || (j == 0 && opts->aggressive_framesync == 0))
    {
      an   = (mpdu_byte[0] >> 6) & 0x1;
      io   = (mpdu_byte[0] >> 5) & 0x1;
      fmt  = mpdu_byte[0] & 0x1F;
      sap  = mpdu_byte[1] & 0x3F;
      blks = mpdu_byte[6] & 0x7F;

      if (an == 1 && fmt == 0b10110) //confirmed data packet header block
        r34 = 1;

      //set end value to number of blocks + 1 header (block gathering for variable len)
      if (sap != 61 && sap != 63) //if not a trunking control block, this fixes an annoyance TSBK/TDULC blink in ncurses
        end = blks + 1;           //(TDU follows any Trunking MPDU, not sure if that's an error in handling, or actual behavior)

      //sanity check -- since blks is only 7 bit anyways, this probably isn't needed now
      if (end > 128) end = 128;  //Storage for up to 127 blocks plus 1 header

    }

  }

  if (err[0] == 0)
    p25_decode_pdu_header (opts, state, mpdu_byte);
  else if (opts->aggressive_framesync == 0)
    p25_decode_pdu_header (opts, state, mpdu_byte);

  if (err[0] != 0) //crc error, so we can't validate information as accurate
  {
    fprintf (stderr, "%s",KRED);
    fprintf (stderr, " P25 Data Header CRC Error");
    fprintf (stderr, "%s",KNRM);
    if (opts->aggressive_framesync != 0)
      end = 1; //go ahead and end after this loop
  }

  //trunking blocks
  if ((sap == 0x3D) && ((fmt == 0x17) || (fmt == 0x15)))
  {

    //CRC32 is now working!
    CRCExtracted = (mpdu_byte[(12*(blks+1))-4] << 24) | (mpdu_byte[(12*(blks+1))-3] << 16) | (mpdu_byte[(12*(blks+1))-2] << 8) | (mpdu_byte[(12*(blks+1))-1] << 0);
    CRCComputed  = crc32mbf(mpdu_byte+12, (96*blks)-32);
    if (CRCComputed == CRCExtracted) err[1] = 0;

    //pass the PDU to p25_decode_pdu_trunking
    if (err[0] == 0 && err[1] == 0 && io == 1 && fmt == 0x17) //ALT Format
      p25_decode_pdu_trunking(opts, state, mpdu_byte);
    // else if (opts->aggressive_framesync == 0 && io == 1 && fmt == 0x17)
    //   p25_decode_pdu_trunking(opts, state, mpdu_byte);

    if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MBT Payload \n  ");
      for (i = 0; i < ((blks+1)*12); i++)
      {
        if ( (i != 0) && ((i % 12) == 0))
          fprintf (stderr, "\n  ");
        fprintf (stderr, "[%02X]", mpdu_byte[i]);

      }

      fprintf (stderr, "\n ");
      fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
      fprintf (stderr, "%s ", KNRM);

      //Header
      if (err[0] != 0)
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, " (HDR CRC16 ERR)");
        fprintf (stderr, "%s",KCYN);
      }

      //Completed MBF
      if (err[1] != 0)
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, " (MBT CRC32 ERR)");
        fprintf (stderr, "%s",KCYN);
      }

    }

    fprintf (stderr, "%s ", KNRM);
    fprintf (stderr, "\n");

  } //end trunking block format

  else if (r34 == 1) // && err[0] == 0) //start r34
  {
    //TODO: Cleanup and make more elegant (maybe just use the crc_bytes on the payload dump)
    uint8_t crc_bytes[127*18];
    memset (crc_bytes, 0, sizeof(crc_bytes));
    for (i = 0; i < 16*(blks+1); i++)
      crc_bytes[i] = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[i*8], 8);

    CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[(128*blks)-32], 32);
    CRCComputed  = crc32mbf(crc_bytes, (128*blks)-32);
    if (CRCComputed == CRCExtracted) err[1] = 0;

    //reset mpdu_byte to load only the data, and not the dbsn and crc into
    memset (mpdu_byte+12, 0, sizeof(mpdu_byte)-12);
    int mpdu_idx = 12;
    int next = 0;

    //arrays for each block
    uint8_t  block_ptr = 0; //ptr to current block
    uint8_t  dbsn[127];     memset(dbsn, 0, sizeof(dbsn));
    uint16_t crc9_ext[127]; memset(crc9_ext, 0, sizeof(crc9_ext));
    uint16_t crc9_cmp[127]; memset(crc9_cmp, 0, sizeof(crc9_cmp));

    //reconstruct the message
    for (i = 2; i <= 18*blks; i++)
    {
      if ( (i != 0) && ((i % 18) == 0))
      {
        dbsn[block_ptr] = r34bytes[i-18] >> 1; //get the previous DBSN at this point
        crc9_ext[block_ptr] = ((r34bytes[i-18] & 1) << 8) | r34bytes[i-17];
        crc9_cmp[block_ptr] = ComputeCrc9Bit(mpdu_crc9_bits+next, 135);
        next += 135;
        block_ptr++;
        if (i != 18*blks) i += 2; //skip the next DBSN/CRC9
      }
      mpdu_byte[mpdu_idx++] = r34bytes[i];
    }

    //minus 1 to offset the last rounds mpdu_idx++
    if (err[1] == 0 && blks != 0)
      p25_decode_pdu_data(opts, state, mpdu_byte, mpdu_idx-1);
    else if (opts->aggressive_framesync == 0 && blks != 0)
      p25_decode_pdu_data(opts, state, mpdu_byte, mpdu_idx-1);

    if (opts->payload == 1)
    {
      block_ptr = 0;
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU Rate 34 Payload \n ");
      for (i = 0; i < 12; i++) //header
        fprintf (stderr, "%02X", mpdu_byte[i]);
      fprintf (stderr, "         Header \n ");

      for (i = 12; i < mpdu_idx; i++)
      {
        if ( ((i - 12) != 0) && (((i-12)%16) == 0))
        {
          if (crc9_ext[block_ptr] == crc9_cmp[block_ptr])
            fprintf (stderr, " DBSN: %d;", dbsn[block_ptr]+1);
          else
          {
            fprintf (stderr, "%s",KRED);
            fprintf (stderr, " CRC ERR;");
            fprintf (stderr, "%s",KCYN);
            // fprintf (stderr, " EXT: %03X; CMP: %03X", crc9_ext[block_ptr], crc9_cmp[block_ptr]);
          }
          if (i != (mpdu_idx-1))
            fprintf (stderr, "\n ");
          block_ptr++;
        }
        if (i != (mpdu_idx-1))
          fprintf (stderr, "%02X", mpdu_byte[i]);
      }

      if (err[1] != 0)
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, "\n (MPDU CRC32 ERR)");
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
      }

    }

    fprintf (stderr, "%s ", KNRM);
    fprintf (stderr, "\n");

    //clear these, regardless of if PDU was deocded, or attempted,
    //so we don't create phantom voice calls in the event history
    state->lasttg = 0;
    state->lastsrc = 0;

  } //end r34

  else if (r34 == 0) //12 rate unconfirmed data //err[0] == 0
  {
    int len = 12*(blks+1);
    if (blks != 0)
    {
      CRCExtracted = (mpdu_byte[len-4] << 24) | (mpdu_byte[len-3] << 16) | (mpdu_byte[len-2] << 8) | (mpdu_byte[len-1] << 0);
      CRCComputed  = crc32mbf(mpdu_byte+12, (96*blks)-32);
      if (CRCComputed == CRCExtracted) err[1] = 0;
    }
    else err[1] = 0; //No CRC32 on a lonely header

    if (err[1] == 0 && blks != 0)
      p25_decode_pdu_data(opts, state, mpdu_byte, len);
    else if (opts->aggressive_framesync == 0 && blks != 0)
      p25_decode_pdu_data(opts, state, mpdu_byte, len);

    if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU Rate 12 Payload: \n  ");
      for (i = 0; i < len; i++) //header and payload combined
      {
        if (i == 12) fprintf (stderr, " Header");
        if ( (i != 0) && ((i % 12) == 0))
          fprintf (stderr, "\n  ");
        fprintf (stderr, "%02X", mpdu_byte[i]);
      }
    }

    if (err[1] != 0)
    {
      fprintf (stderr, "%s",KRED);
      fprintf (stderr, "\n (MPDU CRC32 ERR)");
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
    }

    fprintf (stderr, "%s",KNRM);
    fprintf (stderr, "\n");

    //clear these, regardless of if PDU was deocded, or attempted,
    //so we don't create phantom voice calls in the event history
    state->lasttg = 0;
    state->lastsrc = 0;

  } //end r12
  else //crc header failure or other
  {
    fprintf (stderr, "%s",KNRM);
    fprintf (stderr, "\n");
  }

}
