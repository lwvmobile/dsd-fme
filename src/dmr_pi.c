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

  if((IrrecoverableErrors == 0)) 
  {
    if (state->currentslot == 0)
    {

      if (0 == 0) 
      {
        state->payload_algid = PI_BYTE[0];
        state->payload_keyid = PI_BYTE[2];
      }

      state->payload_mi    = ( ((PI_BYTE[3]) << 24) + ((PI_BYTE[4]) << 16) + ((PI_BYTE[5]) << 8) + (PI_BYTE[6]) );
      if (state->payload_algid < 0x26) 
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "\n Slot 1");
        fprintf (stderr, " DMR PI H- ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algid, state->payload_keyid, state->payload_mi);
        fprintf (stderr, "%s ", KNRM);
      }

      if (state->payload_algid >= 0x26)
      {
        if (0 == 0) 
        {
          state->payload_algid = 0;
          state->payload_keyid = 0;
          state->payload_mi = 0;
        }
      }
    }

    if (state->currentslot == 1)
    {
      if (0 == 0) 
      {
        state->payload_algidR = PI_BYTE[0];
        state->payload_keyidR = PI_BYTE[2];
      }

      state->payload_miR    = ( ((PI_BYTE[3]) << 24) + ((PI_BYTE[4]) << 16) + ((PI_BYTE[5]) << 8) + (PI_BYTE[6]) );
      if (state->payload_algidR < 0x26) 
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "\n Slot 2");
        fprintf (stderr, " DMR PI H- ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algidR, state->payload_keyidR, state->payload_miR);
        fprintf (stderr, "%s ", KNRM);
      }

      if (state->payload_algidR >= 0x26)
      {
        if (0== 0) 
        {
          state->payload_algidR = 0;
          state->payload_keyidR = 0;
          state->payload_miR = 0;
        }
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
    if (1 == 1)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 1");
      fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algid, state->payload_keyid);
      fprintf(stderr, " Next MI: 0x%08X", lfsr);
      fprintf (stderr, "%s", KNRM);
    }
    state->payload_mi = lfsr;
  }

  if (state->currentslot == 1) 
  {
    if (1 == 1)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Slot 2");
      fprintf (stderr, " DMR PI C- ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
      fprintf(stderr, " Next MI: 0x%08X", lfsr);
      fprintf (stderr, "%s", KNRM);
    }
    state->payload_miR = lfsr;
  }
}