/*-------------------------------------------------------------------------------
 * p25p1_tsbk.c
 * P25p1 Trunking Signal Block Handler
 *
 * LWVMOBILE
 * 2022-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

int block_convolution (int payload[196], uint8_t decoded[12])
{
  int ec = -1; //initialize error value

  uint8_t deinterleaved_bits[196];
  uint8_t trellis_bytes[12];
  uint8_t m_data[14];
  uint8_t temp[210]; //temp bit position buffer for convolution

  //initialize our deinterleaved bits and bytes with zeroes
  for (int i = 0; i < 196; i++) deinterleaved_bits[i] = 0;
  for (int i = 0; i < 210; i++) temp[i] = 0;
  for (int i = 0; i < 12; i++)  trellis_bytes[i] = 0;
  for (int i = 0; i < 14; i++)  m_data[i] = 0;

  //convolution decoder 
  uint8_t s0;
  uint8_t s1;

  //initialize our decoded byte buffer with zeroes
  for (int i = 0; i < 12; i++) decoded[i] = 0;

  static const uint16_t deinterleave_tsbk[] = {
  0,  1,  2,  3,  52, 53, 54, 55, 100,101,102,103, 148,149,150,151,
  4,  5,  6,  7,  56, 57, 58, 59, 104,105,106,107, 152,153,154,155,
  8,  9, 10, 11,  60, 61, 62, 63, 108,109,110,111, 156,157,158,159,
  12, 13, 14, 15,  64, 65, 66, 67, 112,113,114,115, 160,161,162,163,
  16, 17, 18, 19,  68, 69, 70, 71, 116,117,118,119, 164,165,166,167,
  20, 21, 22, 23,  72, 73, 74, 75, 120,121,122,123, 168,169,170,171,
  24, 25, 26, 27,  76, 77, 78, 79, 124,125,126,127, 172,173,174,175,
  28, 29, 30, 31,  80, 81, 82, 83, 128,129,130,131, 176,177,178,179,
  32, 33, 34, 35,  84, 85, 86, 87, 132,133,134,135, 180,181,182,183,
  36, 37, 38, 39,  88, 89, 90, 91, 136,137,138,139, 184,185,186,187,
  40, 41, 42, 43,  92, 93, 94, 95, 140,141,142,143, 188,189,190,191,
  44, 45, 46, 47,  96, 97, 98, 99, 144,145,146,147, 192,193,194,195,
  48, 49, 50, 51 };

  //get the deinterleaved bits
  for (int i = 0; i < 196; i++) deinterleaved_bits[deinterleave_tsbk[i]] = payload[i];

  //shift into proper position for the convolutional decoder
  for (int i = 0; i < 196; i++) temp[i] = deinterleaved_bits[i] << 1; 

  CNXDNConvolution_start();
  for (int i = 0U; i < 98U; i++) //102U
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 96U); //98U

  //debug print
  fprintf (stderr, "\n Convolution Chainback - ");
  for (int i = 0; i < 14; i++) fprintf (stderr, "[%02X]", m_data[i]);

  //transfer m_data to decoded bytes -- when sorted out, can probably send decoded straight to chainback, and save the code
  // for (int i = 0; i < 12; i++) decoded[i] = m_data[i];

  ec = 0; //unknown if a method exists to returns errors from the convolutional decoder, just rely on CRC check for now

  return ec;
}

void processTSBK(dsd_opts * opts, dsd_state * state)
{
  
  int tsbkbit[196] = {0}; //tsbk bit array, 196 trellis encoded bits
  int tsbk_dibit[98] = {0};

  int dibit = 0;
  uint8_t tsbk_byte[12] = {0}; //12 byte return from bd_bridge (block_deinterleave)
  unsigned long long int PDU[24] = {0}; //24 byte PDU to send to the tsbk_byte vPDU handler, should be same formats (mostly)
  int tsbk_decoded_bits[190] = {0}; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  int i, j, k, b, x, y;
  int ec = -2; //error value returned from (block_deinterleave)
  int err = -2; //error value returned from crc16_lb_bridge
  int skipdibit = 14; //initial status dibit will occur at 14, then add 36 each time it occurs
  int protectbit = 0;
  int MFID = 0xFF; //Manufacturer ID - Might be beneficial to NOT send anything but standard 0x00 or 0x01 messages

  //Update: Identified Bug in skipdibit incrememnt causing many block deinterleave/crc failures
  //with correct values set, now we are decoding many more TSBKs
  //TODO: Don't Print TSBKs/vPDUs unless verbosity levels set higher
  //TSBKs are always single block, but up to three can be transmitted in a row,
  //Multi-Block PDUs are sent on a different DUID - 0xC (not handled currently)

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
        skipdibit += 36; //was set to 35, this was the bug
      }

    }

    //test with the nxdn convolutional decoder
    // ec = block_convolution (tsbkbit, tsbk_byte);

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

    err = crc16_lb_bridge(tsbk_decoded_bits, 80);

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
    }

    //convert tsbk_byte to vPDU and send to vPDU handler
    //...may or may not be entirely compatible,
    MFID   = tsbk_byte[1];
    PDU[0] = 0x07; //P25p1 TSBK Duid 0x07
    PDU[1] = tsbk_byte[0] & 0x3F;
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

    //check the protect bit, don't run if protected
    protectbit = (tsbk_byte[0] >> 6) & 0x1;

    //Don't run NET_STS out of this, or will set wrong NAC/CC //&& PDU[1] != 0x49 && PDU[1] != 0x45
    //0x49 is telephone grant, 0x46 Unit to Unit Channel Answer Request (seems bogus) (mfid 90)
    // if (MFID < 0x2 && protectbit == 0 && err == 0 && ec == 0 && PDU[1] != 0x7B )
    if ( (MFID < 0x2 || MFID == 0xA4 || MFID == 0x90) && protectbit == 0 && err == 0 && ec == 0 && PDU[1] != 0x7B )  
    {
      fprintf (stderr, "%s",KYEL);
      process_MAC_VPDU(opts, state, 0, PDU);
      fprintf (stderr, "%s",KNRM);
    }

    //set our WACN and SYSID here now that we have valid ec and crc/checksum
    else if (protectbit == 0 && err == 0 && ec == 0 && (tsbk_byte[0] & 0x3F) == 0x3B) 
    {
      long int wacn = (tsbk_byte[3] << 12) | (tsbk_byte[4] << 4) | (tsbk_byte[5] >> 4);;
      int sysid = ((tsbk_byte[5] & 0xF) << 8) | tsbk_byte[6];
      int channel = (tsbk_byte[7] << 8) | tsbk_byte[8];
      fprintf (stderr, "%s",KYEL);
      fprintf (stderr, "\n Network Status Broadcast TSBK - Abbreviated \n");
      fprintf (stderr, "  WACN [%05lX] SYSID [%03X] NAC [%03llX]", wacn, sysid, state->p2_cc);
      state->p25_cc_freq = process_channel_to_freq(opts, state, channel);
      state->p25_cc_is_tdma = 0; //flag off for CC tuning purposes when system is qpsk

      //place the cc freq into the list at index 0 if 0 is empty, or not the same, 
      //so we can hunt for rotating CCs without user LCN list
      if (state->trunk_lcn_freq[0] == 0 || state->trunk_lcn_freq[0] != state->p25_cc_freq)
      {
        state->trunk_lcn_freq[0] = state->p25_cc_freq; 
      } 

      //only set IF these values aren't already hard set by the user
      if (state->p2_hardset == 0)
      {
        state->p2_wacn = wacn;
        state->p2_sysid = sysid;
      }  
        
      if (opts->payload == 1)
      {
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n P25 PDU Payload #%d ", j);
        for (i = 0; i < 12; i++)
        {
          fprintf (stderr, "[%02X]", tsbk_byte[i]);
        }
      }

      fprintf (stderr, "%s ", KNRM);

    }

    else
    {
      if (opts->payload == 1)
      {
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n P25 PDU Payload #%d ", j+1);
        for (i = 0; i < 12; i++)
        {
          fprintf (stderr, "[%02X]", tsbk_byte[i]);
        }
        fprintf (stderr, "\n MFID %02X Protected: %d Last Block: %d", MFID, protectbit, (tsbk_byte[0] >> 7) );
        
        if (ec != 0) 
        {
          fprintf (stderr, "%s",KRED);
          fprintf (stderr, " (FEC ERR)");
        }
        else if (err != 0) 
        {
          fprintf (stderr, "%s",KRED);
          fprintf (stderr, " (CRC ERR)");
        }
        fprintf (stderr, "%s ", KNRM);
      }
    } 

    //reset for next rep
    ec = -2;
    err = -2;
    protectbit = 0;
    MFID = 0xFF;

    //check for last block bit
    if ( (tsbk_byte[0] >> 7) == 1 ) 
    {
      j = 4; //set j to break the loop early
    }
  }

  fprintf (stderr, "%s ", KNRM);
  fprintf (stderr, "\n"); 
}
