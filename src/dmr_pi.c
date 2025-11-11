/*-------------------------------------------------------------------------------
 * dmr_pi.c
 * DMR Privacy Indicator and LFSR Function
 *
 * LFSR code courtesy of https://github.com/mattames/LFSR/
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void dmr_pi (dsd_opts * opts, dsd_state * state, uint8_t PI_BYTE[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors)
{

  uint8_t MFID = PI_BYTE[1];

  if(IrrecoverableErrors == 0)
  {

    //update cc amd vc sync time for trunking purposes (particularly Con+)
    if (opts->p25_is_tuned == 1)
    {
      state->last_vc_sync_time = time(NULL);
      state->last_cc_sync_time = time(NULL);
    }

    if (MFID == 0x0A && CRCCorrect == 1) //Kirisun
    {
      uint8_t so  = PI_BYTE[2]; //in VLC and PI, this byte is 0x40, so thinking this could be SVC_OPT
      uint8_t alg = PI_BYTE[0]; //observed 0x36 and 0x37 for Kirisun Advanced and Universal Privacy
      // uint8_t kid = PI_BYTE[2]; //user info conveyed there is only 16 allotments for Key, and keys are channel specific, so no key id value
      uint32_t target = ((unsigned long long int)PI_BYTE[7] << 16) | ((unsigned long long int)PI_BYTE[8] << 8)  | ((unsigned long long int)PI_BYTE[9] << 0);

      //TODO: Use ALG and TGT value here to produce a key id via hashing it
      uint8_t hash = alg * target % 256; //more complex hash later (honestly, probably not)

      //MI only appears to be 32-bit
      uint32_t mi = ((uint32_t)PI_BYTE[3] << 24) | ((uint32_t)PI_BYTE[4] << 16) | 
                    ((uint32_t)PI_BYTE[5] << 8)  | ((uint32_t)PI_BYTE[6] << 0);

      if (state->currentslot == 0)
      {
        state->dmr_so = so;
        state->payload_algid = alg; 
        state->payload_keyid = hash;
        state->payload_mi = mi;
      }
      else
      {
        state->dmr_soR = so;
        state->payload_algidR = alg;
        state->payload_keyidR = hash;
        state->payload_miR = mi;
      }

      fprintf (stderr, "%s ", KYEL);
      fprintf (stderr, "\n Slot %d", state->currentslot+1);
      fprintf (stderr, " DMR PI H- ALG ID: %02X; KEY ID: %02X; MI(32): %08X;", alg, hash, mi);

      fprintf (stderr, " Kirisun ");
      if (alg == 0x36)
        fprintf (stderr, "Advanced;");
      else if (alg == 0x37)
        fprintf (stderr, "Universal;");
      else fprintf (stderr, "Encryption;");
      fprintf (stderr, "%s", KNRM);

      //disable late entry for DMRA, Flag 3 for future check of VC-F 48-bit region with Golay 24,12 Encoding
      //SEE: https://patents.google.com/patent/CN102307075A/en?q=(kirisun)&q=(dmr)&oq=kirisun
      //tested patent above with information provided, VC-F had a SB, but the 48-bit
      //full value yielded bad Golay results, and user submitted info contradicts this patent for samples provided
      //Late Entry MI does appear to work, however, and reports a good CRC for that as well,
      //reverse engineered LFSR for Kirisun lines up with the late entry for 32-bit MI values.
      opts->dmr_le = 3;

    }

    if (MFID == 0x68) //Hytera Enhanced
    {
      if (state->currentslot == 0)
      {
        state->dmr_so |= 0x40; //OR the enc bit onto the SO
        state->payload_algid = PI_BYTE[0];
        state->payload_keyid = PI_BYTE[2];
        state->payload_mi = ((unsigned long long int)PI_BYTE[3] << 32ULL) | ((unsigned long long int)PI_BYTE[4] << 24) | 
        ((unsigned long long int)PI_BYTE[5] << 16) | ((unsigned long long int)PI_BYTE[6] << 8) | ((unsigned long long int)PI_BYTE[7] << 0);
      }
      else
      {
        state->dmr_soR |= 0x40; //OR the enc bit onto the SO
        state->payload_algidR = PI_BYTE[0];
        state->payload_keyidR = PI_BYTE[2];
        state->payload_miR = ((unsigned long long int)PI_BYTE[3] << 32ULL) | ((unsigned long long int)PI_BYTE[4] << 24) | 
        ((unsigned long long int)PI_BYTE[5] << 16) | ((unsigned long long int)PI_BYTE[6] << 8) | ((unsigned long long int)PI_BYTE[7] << 0);
      }

      fprintf (stderr, "%s ", KYEL);
      fprintf (stderr, "\n Slot %d", state->currentslot+1);
      fprintf (stderr, " DMR PI H- ALG ID: %02X; KEY ID: %02X; MI(40): %02X%02X%02X%02X%02X;", 
      PI_BYTE[0], PI_BYTE[2], PI_BYTE[3], PI_BYTE[4], PI_BYTE[5], PI_BYTE[6], PI_BYTE[7]);

      //PI_BYTE[9] is a checksum of the other bytes combined
      uint8_t checksum = 0;
      for (int i = 0; i < 9; i++)
      {
        checksum += PI_BYTE[i];
        checksum &= 0xFF;
      }
      checksum = ~checksum & 0xFF;
      checksum++;

      //debug
      // fprintf (stderr, " CHK: %02X / %02X;", checksum, PI_BYTE[9]);

      if (checksum == PI_BYTE[9])
      {
        fprintf (stderr, " Hytera Enhanced; ");

        if (state->currentslot == 0 && state->R != 0)
          fprintf (stderr, "Key: %010llX; ", state->R);

        if (state->currentslot == 1 && state->RR != 0)
          fprintf (stderr, "Key: %010llX; ", state->RR);

        //disable late entry for DMRA (hopefully, there aren't any systems running both DMRA and Hytera Enhanced mixed together)
        opts->dmr_le = 2;

        // fprintf (stderr, " (Checksum Okay);");
      }
      else
      {
        fprintf (stderr, "%s", KRED);
        fprintf (stderr, " (Checksum Err);");
      }

      fprintf (stderr, "%s", KNRM);

    }

    else if (MFID == 0x10) //DMRA
    {

      if (state->currentslot == 0)
      {
        state->payload_algid = PI_BYTE[0];
        state->payload_keyid = PI_BYTE[2];
        state->payload_mi    = ((unsigned long long int)PI_BYTE[3] << 24ULL) | ((unsigned long long int)PI_BYTE[4] << 16ULL) | ((unsigned long long int)PI_BYTE[5] << 8ULL) | ((unsigned long long int)PI_BYTE[6] << 0ULL);
        if (state->payload_algid < 0x26)
        {
          fprintf (stderr, "%s ", KYEL);
          fprintf (stderr, "\n Slot 1");
          fprintf (stderr, " DMR PI H- ALG ID: %02X; KEY ID: %02X; MI(32): %08llX;", state->payload_algid, state->payload_keyid, state->payload_mi);

          //check for any values that aren't 0x2X but just 0x0X
          //going to be very generic here to avoid any particular vendors using 0x2X and not 0x0X
          if (state->payload_algid & 0x20)
            fprintf (stderr, " DMRA");
          else fprintf (stderr, " DMRA Compatible");
            
          if ((state->payload_algid & 0x07) == 0x01)
          {
            fprintf (stderr, " RC4;");
            state->payload_algid = 0x21;
          }

          else if ((state->payload_algid & 0x07) == 0x02)
          {
            fprintf (stderr, " DES;");
            state->payload_algid = 0x22;
          }

          else if ((state->payload_algid & 0x07) == 0x04)
          {
            fprintf (stderr, " AES-128;");
            state->payload_algid = 0x24;
          }

          else if ((state->payload_algid & 0x07) == 0x05)
          {
            fprintf (stderr, " AES-256;");
            state->payload_algid = 0x25;
          }

          fprintf (stderr, "%s ", KNRM);

          //expand the 32-bit MI to a 64-bit DES IV
          if (state->payload_algid == 0x22)
          {
            fprintf (stderr, "\n");
            LFSR64 (state);
          }

          //expand the 32-bit MI to a 128-bit AES IV
          if (state->payload_algid == 0x24 || state->payload_algid == 0x25)
          {
            fprintf (stderr, "\n");
            LFSR128d (state);
          }
        }

        if (state->payload_algid >= 0x26)
        {
          state->payload_algid = 0;
          state->payload_keyid = 0;
          state->payload_mi = 0;
        }
      }

      if (state->currentslot == 1)
      {

        state->payload_algidR = PI_BYTE[0];
        state->payload_keyidR = PI_BYTE[2];
        state->payload_miR    = ((unsigned long long int)PI_BYTE[3] << 24ULL) | ((unsigned long long int)PI_BYTE[4] << 16ULL) | ((unsigned long long int)PI_BYTE[5] << 8ULL) | ((unsigned long long int)PI_BYTE[6] << 0ULL);
        if (state->payload_algidR < 0x26)
        {
          fprintf (stderr, "%s ", KYEL);
          fprintf (stderr, "\n Slot 2");
          fprintf (stderr, " DMR PI H- ALG ID: %02X; KEY ID: %02X; MI(32): %08llX;", state->payload_algidR, state->payload_keyidR, state->payload_miR);

          //check for any values that aren't 0x2X but just 0x0X
          //going to be very generic here to avoid any particular vendors using 0x2X and not 0x0X
          if (state->payload_algidR & 0x20)
            fprintf (stderr, " DMRA");
          else fprintf (stderr, " DMRA Compatible");
            
          if ((state->payload_algidR & 0x07) == 0x01)
          {
            fprintf (stderr, " RC4;");
            state->payload_algidR = 0x21;
          }

          else if ((state->payload_algidR & 0x07) == 0x02)
          {
            fprintf (stderr, " DES;");
            state->payload_algidR = 0x22;
          }

          else if ((state->payload_algidR & 0x07) == 0x04)
          {
            fprintf (stderr, " AES-128;");
            state->payload_algidR = 0x24;
          }

          else if ((state->payload_algidR & 0x07) == 0x05)
          {
            fprintf (stderr, " AES-256;");
            state->payload_algidR = 0x25;
          }


          fprintf (stderr, "%s ", KNRM);

          //expand the 32-bit MI to a 64-bit DES IV
          if (state->payload_algidR == 0x22)
          {
            fprintf (stderr, "\n");
            LFSR64 (state);
          }

          //expand the 32-bit MI to a 128-bit AES IV
          if (state->payload_algidR == 0x24 || state->payload_algidR == 0x25)
          {
            fprintf (stderr, "\n");
            LFSR128d (state);
          }
        }

        if (state->payload_algidR >= 0x26)
        {
          state->payload_algidR = 0;
          state->payload_keyidR = 0;
          state->payload_miR = 0;
        }

      }

    } //end DMRA

  }
}

void LFSR(dsd_state * state)
{
  unsigned long long int lfsr = 0;
  if (state->currentslot == 0)
  {
    lfsr = state->payload_mi;
  }
  else lfsr = state->payload_miR;

  uint8_t cnt = 0;

  for(cnt=0;cnt<32;cnt++)
  {
	  // Polynomial is C(x) = x^32 + x^4 + x^2 + 1
    unsigned long long int bit  = ((lfsr >> 31) ^ (lfsr >> 3) ^ (lfsr >> 1)) & 0x1;
    lfsr =  (lfsr << 1) | bit;
  }

  lfsr &= 0xFFFFFFFF;

  if (state->currentslot == 0)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 1");
    fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X;", state->payload_algid, state->payload_keyid);
    fprintf (stderr, " MI(32): %08llX;", lfsr);
    fprintf (stderr, " RC4;");
    fprintf (stderr, "%s", KNRM);
    state->payload_mi = lfsr;
  }

  if (state->currentslot == 1)
  {

    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 2");
    fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X;", state->payload_algidR, state->payload_keyidR);
    fprintf(stderr, " MI(32): %08llX;", lfsr);
    fprintf (stderr, " RC4;");
    fprintf (stderr, "%s", KNRM);
    state->payload_miR = lfsr;
  }
}

//Expand a 32-bit MI into a 64-bit IV for DES
void LFSR64(dsd_state * state)
{
  {
    unsigned long long int lfsr = 0;

    if (state->currentslot == 0)
    {
      lfsr = (uint64_t) state->payload_mi;
    }
    else lfsr = (uint64_t) state->payload_miR;

    uint8_t cnt = 0;

    for(cnt=0;cnt<32;cnt++)
    {
      unsigned long long int bit = ( (lfsr >> 31) ^ (lfsr >> 21) ^ (lfsr >> 1) ^ (lfsr >> 0) ) & 0x1;
      lfsr = (lfsr << 1) | bit;
    }

    if (state->currentslot == 0)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 1");
      fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X;", state->payload_algid, state->payload_keyid);
      fprintf (stderr, " MI(64): %016llX;", lfsr);
      fprintf (stderr, " DES;");
      fprintf (stderr, "%s", KNRM);
      state->payload_mi = lfsr & 0xFFFFFFFF; //truncate for next repitition and le verification
      state->payload_miP = lfsr;
      state->DMRvcL = 0;
    }

    if (state->currentslot == 1)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 2");
      fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X;", state->payload_algidR, state->payload_keyidR);
      fprintf (stderr, " MI(64): %016llX;", lfsr);
      fprintf (stderr, " DES;");
      fprintf (stderr, "%s", KNRM);
      state->payload_miR = lfsr & 0xFFFFFFFF; //truncate for next repitition and le verification
      state->payload_miN = lfsr;
      state->DMRvcR = 0;
    }

  }
}


//Expand a 32-bit MI into a 128-bit IV for AES
void LFSR128d(dsd_state * state)
{
  unsigned long long int lfsr = 0;

  if (state->currentslot == 0)
    lfsr = state->payload_mi;
  else lfsr = state->payload_miR;

  unsigned long long int next_mi;

  //start packing aes_iv
  if (state->currentslot == 0)
  {
    state->aes_iv[0] = (lfsr >> 24) & 0xFF;
    state->aes_iv[1] = (lfsr >> 16) & 0xFF;
    state->aes_iv[2] = (lfsr >> 8 ) & 0xFF;
    state->aes_iv[3] = (lfsr >> 0 ) & 0xFF;
  }
  else if (state->currentslot == 1)
  {
    state->aes_ivR[0] = (lfsr >> 24) & 0xFF;
    state->aes_ivR[1] = (lfsr >> 16) & 0xFF;
    state->aes_ivR[2] = (lfsr >> 8 ) & 0xFF;
    state->aes_ivR[3] = (lfsr >> 0 ) & 0xFF;
  }

  int cnt = 0; int x = 32;
  unsigned long long int bit;
  for(cnt=0;cnt<96;cnt++)
  {
    //32,22,2,1
    bit = ( (lfsr >> 31) ^ (lfsr >> 21) ^ (lfsr >> 1) ^ (lfsr >> 0) ) & 0x1;
    lfsr = (lfsr << 1) | bit;

    //continue packing aes_iv
    if (state->currentslot == 0)
      state->aes_iv[x/8] = (state->aes_iv[x/8] << 1) + bit;
    else if (state->currentslot == 1)
      state->aes_ivR[x/8] = (state->aes_ivR[x/8] << 1) + bit;
    x++;
  }

  //assign the next 32-bit short MI from 4,5,6,7 so it'll match up with OTA late entry
  if (state->currentslot == 0)
    next_mi = ((unsigned long long int)state->aes_iv[4] << 24) | ((unsigned long long int)state->aes_iv[5] << 16) | 
              ((unsigned long long int)state->aes_iv[6] << 8)  | ((unsigned long long int)state->aes_iv[7] << 0);
  if (state->currentslot == 1)
    next_mi = ((unsigned long long int)state->aes_ivR[4] << 24) | ((unsigned long long int)state->aes_ivR[5] << 16) | 
              ((unsigned long long int)state->aes_ivR[6] << 8)  | ((unsigned long long int)state->aes_ivR[7] << 0);

  if (state->currentslot == 0)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 1");
    fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X; MI(128): ", state->payload_algid, state->payload_keyid);
    for (x = 0; x < 16; x++)
      fprintf (stderr, "%02X", state->aes_iv[x]);
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, ";");

    if (state->payload_algid == 0x24)
      fprintf (stderr, " AES-128;");
    else fprintf (stderr, " AES-256;");

    state->payload_mi = next_mi;
    state->DMRvcL = 0;

  }

  if (state->currentslot == 1)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 2");
    fprintf (stderr, " DMR PI C- ALG ID: %02X; KEY ID: %02X; MI(128): ", state->payload_algidR, state->payload_keyidR);
    for (x = 0; x < 16; x++)
      fprintf (stderr, "%02X", state->aes_ivR[x]);
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, ";");
    
    if (state->payload_algidR == 0x24)
      fprintf (stderr, " AES-128;");
    else fprintf (stderr, " AES-256;");

    state->payload_miR = next_mi;
    state->DMRvcR = 0;

  }

}

unsigned long long int hytera_lfsr(uint8_t * mi, uint8_t * taps, uint8_t len)
{

  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t bit = (mi[i] >> 7) & 1;
    mi[i] <<= 1;
    if (bit) mi[i] ^= taps[i%len];
    mi[i] |= bit;
    
  }

  unsigned long long int mi_value = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    mi_value <<= 8;
    mi_value |= mi[i];
  }

  //debug
  // fprintf (stderr, " Next MI: %010llX \n", mi_value);

  return mi_value;
}

void hytera_enhanced_alg_refresh(dsd_state * state)
{
  uint8_t mi[5]; memset (mi, 0, sizeof(mi));
  unsigned long long int mi_value = 0;
  if (state->currentslot == 0)
    mi_value = state->payload_mi;
  else mi_value = state->payload_miR;

  //load mi_value into mi array
  mi[0] = ((mi_value & 0xFF00000000) >> 32UL);
  mi[1] = ((mi_value & 0xFF000000) >> 24);
  mi[2] = ((mi_value & 0xFF0000) >> 16);
  mi[3] = ((mi_value & 0xFF00) >> 8);
  mi[4] = ((mi_value & 0xFF) >> 0);

  //calculate the next MI value
  uint8_t taps[5]; memset(taps, 0, sizeof(taps));

  //the tap values
  taps[0] = 0x12;
  taps[1] = 0x24;
  taps[2] = 0x48;
  taps[3] = 0x22;
  taps[4] = 0x14;  
  mi_value = hytera_lfsr(mi, taps, 5);

  if (state->currentslot == 0)
    state->payload_mi = mi_value;
  else state->payload_miR = mi_value;
}

uint32_t kirisun_lfsr(unsigned long long int mi)
{

  uint32_t taps = 0xD459C4F1;
  uint32_t lfsr = (uint32_t)mi;
  uint32_t new_mi = 0;

  for (int i = 0; i < 4; i++) 
  {

    uint8_t byte = 0;

    for (int j = 0; j < 8; j++) 
    {
      uint32_t temp = (lfsr << 1);
      uint8_t  msb  = (lfsr >> 31) & 1;

      if (msb) 
      {
        byte |= (1 << j);
        lfsr = temp ^ taps;
      }
      else lfsr = temp; //was previously temp ^ 1, but that isn't a primitive
    }

    new_mi <<=8;
    new_mi |= byte;

  }

  return new_mi;

}
