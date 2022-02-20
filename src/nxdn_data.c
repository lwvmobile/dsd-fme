#include "dsd.h"
#include "nxdn_const.h"

//don't know why this doesn't work right in LEH, need to figure that out for him
//works just fien when ripped from his and into mine
//must be some random thing, I'll look at his when I have a chance
void processNXDNData (dsd_opts * opts, dsd_state * state)
{
  int i, dibit = 0;
  unsigned char sacch_raw[72] = {0};
  unsigned char sacch_decoded[32] = {0}; /* 26 bit + 6 bit CRC */
  unsigned char *pr;
  uint8_t CrcIsGood = 0;
  uint8_t StructureField = 0;
  uint8_t RAN = 0;
  uint8_t PartOfFrame = 0;

  /* Remove compiler warning */
  //UNUSED_VARIABLE(dibit);

  if (opts->errorbars == 1)
  {
    fprintf(stderr, "DATA  - ");
  }

  /* Start pseudo-random NXDN sequence after
   * LITCH = 16 bit = 8 dibit
   * ==> Index 8 */
  pr = (unsigned char *)(&nxdnpr2[8]);
  for (i = 0; i < 30; i++)
  {
    dibit = getDibit (opts, state);

    /* Store and de-interleave directly SACCH bits */
    sacch_raw[(2*i)]   = ((*pr) & 1) ^ (1 & (dibit >> 1));  // bit 1
    sacch_raw[(2*i)+1] = (1 & dibit);               // bit 0
    pr++;
#ifdef NXDN_DUMP
    fprintf(stderr, "%c", dibit + 48);
#endif
  }
#ifdef NXDN_DUMP
  fprintf(stderr, " ");
#endif


  /* Decode the SACCH fields */
  CrcIsGood = NXDN_SACCH_raw_part_decode(sacch_raw, sacch_decoded);
  StructureField = (sacch_decoded[0]<<1 | sacch_decoded[1]<<0) & 0x03; /* Compute the Structure Field (remove 2 first bits of the SR Information in the SACCH) */
  RAN = (sacch_decoded[2]<<5 | sacch_decoded[3]<<4 | sacch_decoded[4]<<3 | sacch_decoded[5]<<2 | sacch_decoded[6]<<1 | sacch_decoded[7]<<0) & 0x3F; /* Compute the RAN (6 last bits of the SR Information in the SACCH) */

  /* Compute the Part of Frame according to the structure field */
  if     (StructureField == 3) PartOfFrame = 0;
  else if(StructureField == 2) PartOfFrame = 1;
  else if(StructureField == 1) PartOfFrame = 2;
  else if(StructureField == 0) PartOfFrame = 3;
  else PartOfFrame = 0; /* Normally we should never go here */

  /* Fill the SACCH structure */
  state->NxdnSacchRawPart[PartOfFrame].CrcIsGood = CrcIsGood;
  state->NxdnSacchRawPart[PartOfFrame].StructureField = StructureField;
  state->NxdnSacchRawPart[PartOfFrame].RAN = RAN;

  memcpy(state->NxdnSacchRawPart[PartOfFrame].Data, &sacch_decoded[8], 18); /* Copy the 18 bits of SACCH content */

  //fprintf(stderr, "RAN=%02d - Part %d/4 ", RAN, PartOfFrame + 1);
  if(CrcIsGood)
  {
    fprintf (stderr, "RAN=%02d - Part %d/4 ", RAN, PartOfFrame + 1);
    fprintf (stderr, "   (OK)   - ");
    state->nxdn_last_ran = RAN; //disable, try to grab this in voice instead
  }

  else fprintf (stderr, "(CRC ERR) - ");


  /* Decode the SACCH only when all 4 voice frame
   * SACCH spare parts have been received */
  if(PartOfFrame == 3)
  {
    /* Decode the entire SACCH content */
    NXDN_SACCH_Full_decode(opts, state);
  }

  for (i = 0; i < 144; i++)
  {
    dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
    fprintf(stderr, "%c", dibit + 48);
#endif
  }

  if (opts->errorbars == 1)
  {
    fprintf(stderr, "\n");
  }

  /* Reset the SACCH CRCs when all 4 voice frame
   * SACCH spare parts have been received */
  if(PartOfFrame == 3)
  {
    /* Reset all CRCs of the SACCH */
    for(i = 0; i < 4; i++) state->NxdnSacchRawPart[i].CrcIsGood = 0;
  }
} /* End processNXDNData() */
