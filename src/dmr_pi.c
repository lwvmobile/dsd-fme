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
          fprintf (stderr, " Anytone (0x01)");
          state->payload_algid = 0x21;
        }

        fprintf (stderr, "%s ", KNRM);
        if (state->payload_algid != 0x21)
        {
          fprintf (stderr, "\n");
          LFSR64 (state);
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

        fprintf (stderr, "%s ", KNRM);
        if (state->payload_algidR != 0x21)
        {
          fprintf (stderr, "\n");
          LFSR64 (state);
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
    fprintf(stderr, " MI: 0x%08X", lfsr);
    fprintf (stderr, "%s", KNRM);
    state->payload_mi = lfsr;
  }

  if (state->currentslot == 1) 
  {

    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, " Slot 2");
    fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
    fprintf(stderr, " MI: 0x%08X", lfsr);
    fprintf (stderr, "%s", KNRM);
    state->payload_miR = lfsr;
  }
}

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
      fprintf (stderr, " MI: 0x%016llX", lfsr);
      fprintf (stderr, "%s", KNRM);
			state->payload_mi = lfsr & 0xFFFFFFFF; //truncate for next repitition and le verification
		}

		if (state->currentslot == 1)
		{
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 2");
      fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
      fprintf (stderr, " MI: 0x%016llX", lfsr);
      fprintf (stderr, "%s", KNRM);
			state->payload_miR = lfsr & 0xFFFFFFFF; //truncate for next repitition and le verification
		}

	}
}