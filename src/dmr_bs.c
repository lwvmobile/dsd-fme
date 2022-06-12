#include "dsd.h"
#include "dmr_const.h"

//A subroutine for processing each TDMA frame individually to allow for
//processing voice and/or data on both BS slots (channels) simultaneously
void dmrBS (dsd_opts * opts, dsd_state * state)
{
  int i, j, k, l, dibit;
  int *dibit_p;
  char ambe_fr[4][24] = {0};
  char ambe_fr2[4][24] = {0};
  char ambe_fr3[4][24] = {0};
  char t_ambe_fr[4][24] = {9};
  char t_ambe_fr2[4][24] = {9};
  char t_ambe_fr3[4][24] = {9};
  const int *w, *x, *y, *z;
  char sync[25] = {0};
  char syncdata[48] = {0};
  char cachdata[13] = {0};
  int mutecurrentslot;
  int msMode;
  char cc[4];
  unsigned char EmbeddedSignalling[16] = {0};
  int EmbeddedSignallingOk;
  unsigned int internalcolorcode;
  int internalslot;
  char redundancyA[36];
  char redundancyB[36];
  unsigned short int vc1;
  unsigned short int vc2;

  //add time to mirror printFrameSync
  time_t now;
  char * getTime(void) //get pretty hh:mm:ss timestamp
  {
    time_t t = time(NULL);

    char * curr;
    char * stamp = asctime(localtime( & t));

    curr = strtok(stamp, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");

    return curr;
  }

  //Init slot lights
  sprintf (state->slot1light, " slot1 ");
  sprintf (state->slot2light, " slot2 ");

  //Init the color code status
  state->color_code_ok = 0;

  vc1 = 2;
  vc2 = 2;

  short int loop = 1;
  short int skipcount = 0;

  //Run Loop while the getting is good
  while (loop == 1) {

  // CACH
  internalslot = -1; //reset here so we know if this value is being set properly
  for(i = 0; i < 12; i++)
  {
    dibit = getDibit(opts, state);
    cachdata[i] = dibit;
    state->dmr_stereo_payload[i] = dibit;
    //fprintf (stderr, "D%X ", dibit);
    //fprintf (stderr, "P%X ", state->dmr_stereo_payload[i]);
    if(i == 2)
    {
      state->currentslot = (1 & (dibit >> 1));
      internalslot = (1 & (dibit >> 1));
    }
  }

  //Setup for first AMBE Frame
  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //First AMBE Frame, Full 36
  for(i = 0; i < 36; i++)
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i+12] = dibit;
    redundancyA[i] = dibit;
    ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //check for repetitive data if caught in a 'no carrier' loop? Just picking random values.
  //this will test for no carrier (input signal) and return us to no sync state if necessary
  if (redundancyA[16] == redundancyB[16] && redundancyA[27] == redundancyB[27] &&
      redundancyA[01] == redundancyB[01] && redundancyA[32] == redundancyB[32] &&
      redundancyA[03] == redundancyB[03] && redundancyA[33] == redundancyB[33] &&
      redundancyA[13] == redundancyB[13] && redundancyA[07] == redundancyB[07]    )
  {
    goto END;
  }
  // end redundancy test, set B to A
  for(i = 0; i < 36; i++)
  {
    redundancyB[i] = redundancyA[i];
  }

  //Setup for Second AMBE Frame
  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //Second AMBE Frame, First Half 18 dibits just before Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i+48] = dibit;
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //short int l = 0; //do I still use this?
  // signaling data or sync
  for(i = 0; i < 24; i++)
  {
    dibit = getDibit(opts, state);

    state->dmr_stereo_payload[i+66] = dibit;
    syncdata[(2*i)]   = (1 & (dibit >> 1));  // bit 1
    syncdata[(2*i)+1] = (1 & dibit);         // bit 0
    sync[i] = (dibit | 1) + 48;

    if (i < 4 || i > 19) //
    {
      EmbeddedSignalling[2*i]   = (1 & (dibit >> 1));  // bit 1
      EmbeddedSignalling[2*i+1] = (1 & dibit);         // bit 0
    }

    //Used for ProcessVoiceBurstSync
    if(internalslot == 0 && vc1 > 1 && vc1 < 6) //grab on vc1 values 2-5 B C D and E
    {
      /* Time slot 1 superframe buffer filling => SYNC data */
      state->TS1SuperFrame.TimeSlotRawVoiceFrame[vc1-1].Sync[i*2]   = (1 & (dibit >> 1)); // bit 1
      state->TS1SuperFrame.TimeSlotRawVoiceFrame[vc1-1].Sync[i*2+1] = (1 & dibit);        // bit 0
    }
    if(internalslot == 1 && vc2 > 1 && vc2 < 6) //grab on vc2 values 2-5 B C D and E
    {
      /* Time slot 2 superframe buffer filling => SYNC data */
      state->TS2SuperFrame.TimeSlotRawVoiceFrame[vc2-1].Sync[i*2]   = (1 & (dibit >> 1)); // bit 1
      state->TS2SuperFrame.TimeSlotRawVoiceFrame[vc2-1].Sync[i*2+1] = (1 & dibit);        // bit 0
    }
  }

  sync[24] = 0;

  EmbeddedSignallingOk = -1;
  if(QR_16_7_6_decode(EmbeddedSignalling))
  {
    EmbeddedSignallingOk = 1;
  }

  internalcolorcode = 69; //set so we know if this value is being set properly

  if( EmbeddedSignallingOk == 1 ) //don't set on 1?
  {
    //state->color_code = (unsigned int)((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) + EmbeddedSignalling[3]);
    internalcolorcode = (unsigned int)((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) + EmbeddedSignalling[3]);
    state->color_code_ok = EmbeddedSignallingOk;

    //Power Indicator, not the other PI (header)
    state->PI = (unsigned int)EmbeddedSignalling[4];
    state->PI_ok = EmbeddedSignallingOk;

    //Link Control Start Stop Indicator
    state->LCSS = (unsigned int)((EmbeddedSignalling[5] << 1) + EmbeddedSignalling[6]);
    state->LCSS_ok = EmbeddedSignallingOk;

  }
  else skipcount++;

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      //dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+90] = dibit;
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //Setup for Third AMBE Frame
  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //Third AMBE Frame, Full 36
  for(i = 0; i < 36; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      //dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+108] = dibit;
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //reset vc counters to 1 if new voice sync frame on each slot
  if ( strcmp (sync, DMR_BS_VOICE_SYNC) == 0)
  {
    if (internalslot == 0)
    {
      vc1 = 1;
      state->dropL = 256;
    }
    if (internalslot == 1)
    {
      vc2 = 1;
      state->dropR = 256;
    }
  }

  //check for sync pattern here after collected the rest of the payload, decide what to do with it
  if ( strcmp (sync, DMR_BS_DATA_SYNC) == 0 )
  {
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");

      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  ");
      }
      else fprintf (stderr,"Sync: -DMR  ");
      //constantly reset the vc counter to 1 each data frame in anticipation of new voice frame
      vc1 = 1;
      //state->dropL = 256;
    }
    if (internalslot == 1)
    {
      sprintf(state->slot2light, "[slot2]");
      sprintf(state->slot1light, " slot1 ");

      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  ");
      }
      else fprintf (stderr,"Sync: -DMR  ");
      //constantly reset the vc counter to 1 each data frame in anticipation of new voice frame
      vc2 = 1;
      //state->dropR = 256;
    }
    processDMRdata (opts, state);
    skipcount++; //after 2 data frames, drop back to getFrameSync and process subsequent data with processDMRdata
    goto SKIP;
  }

  //find way to mitigate or correct coming back here after leaving in inverted signal
  if ( strcmp (sync, DMR_BS_DATA_SYNC) == 0 && opts->inverted_dmr == 1)
  {
    skipcount++;
    goto SKIP;
  }

  //only play voice on no data sync
  if (strcmp (sync, DMR_BS_DATA_SYNC) != 0)
  {
    if (EmbeddedSignallingOk == 0)
    {
      //goto END;
    }
    skipcount = 0; //reset skip count if processing voice frames
    fprintf (stderr,"%s ", getTime());

    if (internalslot == 0)
    {
      state->dmrburstL = 16; //use 16 for Voice?
      //fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | VC%d \n", state->dmr_color_code, vc1);
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  [SLOT1]  slot2  |               | DMRSTEREO | VC%d ",vc1);
        if (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf(stderr, " BPK %lld", state->K);
          fprintf (stderr, "%s", KNRM);
        }
        //the condition below should be swapped to vc6 and not on vc1, placement after processMbeFrame
        if (vc1 == 1 && state->payload_algid != 0 && opts->payload == 1)
        {
          //LFSR(state); //placement here causes issues when second slot becomes active as well
        }
        //fprintf (stderr, "\n");
      }
      else
      {
        fprintf (stderr,"Sync: -DMR  [SLOT1]  slot2  |               | DMRSTEREO | VC%d ",vc1);
        if (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf(stderr, " BPK %lld", state->K);
          fprintf (stderr, "%s", KNRM);
        }
        //the condition below should be swapped to vc6 and not on vc1, placement after processMbeFrame
        if (vc1 == 1 && state->payload_algid != 0 && opts->payload == 1)
        {
          //LFSR(state); //placement here causes issues when second slot becomes active as well
        }
        //fprintf (stderr, "\n");
      }
    }

    if (internalslot == 1)
    {
      state->dmrburstR = 16; //use 16 for Voice?
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR   slot1  [SLOT2] |               | DMRSTEREO | VC%d ",vc2);
        if (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf(stderr, " BPK %lld", state->K);
          fprintf (stderr, "%s", KNRM);
        }
        //the condition below should be swapped to vc6 and not on vc1, placement after processMbeFrame
        if (vc2 == 1 && state->payload_algidR != 0 && opts->payload == 1)
        {
          //LFSR(state);
        }
        //fprintf (stderr, "\n");
      }
      else
      {
        fprintf (stderr,"Sync: -DMR   slot1  [SLOT2] |               | DMRSTEREO | VC%d ",vc2);
        if (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf(stderr, " BPK %lld", state->K);
          fprintf (stderr, "%s", KNRM);
        }
        //the condition below should be swapped to vc6 and not on vc1, placement after processMbeFrame
        if (vc2 == 1 && state->payload_algidR != 0 && opts->payload == 1)
        {
          //LFSR(state);
        }
        //fprintf (stderr, "\n");
      }
    }
    if (internalslot == 0 && vc1 == 6) //presumably when full (and no sync issues)
    {
      //process voice burst
      ProcessVoiceBurstSync(opts, state);
      fprintf (stderr, "\n");
    }

    if (internalslot == 1 && vc2 == 6) //presumably when full (and no sync issues)
    {
      //process voice burst
      ProcessVoiceBurstSync(opts, state);
      fprintf (stderr, "\n");
    }

    //test printing ambe_fr values
    /*
    if (1 == 1)
    {
      fprintf (stderr, "\nAMBE_FR1 = ");
      for (j = 0; j < 4; j++)
      {
        fprintf (stderr, " - ");
        for (k = 0; k < 12;)
        {
          int b = 0;
          int c = 0;
          int d = 0;
          int e = 0;
          b = ambe_fr[j][k];
          c = ambe_fr[j][k+1];
          d = ambe_fr[j][k+2];
          e = ambe_fr[j][k+3];
          //fprintf (stderr, "%X", (((uint8_t)ambe_fr[k] << 2) + ambe_fr[k+1]) );
          fprintf (stderr, "%01X", ( (b << 3) + (c << 2) + (d << 1) + e ) );
          k = k + 4;
        }
      }
      fprintf (stderr, "\nAMBE_FR2 = ");
      for (j = 0; j < 4; j++)
      {
        fprintf (stderr, " - ");
        for (k = 0; k < 12;)
        {
          int b = 0;
          int c = 0;
          int d = 0;
          int e = 0;
          b = ambe_fr2[j][k];
          c = ambe_fr2[j][k+1];
          d = ambe_fr2[j][k+2];
          e = ambe_fr2[j][k+3];
          //fprintf (stderr, "%X", (((uint8_t)ambe_fr[k] << 2) + ambe_fr[k+1]) );
          fprintf (stderr, "%01X", ( (b << 3) + (c << 2) + (d << 1) + e ) );
          k = k + 4;
        }
      }
      fprintf (stderr, "\nAMBE_FR3 = ");
      for (j = 0; j < 4; j++)
      {
        fprintf (stderr, " - ");
        for (k = 0; k < 12;)
        {
          int b = 0;
          int c = 0;
          int d = 0;
          int e = 0;
          b = ambe_fr3[j][k];
          c = ambe_fr3[j][k+1];
          d = ambe_fr3[j][k+2];
          e = ambe_fr3[j][k+3];
          //fprintf (stderr, "%X", (((uint8_t)ambe_fr[k] << 2) + ambe_fr[k+1]) );
          fprintf (stderr, "%01X", ( (b << 3) + (c << 2) + (d << 1) + e ) );
          k = k + 4;
        }
      }

      fprintf (stderr, "\n");

    }
    */
    //end testing fr values
    processMbeFrame (opts, state, NULL, ambe_fr, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr3, NULL);

    //We only want to run LFSR after processing VC6 frames and before rolling
    //back to VC1, otherwise we will incur an unwanted LFSR if voice becomes active
    //in both slots while in this loop
    if (internalslot == 0 && vc1 == 6)
    {
      state->dropL = 256;
      //if (vc1 == 1 && state->payload_algid != 0 && opts->payload == 1)
      if (state->payload_algid != 0 && opts->payload == 1)
      {
        LFSR(state);
        fprintf (stderr, "\n");
      }

    }

    if (internalslot == 1 && vc2 == 6)
    {
      state->dropR = 256;
      if (state->payload_algidR != 0 && opts->payload == 1)
      {
        LFSR(state);
        fprintf (stderr, "\n");
      }

    }

    if (internalslot == 0)
    {
      vc1++;
    }
    if (internalslot == 1)
    {
      vc2++;
    }
  }

  // recalibrate center/umid/lmid, not sure this helps or does anything particularly in this case
  state->center = ((state->max) + (state->min)) / 2;
  state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
  state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;

  //Escape conditions to break the loop
  //7 seems optimal, allows for an extra frame each in case of late entry on dual voices
  if ( (vc1 > 7 && vc2 > 7) )
  {
    goto END;
  }
  //14 probably allows a few more in case of a bit of jitter but eventually breaks loop (1 second?)
  if ( (vc1 > 14 || vc2 > 14) )
  {
    goto END;
  }
  //using these conditions may cause excessive resyncs IF bad signal,
  //but still better than getting stuck in a wonk wonk loop for too long.
  //set for more aggressive or less aggressive resync during accumulated playback errs
  if (opts->aggressive_framesync == 1)
  {
    //errors caused due to playing MBE files out of sync, break loop
    if (state->errs > 2 || state->errsR > 2)
    {
      //goto END; //
    }
    //errors caused due to playing MBE files out of sync, break loop
    if (state->errs2 > 4 || state->errs2R > 4)
    {
      goto END; //
    }
  }

  //earlier skip conditions teleport us here to process further skip conditions before restarting while loop
  SKIP:
  if (skipcount > 2) //after 2 consecutive data frames, drop back to getFrameSync and process with processDMRdata
  {
    goto END;
  }

  //since we are in a while loop, run ncursesPrinter here.
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

 } // while loop
 //Teleport here on END condition, set or reset variables and return to look for new frame sync
 END:
 state->dmr_stereo = 0;
 state->errs2R = 0;
 state->errs2 = 0;
//

//
}

//Process buffered half frame and 2nd half and then jump to full BS decoding
void dmrBSBootstrap (dsd_opts * opts, dsd_state * state)
{
  int i, j, k, l, dibit;
  int *dibit_p;
  char ambe_fr[4][24] = {0};
  char ambe_fr2[4][24] = {0};
  char ambe_fr3[4][24] = {0};
  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[48];
  char cachdata[13] = {0};
  int mutecurrentslot;
  int msMode;
  char cc[4];
  unsigned char EmbeddedSignalling[16];
  unsigned int EmbeddedSignallingOk;

  //add time to mirror printFrameSync
  time_t now;
  char * getTime(void) //get pretty hh:mm:ss timestamp
  {
    time_t t = time(NULL);

    char * curr;
    char * stamp = asctime(localtime( & t));

    curr = strtok(stamp, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");

    return curr;
  }

  //payload buffer
  //CACH + First Half Payload + Sync = 12 + 54 + 24
  dibit_p = state->dibit_buf_p - 90; //this seems to work okay for both
  //dibit_p = state->dmr_payload_p - 90;
  for (i = 0; i < 90; i++) //90
  {
    dibit = *dibit_p;
    dibit_p++;
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i] = dibit;
  }

  for(i = 0; i < 12; i++)
  {
    dibit = state->dmr_stereo_payload[i];
    cachdata[i] = dibit;
    if(i == 2)
    {
      state->currentslot = (1 & (dibit >> 1));
    }
  }

  //Setup for first AMBE Frame

  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //First AMBE Frame, Full 36
  for(i = 0; i < 36; i++)
  {
    dibit = state->dmr_stereo_payload[i+12];
    ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //Setup for Second AMBE Frame

  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //Second AMBE Frame, First Half 18 dibits just before Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = state->dmr_stereo_payload[i+48];
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  // signaling data or sync, just redo it
  for(i = 0; i < 24; i++)
  {
    dibit = state->dmr_stereo_payload[i+66];
    sync[i] = (dibit | 1) + 48;
  }
  sync[24] = 0;
  //this method seems to work to make sure we aren't in a data sync especially when inverted signal
  //consider only testing this when inverted signal if this causes issues with sync later on
  if ( strcmp (sync, DMR_BS_VOICE_SYNC) != 0)
  {
    goto END;
  }

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //Setup for Third AMBE Frame

  //Interleave Schedule
  w = rW;
  x = rX;
  y = rY;
  z = rZ;

  //Third AMBE Frame, Full 36
  for(i = 0; i < 36; i++)
  {
    dibit = getDibit(opts, state);
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  fprintf (stderr,"%s ", getTime());
  if (opts->inverted_dmr == 0)
  {
    fprintf (stderr,"Sync: +DMR                  |  Frame Sync   | DMRSTEREO | VC1 ");
    if ( (state->K > 0 && state->dmr_so  & 0x40 && state->payload_keyid  == 0) ||
         (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0) )
    {
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " BPK %lld", state->K);
      fprintf (stderr, "%s", KNRM);
      //state->currentslot = 0; //slot now set from cach data from buffer
    }
    fprintf (stderr, "\n");
  }
  else
  {
    fprintf (stderr,"Sync: -DMR                  |  Frame Sync   | DMRSTEREO | VC1 ");
    if ( (state->K > 0 && state->dmr_so  & 0x40 && state->payload_keyid  == 0) ||
         (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0) )
    {
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " BPK %lld", state->K);
      fprintf (stderr, "%s", KNRM);
      //state->currentslot = 0; //slot now set from cach data from buffer
    }
    fprintf (stderr, "\n");
  }

  //these ambe_fr values are set correctly now!
  //reset drop values since we don't know which slot (because I'm too lazy to write that part)
  state->dropL = 256;
  state->dropR = 256;
  processMbeFrame (opts, state, NULL, ambe_fr, NULL);
  processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
  processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
  dmrBS (opts, state); //bootstrap into full TDMA frame for BS mode
  END:
  //placing this below to fix compiler error, it will never run but compiler needs something there
  if (0 == 1)
  {
    fprintf (stderr, "this is a dumb thing to have to fix");
  }

}
