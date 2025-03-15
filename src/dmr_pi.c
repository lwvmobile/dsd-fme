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
  UNUSED2(opts, CRCCorrect);

  if((IrrecoverableErrors == 0)) 
  {

    //update cc amd vc sync time for trunking purposes (particularly Con+)
    if (opts->p25_is_tuned == 1)
    {
      state->last_vc_sync_time = time(NULL);
      state->last_cc_sync_time = time(NULL);
    } 

    if (state->currentslot == 0)
    {
      state->payload_algid = PI_BYTE[0];
      state->payload_keyid = PI_BYTE[2];
      state->payload_mi    = ( ((PI_BYTE[3]) << 24) + ((PI_BYTE[4]) << 16) + ((PI_BYTE[5]) << 8) + (PI_BYTE[6]) );
      if (state->payload_algid < 0x26) 
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "\n Slot 1");
        fprintf (stderr, " DMR PI H- ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algid, state->payload_keyid, state->payload_mi);

        //Anytone RC4 Shim
        if (state->payload_algid == 0x01)
        {
          fprintf (stderr, " Anytone RC4 (0x01)");
          state->payload_algid = 0x21;
        }

        //Anytone/Hytera AES-128 Shim
        if (state->payload_algid == 0x04)
        {
          fprintf (stderr, " Anytone/Hytera AES-128 (0x04)");
          state->payload_algid = 0x24;
        }

        //Anytone/Hytera AES-256 Shim
        if (state->payload_algid == 0x05)
        {
          fprintf (stderr, " Anytone/Hytera AES-256 (0x05)");
          state->payload_algid = 0x25;
        }

        fprintf (stderr, "%s ", KNRM);

        //expand the 32-bit MI to a 64-bit DES1 IV
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
      state->payload_miR    = ( ((PI_BYTE[3]) << 24) + ((PI_BYTE[4]) << 16) + ((PI_BYTE[5]) << 8) + (PI_BYTE[6]) );
      if (state->payload_algidR < 0x26) 
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "\n Slot 2");
        fprintf (stderr, " DMR PI H- ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algidR, state->payload_keyidR, state->payload_miR);

        //Anytone RC4 Shim
        if (state->payload_algidR == 0x01)
        {
          fprintf (stderr, " Anytone (0x01)");
          state->payload_algidR = 0x21;
        }

        //Anytone/Hytera AES-128 Shim
        if (state->payload_algidR == 0x04)
        {
          fprintf (stderr, " Anytone/Hytera AES-128 (0x04)");
          state->payload_algidR = 0x24;
        }

        //Anytone/Hytera AES-256 Shim
        if (state->payload_algidR == 0x05)
        {
          fprintf (stderr, " Anytone/Hytera AES-256 (0x05)");
          state->payload_algidR = 0x25;
        }

        fprintf (stderr, "%s ", KNRM);

        //expand the 32-bit MI to a 64-bit DES1 IV
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

  }
}

void LFSR(dsd_state * state)
{
  int lfsr = 0;
  if (state->currentslot == 0)
  {
    lfsr = state->payload_mi;
  }
  else lfsr = state->payload_miR;

  uint8_t cnt = 0;

  for(cnt=0;cnt<32;cnt++)
  {
	  // Polynomial is C(x) = x^32 + x^4 + x^2 + 1
    int bit  = ((lfsr >> 31) ^ (lfsr >> 3) ^ (lfsr >> 1)) & 0x1;
    lfsr =  (lfsr << 1) | (bit);
  }

  if (state->currentslot == 0)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 1");
    fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algid, state->payload_keyid);
    fprintf(stderr, " MI(32): 0x%08X", lfsr);
    fprintf (stderr, "%s", KNRM);
    state->payload_mi = lfsr;
  }

  if (state->currentslot == 1) 
  {

    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 2");
    fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
    fprintf(stderr, " MI(32): 0x%08X", lfsr);
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
      fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algid, state->payload_keyid);
      fprintf (stderr, " MI(64): 0x%016llX", lfsr);
      fprintf (stderr, "%s", KNRM);
			state->payload_mi = lfsr & 0xFFFFFFFF; //truncate for next repitition and le verification
      state->payload_miP = lfsr;
      state->DMRvcL = 0;
		}

		if (state->currentslot == 1)
		{
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 2");
      fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
      fprintf (stderr, " MI(64): 0x%016llX", lfsr);
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
    next_mi = (state->aes_iv[4] << 24) + (state->aes_iv[5] << 16) + (state->aes_iv[6] << 8) + (state->aes_iv[7] << 0);
  if (state->currentslot == 1)
    next_mi = (state->aes_ivR[4] << 24) + (state->aes_ivR[5] << 16) + (state->aes_ivR[6] << 8) + (state->aes_ivR[7] << 0);

  if (state->currentslot == 0)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 1");
    fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X MI(128): ", state->payload_algid, state->payload_keyid);
    for (x = 0; x < 16; x++)
      fprintf (stderr, "%02X", state->aes_iv[x]);
    fprintf (stderr, "%s", KNRM);
    // fprintf (stderr, "\n");

    state->payload_mi = next_mi;
    state->DMRvcL = 0;

  }

  if (state->currentslot == 1)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 2");
    fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X MI(128): ", state->payload_algidR, state->payload_keyidR);
    for (x = 0; x < 16; x++)
      fprintf (stderr, "%02X", state->aes_ivR[x]);
    fprintf (stderr, "%s", KNRM);
    // fprintf (stderr, "\n");

    state->payload_miR = next_mi;
    state->DMRvcR = 0;

  }

}