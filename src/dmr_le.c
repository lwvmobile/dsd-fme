/*-------------------------------------------------------------------------------
 * dmr_le.c
 * DMR Late Entry MI Fragment Assembly, Procesing, and related Alg functions
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//gather ambe_fr mi fragments for processing
void dmr_late_entry_mi_fragment (dsd_opts * opts, dsd_state * state, uint8_t vc, char ambe_fr[4][24], char ambe_fr2[4][24], char ambe_fr3[4][24])
{
  
  uint8_t slot = state->currentslot;

  //enforce RC4 due to missing PI header, but with valid SVC Opts
  //ideally, this would be handled by VC-F single burst, but its not fully reliable compared to this
  //due to multiple signalling occurring inside of it, depending on system type
  if (state->M == 0x21)
  {
    if (slot == 0 && state->dmr_so & 0x40)
    {
      state->payload_algid = 0x21;
      state->payload_keyid = 0xFF;
    }
    if (slot == 1 && state->dmr_soR & 0x40)
    {
      state->payload_algidR = 0x21;
      state->payload_keyidR = 0xFF;
    }
  }
  
  //collect our fragments and place them into storage
  state->late_entry_mi_fragment[slot][vc][0] = (uint64_t)ConvertBitIntoBytes(&ambe_fr[3][0], 4);
  state->late_entry_mi_fragment[slot][vc][1] = (uint64_t)ConvertBitIntoBytes(&ambe_fr2[3][0], 4);
  state->late_entry_mi_fragment[slot][vc][2] = (uint64_t)ConvertBitIntoBytes(&ambe_fr3[3][0], 4);

  if (vc == 6) dmr_late_entry_mi (opts, state);

}

void dmr_late_entry_mi (dsd_opts * opts, dsd_state * state)
{
  uint8_t slot = state->currentslot;
  int i, j;
  int g[3]; 
  unsigned char mi_go_bits[24]; 

  uint64_t mi_test = 0;
  uint64_t go_test = 0;
  uint64_t mi_corrected = 0;
  uint64_t go_corrected = 0;
  
  mi_test = (state->late_entry_mi_fragment[slot][1][0] << 32L) | (state->late_entry_mi_fragment[slot][2][0] << 28) | (state->late_entry_mi_fragment[slot][3][0] << 24) |
            (state->late_entry_mi_fragment[slot][1][1] << 20) | (state->late_entry_mi_fragment[slot][2][1] << 16) | (state->late_entry_mi_fragment[slot][3][1] << 12) |
            (state->late_entry_mi_fragment[slot][1][2] << 8)  | (state->late_entry_mi_fragment[slot][2][2] << 4)  | (state->late_entry_mi_fragment[slot][3][2] << 0);

  go_test = (state->late_entry_mi_fragment[slot][4][0] << 32L) | (state->late_entry_mi_fragment[slot][5][0] << 28) | (state->late_entry_mi_fragment[slot][6][0] << 24) |
            (state->late_entry_mi_fragment[slot][4][1] << 20) | (state->late_entry_mi_fragment[slot][5][1] << 16) | (state->late_entry_mi_fragment[slot][6][1] << 12) |
            (state->late_entry_mi_fragment[slot][4][2] << 8)  | (state->late_entry_mi_fragment[slot][5][2] << 4)  | (state->late_entry_mi_fragment[slot][6][2] << 0);

  for (j = 0; j < 3; j++)
  {
    for (i = 0; i < 12; i++)
    {
      mi_go_bits[i] = (( mi_test << (i+j*12) ) & 0x800000000) >> 35; 
      mi_go_bits[i+12] = (( go_test << (i+j*12) ) & 0x800000000) >> 35; 
    }
    //execute golay decode and assign pass or fail to g
    if ( Golay_24_12_decode(mi_go_bits) ) g[j] = 1;
    else g[j] = 0;

    for (i = 0; i < 12; i++)
    {
      mi_corrected = mi_corrected << 1;
      mi_corrected |= mi_go_bits[i];
      go_corrected = go_corrected << 1;
      go_corrected |= mi_go_bits[i+12];
    }
  }

  int mi_final = 0; 
  mi_final = (mi_corrected >> 4) & 0xFFFFFFFF;

  if (g[0] && g[1] && g[2])
  {
    if (slot == 0 && state->payload_algid != 0)
    {
      if (state->payload_mi != mi_final) 
      {
        fprintf (stderr, "%s", KCYN);
        fprintf (stderr, " Slot 1 PI/LFSR and Late Entry MI Mismatch - %08X : %08X \n", state->payload_mi, mi_final);
        state->payload_mi = mi_final; 
        fprintf (stderr, "%s", KNRM);
      }
      //run afterwards, or le verification won't match up properly
      if (state->payload_algid != 0x21)
      {
        LFSR64 (state);
        fprintf (stderr, "\n");
      } 
    }
    if (slot == 1 && state->payload_algidR != 0)
    {
      if (state->payload_miR != mi_final)
      {
        fprintf (stderr, "%s", KCYN);
        fprintf (stderr, " Slot 2 PI/LFSR and Late Entry MI Mismatch - %08X : %08X \n", state->payload_miR, mi_final);
        state->payload_miR = mi_final;
        fprintf (stderr, "%s", KNRM);
      }
      //run afterwards, or le verification won't match up properly
      if (state->payload_algidR != 0x21)
      {
        LFSR64 (state);
        fprintf (stderr, "\n");
      } 
    }

  }

}

void dmr_alg_refresh (dsd_opts * opts, dsd_state * state)
{
  if (state->currentslot == 0)
  {
    state->dropL = 256;

    if (state->K1 != 0) 
    {
      state->DMRvcL = 0;
    }

    if (state->payload_algid == 0x21)
    {
      LFSR (state);
      fprintf (stderr, "\n");
    }
    //LFSR64 carried out after LE verification in order to keep it from constantly resetting the MI to the previous value

  }
  if (state->currentslot == 1)
  {
    state->dropR = 256;

    if (state->K1 != 0) 
    {
      state->DMRvcR = 0;
    }

    if (state->payload_algidR == 0x21)
    {
      LFSR (state);
      fprintf (stderr, "\n");
    }
    //LFSR64 carried out after LE verification in order to keep it from constantly resetting the MI to the previous value

  }

}

void dmr_alg_reset (dsd_opts * opts, dsd_state * state)
{
  state->dropL = 256;
  state->dropR = 256;
  state->DMRvcL = 0;
  state->DMRvcR = 0; 
  state->payload_miP = 0;
  state->payload_miN = 0; 
}

//handle Single Burst (Voice Burst F) or Reverse Channel Signalling 
void dmr_sbrc (dsd_opts * opts, dsd_state * state, uint8_t power)
{
  int i;
  uint8_t slot = state->currentslot;
  uint8_t sbrc_interleaved[32];
  uint8_t sbrc_return[32];
  uint8_t sbrc_retcrc[32]; //crc significant bits of the return for SB
  memset (sbrc_interleaved, 0, sizeof(sbrc_interleaved));
  memset (sbrc_return, 0, sizeof(sbrc_return));
  memset (sbrc_retcrc, 0, sizeof(sbrc_retcrc));
  uint32_t irr_err = 0;
  uint32_t sbrc_hex = 0;
  uint16_t crc_extracted = 7777;
  uint16_t crc_computed = 9999;
  uint8_t crc7_okay = 0; //RC
  uint8_t crc3_okay = 0; //TXI
  uint8_t txi = 0; //SEE: https://patents.google.com/patent/US8271009B2
  
  //NOTE: Any previous mentions to Cap+ in this area may have been in error,
  //The signalling observed here was actually TXI information, not Cap+ Specifically

  //check to see if this a TXI system
  if (slot == 0 && state->dmr_so & 0x20)  txi = 1;
  if (slot == 1 && state->dmr_soR & 0x20) txi = 1;

  // 9.3.2 Pre-emption and power control Indicator (PI)
  // 0 - The embedded signalling carries information associated to the same logical channel or the Null embedded message
  // 1 - The embedded signalling carries RC information associated to the other logical channel

  for(i = 0; i < 32; i++) sbrc_interleaved[i] = state->dmr_embedded_signalling[slot][5][i + 8];
  //power == 0 should be single burst
  if (power == 0) irr_err = BPTC_16x2_Extract_Data(sbrc_interleaved, sbrc_return, 0);
  //power == 1 should be reverse channel -- still need to check the interleave inside of BPTC
  if (power == 1) irr_err = BPTC_16x2_Extract_Data(sbrc_interleaved, sbrc_return, 1);
  //bad emb burst, never set a valid power indicator value (probably 9)
  if (power > 1) goto SBRC_END;

  //arrange the return for proper order for the crc3 check
  for (i = 0; i < 8; i++) sbrc_retcrc[i] = sbrc_return[i+3];

  //RC Channel CRC 7 Mask = 0x7A; CRC bits are used as privacy indicators on 
  //Single Voice Burst F (see below), other moto values seem to exist there as well -- See TXI patent
  if (power == 1) //RC
  {
    crc_extracted = 0;
    for (i = 0; i < 7; i++)
    {
      crc_extracted = crc_extracted << 1;
      crc_extracted = crc_extracted | sbrc_return[i+4];
    }
    crc_extracted = crc_extracted ^ 0x7A;
    crc_computed = crc7((uint8_t *) sbrc_return, 7);
    if (crc_extracted == crc_computed) crc7_okay = 1;
  }
  else //if (txi == 1) //if TXI -- but TXI systems also carry the non-crc protected ENC identifiers
  {
    crc_extracted = 0;
    for (i = 0; i < 3; i++)
    {
      crc_extracted = crc_extracted << 1;
      crc_extracted = crc_extracted | sbrc_return[i]; //first 3 most significant bits
    }
    crc_computed = crc3((uint8_t *) sbrc_retcrc, 8); //working now seems consistent as well 
    if (crc_extracted == crc_computed) crc3_okay = 1;
    // fprintf (stderr, " CRC EXT %02X, CRC CMP %02X", crc_extracted, crc_computed);
  }
  // else //crc_okay = 0; //CRC invalid / not available when is ENC Identifiers (I really hate that)

  for(i = 0; i < 11; i++)
  {
    sbrc_hex = sbrc_hex << 1;
    sbrc_hex |= sbrc_return[i] & 1;
  }

  if (opts->payload == 1) //hide the sb/rc behind the payload printer, won't be useful to most people
  {
    fprintf (stderr, "%s", KCYN);
    if (power == 0) fprintf (stderr, " SB: ");
    if (power == 1) fprintf (stderr, " RC: ");
    for(i = 0; i < 11; i++)
      fprintf (stderr, "%d", sbrc_return[i]);
    fprintf (stderr, " - %03X; ", sbrc_hex);
    fprintf (stderr, "%s", KNRM);
    
    // if (crc_okay == 0) //forego this since the crc can vary or not be used at all
    // {
    //   fprintf (stderr, "%s", KRED);
    //   fprintf (stderr, " (CRC ERR)");
    //   fprintf (stderr, "%s", KNRM);
    //   fprintf (stderr, " CRC EXT %02X, CRC CMP %02X", crc_extracted, crc_computed);
    // }

    fprintf (stderr, "\n");
  }
  
  uint8_t sbrc_opcode = sbrc_hex & 0x7; //opcode and alg the same bits, but the alg is present when CRC is bad (I know they are limited on bits, but I hate that idea)
  uint8_t alg = sbrc_hex & 0x7;
  uint8_t key = (sbrc_hex >> 3) & 0xFF;
  uint8_t txi_delay = (sbrc_hex >> 3) & 0x1F; //middle five are the 'delay' value on a TXI system

  //Note: AES-256 Key 1 will pass a CRC3 due to its bit arrangement vs the CRC poly 1101
  // if (irr_err == 0 && sbrc_hex = 0xD) crc3_okay = 0; //SB: 00000001101

  //NOTE: on above, I belive that we need to check by opcode as well, as a CRC3 can have multiple collisions
  //so we need to exclude op/alg 0 and 3 from the check (does algID 0x03/0x23 even exist?)

  if (opts->dmr_le == 1) //this will now require a user to switch it on or off until more testing/figuring can be done
  {
    if (irr_err != 0) fprintf (stderr, "\n%s SLOT %d SB/RC (FEC ERR) %d %s \n", KRED, slot, irr_err, KNRM);
    if (irr_err == 0)
    {
      if (sbrc_hex == 0) ; //NULL, do nothing

      //else if (placeholder for future conditions)
      // {
      //   placeholder for future conditions
      // }

      else if (crc7_okay == 1)
      {
        //do something with the reverse channel information
      }

      //if the call is interruptable (TXI) and the crc3 is okay and TXI Opcode
      else if (crc3_okay == 1 && (sbrc_opcode == 0 || sbrc_opcode == 3) )
      {
        //opcodes -- 0 (NULL), BR Delay (3)
        fprintf (stderr, "\n");
        fprintf (stderr, " TXI Op: %X - ", sbrc_opcode);
        if      (sbrc_opcode == 0) fprintf (stderr, " Null; ");
        else if (sbrc_opcode == 3) 
        {
          if (txi_delay != 0)
            fprintf (stderr, " BR Delay: %d - %d ms; ", txi_delay, txi_delay * 30); //could also indicate number of superframes until next VC6 pre-emption
          else fprintf (stderr, "BR Delay: Irrelevant / Send at any time;");
        }
        else fprintf (stderr, " Unk; ");

        if (opts->payload == 1) fprintf (stderr, "\n"); //only during payload

      }

      else if (sbrc_opcode != 0 && sbrc_opcode != 3) //all that should be left in this field is the potential ENC identifiers
      {

        if (slot == 0) //may not need the state->errs anymore //&& state->errs < 3
        {
          if (state->dmr_so & 0x40 && key != 0 && state->payload_keyid == 0)
          {
            //if we aren't forcing a particular alg or privacy key set
            if (state->M == 0)
            {
              fprintf (stderr, "\n");
              fprintf (stderr, "%s", KCYN);
              fprintf (stderr, " Slot 1");
              fprintf (stderr, " DMR LE SB ALG ID: 0x%02X KEY ID: 0x%02X", alg + 0x20, key);
              // fprintf (stderr, "\n");
              fprintf (stderr, "%s ", KNRM);

              state->payload_keyid = key;
              state->payload_algid = alg + 0x20; //assuming DMRA approved alg values (moto patent)
            }

          }
        }

        if (slot == 1) //may not need the state->errs anymore //&& state->errsR < 3
        {
          if (state->dmr_soR & 0x40 && key != 0 && state->payload_keyidR == 0)
          {
            //if we aren't forcing a particular alg or privacy key set
            if (state->M == 0)
            {
              fprintf (stderr, "\n");
              fprintf (stderr, "%s", KCYN);
              fprintf (stderr, " Slot 2");
              fprintf (stderr, " DMR LE SB ALG ID: 0x%02X KEY ID: 0x%02X", alg + 0x20, key);
              // fprintf (stderr, "\n");
              fprintf (stderr, "%s ", KNRM);

              state->payload_keyidR = key;
              state->payload_algidR = alg + 0x20; //assuming DMRA approved alg values (moto patent)
            }

          }
        }

      }

    }
  }

  SBRC_END:

  //'DSP' output to file -- only RC, or RC and SB?
  if (power == 1 && opts->use_dsp_output == 1 && sbrc_hex != 0) //if not NULL
  {
    FILE * pFile; //file pointer
    pFile = fopen (opts->dsp_out_file, "a");
    fprintf (pFile, "\n%d 99 ", slot+1); //'99' is RC designation value
    int k = 0;
    for (i = 0; i < 24; i++) //12 bytes, SB or RC
    {
      //check to see if k++ starts at zero, or at 1
      int sbrc_nib = (state->dmr_embedded_signalling[slot][5][k++] << 3) | (state->dmr_embedded_signalling[slot][5][k++] << 2) | (state->dmr_embedded_signalling[slot][5][k++] << 1) | (state->dmr_embedded_signalling[slot][5][k++] << 0);
      fprintf (pFile, "%X", sbrc_nib);
    }
    fclose (pFile);
  } 

}

uint8_t crc3(uint8_t bits[], unsigned int len)
{
  uint8_t crc=0;
  unsigned int K = 3;
  //x^3+x+1 
  uint8_t poly[4] = {1,1,0,1};
  uint8_t buf[256];
  if (len+K > sizeof(buf)) {
    return 0;
  }
  memset (buf, 0, sizeof(buf));
  for (unsigned int i=0; i<len; i++){
    buf[i] = bits[i];
  }
  for (unsigned int i=0; i<len; i++)
    if (buf[i])
      for (unsigned int j=0; j<K+1; j++)
        buf[i+j] ^= poly[j];
  for (unsigned int i=0; i<K; i++){
    crc = (crc << 1) + buf[len + i];
  }
  return crc;
}

