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

    if (state->payload_algid >= 0x21)
    {
      LFSR (state);
      fprintf (stderr, "\n");
    }
  }
  if (state->currentslot == 1)
  {
    state->dropR = 256;

    if (state->K1 != 0) 
    {
      state->DMRvcR = 0;
    }

    if (state->payload_algidR >= 0x21)
    {
      LFSR (state);
      fprintf (stderr, "\n");
    }
  }

}

void dmr_alg_reset (dsd_opts * opts, dsd_state * state)
{
  state->dropL = 256;
  state->dropR = 256;
  state->DMRvcL = 0;
  state->DMRvcR = 0; 
  state->payload_miP = 0; 
}

