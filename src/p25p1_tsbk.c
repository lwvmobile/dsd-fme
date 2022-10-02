/*-------------------------------------------------------------------------------
 * p25p1_tsbk.c
 * P25 Trunking Signal Block Handler
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
void processTSBK(dsd_opts * opts, dsd_state * state)
{
  //process TSDU/TSBK and look for relevant data (WACN, SYSID, iden_up, other goodies later on)
  int tsbkbit[196] = {0}; //tsbk bit array, 196 trellis encoded bits
  int tsbk_dibit[98] = {0};

  int dibit = 0;
  uint8_t tsbk_byte[12] = {0}; //12 byte return from bd_bridge (block_deinterleave)
  unsigned long long int PDU[24] = {0}; //24 byte PDU to send to the tsbk_byte vPDU handler, should be same formats
  int tsbk_decoded_bits[190] = {0}; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  int i, j, k, b, x, y;
  int ec = -2; //error value returned from (block_deinterleave)
  int checksum = -2; //checksum returned from crc16, 0 is valid, anything else is invalid
  int skipdibit = 14; //initial status dibit will occur at 14, then add 35 each time it occurs

  //TSBKs are always single block, but up to three can be transmitted in a row,
  //my observation shows that, as far as I know, only the first block of three is good,
  //and NULL types can contain some amount of garbage, but the CRC is always 00s.
  //Multi-Block PDUs are sent on a different DUID - 0xC
  //Best way to proceed for now is going to be to collect all three potentials, and only process on good
  //de-interleaves AND good CRCs, otherwise, ignore them
  //The Last Block flag doesn't really seem to apply for some reason (byte 0 0x80), or is always on a garbage block

  //collect three reps of 101 dibits (98 valid dibits with status dibits interlaced)
  for (j = 0; j < 3; j++)
  {
    k = 0;
    for (i = 0; i < 101; i++)
    {

      dibit = getDibit(opts, state);

      if (i+(j*101) != skipdibit) //
      {
        tsbk_dibit[k] =   dibit;
        tsbkbit[k*2]   = (dibit >> 1) & 1;
        tsbkbit[k*2+1] = (dibit & 1);
        k++;
      }

      if (i+(j*101) == skipdibit) 
      {
        skipdibit += 35;
      }

    }

    //send tsbkbit to block_deinterleave and return tsbk_byte
    ec = bd_bridge(tsbkbit, tsbk_byte);

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

    int err = -2;
    err = crc16_lb_bridge(tsbk_decoded_bits, 80);
    if (ec != 0)
    {
      //fprintf (stderr, "BAD BLOCK DEINTERLEAVE");
    }
    if (err != 0)
    {
      //fprintf (stderr, "BAD CRC16");
    }

    //convert tsbk_byte to PDU and send to vPDU handler...may or may not be entirely compatible, 
    PDU[0] = 07; //P25p1 TSBK Duid 0x07
    PDU[1] = tsbk_byte[0];
    PDU[2] = tsbk_byte[2];
    PDU[3] = tsbk_byte[3];
    PDU[4] = tsbk_byte[4];
    PDU[5] = tsbk_byte[5];
    PDU[6] = tsbk_byte[6];
    PDU[7] = tsbk_byte[7];
    PDU[8] = tsbk_byte[8];
    PDU[9] = tsbk_byte[9];
    //remove CRC to prevent false positive when vPDU goes to look for additional message in block
    PDU[10] = 0; //tsbk_byte[10]; 
    PDU[11] = 0; //tsbk_byte[11];
    PDU[1] = PDU[1] ^ 0x40; //flip bit to make it compatible with MAC_PDUs, i.e. 3D to 7D

    //Don't run NET_STS out of this, or will set wrong NAC/CC
    if (err == 0 && ec == 0 && PDU[1] != 0x7B && PDU[1] != 0xFB) 
    {
      fprintf (stderr, "%s",KMAG);
      process_MAC_VPDU(opts, state, 0, PDU);
      fprintf (stderr, "%s",KNRM);
    }

    //set our WACN and SYSID here now that we have valid ec and crc/checksum
    if (err == 0 && ec == 0 && tsbk_byte[0] == 0x3B)
    {
      //only set IF these values aren't already hard set by the user
      if (state->p2_hardset == 0)
      {
        state->p2_wacn  = (tsbk_byte[3] << 12) | (tsbk_byte[4] << 4) | (tsbk_byte[5] >> 4);
        state->p2_sysid = ((tsbk_byte[5] & 0xF) << 8) | tsbk_byte[6];
        int channel = (tsbk_byte[7] << 8) | tsbk_byte[8];
        fprintf (stderr, "%s",KMAG);
        fprintf (stderr, "\n Network Status Broadcast TSBK - Abbreviated \n");
        fprintf (stderr, "  WACN [%05llX] SYSID [%03llX] NAC [%03llX]", state->p2_wacn, state->p2_sysid, state->p2_cc);
        
        state->p25_cc_freq = process_channel_to_freq(opts, state, channel);
        
        if (opts->payload == 1)
        {
          fprintf (stderr, "%s",KCYN);
          fprintf (stderr, "\n P25 PDU Payload ");
          fprintf (stderr, " BD = %d  CRC = %d \n  ", ec, err);
          for (i = 0; i < 12; i++)
          {
            fprintf (stderr, "[%02X]", tsbk_byte[i]);
          }
        }
        fprintf (stderr, "%s ", KNRM);

      }

    }

    //reset for next rep
    ec = -2;
    checksum = -2;
  }
  fprintf (stderr, "\n"); 
}
