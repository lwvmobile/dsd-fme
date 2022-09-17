/*-------------------------------------------------------------------------------
 * p25p1_tsbk.c
 * P25 TSBK Handler for Network Status Broadcast (WACN, SYSID, NAC/CC), etc.
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
void processTSBK(dsd_opts * opts, dsd_state * state)
{
  //process TSDU/TSBK and look for relevant data (WACN, SYSID, other goodies later on)
  int tsbkbit[196] = {0}; //tsbk bit array, 196 trellis encoded bits
  int tsbk_dibit[98] = {0};

  int dibit = 0;
  uint8_t tsbk_byte[12] = {0}; //12 byte return from bd_bridge (block_deinterleave)
  int tsbk_decoded_bits[190] = {0}; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  int i, j, k, b;
  int ec = -2; //error value returned from (block_deinterleave)
  int checksum = -2; //checksum returned from crc16, 0 is valid, anything else is invalid
  int skipdibit = 14; //initial status dibit will occur at 14, then add 35 each time it occurs
  for (j = 0; j < 3; j++)
  {
    k = 0;
    //originally skipped 303 dibits, instead we collect three reps of 101 (98 valid dibits)
    for (i = 0; i < 101; i++)
    {
  		//rip dibits directly from the symbol capture file
      //this method causes issues for some reason, probably to do with mixing reading types? don't know why?
  		// if (opts->audio_in_type == 4) //4
  		// {
  		// 	dibit = fgetc(opts->symbolfile);
  		// }
  		// else
  		// {
  		// 	dibit = getDibit(opts, state);
  		// 	if (opts->inverted_p2 == 1)
  		// 	{
  		// 		dibit = (dibit ^ 2);
  		// 	}
  		// }

      dibit = getDibit(opts, state);

      if (i+(j*101) != skipdibit) //
      {
        tsbk_dibit[k] =   dibit;
        tsbkbit[k*2]   = (dibit >> 1) & 1;
        tsbkbit[k*2+1] = (dibit & 1);
        k++;
      }

      if (i+(j*101) == skipdibit) //
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
      for (j = 0; j < 8; j++)
      {
        tsbk_decoded_bits[k] = ((tsbk_byte[i] << j) & 0x80) >> 7;
        k++;
      }
    }

    //crc check works now using the ComputeCrcCCITT method and not the OP25 method (bug?)
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

    //set our WACN and SYSID here now that we have valid ec and crc/checksum
    if (err == 0 && ec == 0 && tsbk_byte[0] == 0x3B)
    {
      //only set IF these values aren't already hard set by the user
      if (state->p2_hardset == 0)
      {
        state->p2_wacn  = (tsbk_byte[3] << 12) | (tsbk_byte[4] << 4) | (tsbk_byte[5] >> 4);
        state->p2_sysid = ((tsbk_byte[5] & 0xF) << 8) | tsbk_byte[6];
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n Network Status Broadcast TSBK \n");
        fprintf (stderr, "  WACN [%05llX] SYSID [%03llX] NAC [%03llX]", state->p2_wacn, state->p2_sysid, state->p2_cc);
        fprintf (stderr, "%s ", KNRM);
        //state->p2_cc = state->nac; //set in frame_sync line 450 instead after valid bch check
      }

    }

    // print dump for debug eval
    if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n  P25 TSBK Byte Payload ");
      fprintf (stderr, " BD = %d  CRC = %d \n  ", ec, err);
      for (i = 0; i < 12; i++)
      {
        fprintf (stderr, "[%02X]", tsbk_byte[i]);
      }
      fprintf (stderr, "%s ", KNRM);
    }

    //reset for next rep
    ec = -2;
    checksum = -2;
  }
  fprintf (stderr, "\n"); //line break on loop end
}
