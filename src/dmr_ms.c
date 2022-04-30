#include "dsd.h"
#include "dmr_const.h"

//A subroutine for processing MS voice
void dmrMS (dsd_opts * opts, dsd_state * state)
{
  //fprintf (stderr, "\n Quick DMR Voice Testing\n");
  int i, j, k, l, dibit;
  int *dibit_p;
  char ambe_fr[4][24] = {0};
  char ambe_fr2[4][24] = {0};
  char ambe_fr3[4][24] = {0};
  char t_ambe_fr[4][24] = {9};
  char t_ambe_fr2[4][24] = {9};
  char t_ambe_fr3[4][24] = {9};
  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[48];
  //standalone inbound rc sync exists on dibits 48-96?
  //or 44-92? of each potential payload, middle 10 ms or 30 ms frame
  char rcsync[25];
  char rcsyncdata[96]; //96 bit RC sync data
  char cachdata[13] = {0};
  int mutecurrentslot;
  int msMode;
  char cc[4];
  unsigned char EmbeddedSignalling[16];
  int EmbeddedSignallingOk;
  unsigned int internalcolorcode;
  int internalslot;
  int activeslot;
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

  //Init the color code status
  state->color_code_ok = 0;

  vc1 = 2;
  vc2 = 2;

  short int loop = 1;
  short int skipcount = 0;

  //Hardset variables for MS
  state->currentslot = 0;
  internalslot = 0;
  activeslot = 0;

  //Run Loop while the getting is good
  while (loop == 1) {

  // No CACH in MS Mode?
  for(i = 0; i < 12; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    cachdata[i] = dibit;
    state->dmr_stereo_payload[i] = dibit;
    if(i == 2)
    {
      //state->currentslot = (1 & (dibit >> 1));
      //internalslot = (1 & (dibit >> 1));
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
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
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
    //goto END;
  }
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
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+48] = dibit;
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  // signaling data or sync
  for(i = 0; i < 24; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+66] = dibit;
    syncdata[(2*i)]   = (1 & (dibit >> 1));  // bit 1
    syncdata[(2*i)+1] = (1 & dibit);         // bit 0
    sync[i] = (dibit | 1) + 48;

    if (i < 4 || i > 19) //
    {
      EmbeddedSignalling[2*i]   = (1 & (dibit >> 1));  // bit 1
      EmbeddedSignalling[2*i+1] = (1 & dibit);         // bit 0
    }

    // load the superframe to do the voiceburstsync processing
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
  if(EmbeddedSignallingOk == 1)
  {
    state->color_code = (unsigned int)((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) + EmbeddedSignalling[3]);
    internalcolorcode = (unsigned int)((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) + EmbeddedSignalling[3]);
    state->color_code_ok = EmbeddedSignallingOk;
    //Power Indicator, not the other PI (header)
    state->PI = (unsigned int)EmbeddedSignalling[4];
    state->PI_ok = EmbeddedSignallingOk;
    //Link Control Start Stop Indicator
    state->LCSS = (unsigned int)((EmbeddedSignalling[5] << 1) + EmbeddedSignalling[6]);
    state->LCSS_ok = EmbeddedSignallingOk;

  }
  //else skipcount++;

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+90] = dibit;
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

    //WIP for RC inbound sync data, part 2, second 24 bits (12 dibits)
    if (i <= 12)
    {
      rcsyncdata[i+12] = (dibit | 1) + 48; //test and double check positions for this sync data
    }

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
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+108] = dibit;
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

    //end early to allow next sync without skipping a voice superframe
    //only use if only doing 6 VC and nothing else, otherwise, will lose sync
    if (vc1 == 6 && i == 30)
    {
      //i = 36;
    }

  }

  if ( (strcmp (sync, DMR_MS_VOICE_SYNC) == 0) ) //
  {
    if (internalslot == 0)
    {
      vc1 = 1;
      fprintf (stderr, "MS Slot 1 Voice Sync \n");
      //activeslot = 0;

    }
    if (internalslot == 1)
    {
      vc2 = 1;
      //fprintf (stderr, "MS Slot 2 Voice Sync \n");
      //activeslot = 1;
    }
    state->dmr_ms_mode = 1; //set to 1 here??
  }

  state->dmr_ms_mode = 1; //set to 1 here?
  //testing to see if we need to do something to get MS mode working properly,
  //get proper sync, skip frames, etc.
  if (state->dmr_ms_mode == 1 ) //&& vc1 < 7
  {
    //do something
    fprintf (stderr, "MS MODE ");
  }

  //don't know if we will catch an RC sync in here or not, should be on VC6 if it occurs
  if ( (strcmp (rcsync, DMR_RC_DATA_SYNC) == 0) || (strcmp (sync, DMR_RC_DATA_SYNC) == 0) ) //should be in same position as sync
  {
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");
      fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc1 = 1;
    }
    if (internalslot == 1)
    {
      sprintf(state->slot2light, "[slot2]");
      sprintf(state->slot1light, " slot1 ");
      fprintf (stderr,"Sync: +DMR   slot1  [slot2] | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc2 = 1;
    }
    fprintf (stderr, "\n                     "); //21 spaces to line up the sync
    //processDMRdata (opts, state);
    state->dmr_ms_rc = 1; //don't know what to do with this, if anything...
    goto SKIP;
  }

  //check for sync pattern here after collected the rest of the payload, decide what to do with it
  if ( (strcmp (sync, DMR_MS_DATA_SYNC) == 0) )
  {
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");
      //fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | MS Data  ", state->color_code);
      fprintf (stderr,"Sync: +DMR  ");
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc1 = 1;
    }
    if (internalslot == 1)
    {
      sprintf(state->slot2light, "[slot2]");
      sprintf(state->slot1light, " slot1 ");
      //fprintf (stderr,"Sync: +DMR   slot1  [slot2] | Color Code=%02d | DMRSTEREO | Data  ", state->color_code);
      fprintf (stderr,"Sync: +DMR  ");
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc2 = 1;
    }
    //fprintf (stderr, "\n                     "); //21 spaces to line up the sync
    //fprintf (stderr, "                     "); //21 spaces to line up the sync
    state->dmr_ms_mode = 1; //set to 1 here??
    processDMRdata (opts, state);
    skipcount++;
    //goto SKIP;
    goto END;
  }

  if( (strcmp (sync, DMR_MS_DATA_SYNC) != 0) && internalslot == activeslot && vc1 < 7) //only play voice no MS Data Sync and vc1 below 6 (no voice resync)
  {

    skipcount = 0; //reset skip count if processing voice frames
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | VC%d \n", state->color_code, vc1);
    }

    if (internalslot == 1)
    {
      fprintf (stderr,"Sync: +DMR   slot1  [slot2] | Color Code=%02d | DMRSTEREO | VC%d \n", state->color_code, vc2);
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
      //ProcessVoiceBurstSync(opts, state);
      //fprintf (stderr, "\n");
    }

    processMbeFrame (opts, state, NULL, ambe_fr, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    if (internalslot == 0)
    {
      //vc1++;
    }
    if (internalslot == 1)
    {
      vc2++;
    }
  }
  vc1++;
  //6 seems optimal for MS, allows to quickly exit and resync if wonky or something
  if ( (vc1 > 6) || skipcount > 2) //escape to find new sync if deadspins without a renewed voice frame sync inside of loop
  {
    goto END;
  }

  SKIP:
  skipDibit (opts, state, 144); //skip to next slot?
  state->dmr_ms_rc = 0;

  //since we are in a repetitive loop, run ncursesPrinter here
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

 } // end while loop
 //reset these variables when exiting MS mode.
 END:
 //get first half payload dibits and store them in the payload for the next repitition
 skipDibit (opts, state, 144);
 //CACH + First Half Payload = 12 + 54
 for (i = 0; i < 66; i++) //66
 {
   dibit = getDibit(opts, state);
   if (opts->inverted_dmr == 1)
   {
     dibit = (dibit ^ 2);
   }
   state->dmr_stereo_payload[i] = dibit;
   //fprintf(stderr, "%X", state->dmr_stereo_payload[i]);
 }

 state->dmr_stereo = 0;
 state->dmr_ms_mode = 0;
 state->dmr_ms_rc = 0;

}

//Only process 2nd half voice payload (3rd frame) and then jump to full MS Voice decoding.
void dmrMSBootstrap (dsd_opts * opts, dsd_state * state)
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

  //add time to mirror sync
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

  dibit_p = state->dmr_payload_p - 90;
  //payload buffer tests
  //CACH + First Half Payload + Sync = 12 + 54 + 24
  //fprintf(stderr, "\n Full DMR Stereo Dump Dibits 90 From Buffer\n");
  for (i = 0; i < 90; i++) //90
  {
    state->dmr_stereo_payload[i] = *dibit_p;
    dibit_p++;
    //fprintf(stderr, "%X", state->dmr_stereo_payload[i]);
  }
  //end payload buffer test

  for(i = 0; i < 12; i++)
  {
    dibit = state->dmr_stereo_payload[i];
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    cachdata[i] = dibit;
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
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    //state->dmr_stereo_payload[i+12] = dibit;
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
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
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
      dibit = (dibit ^ 2);
    }
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  fprintf (stderr, "MS MODE ");
  fprintf (stderr,"%s ", getTime());
  fprintf (stderr,"Sync: +DMR                  | Color Code=XX | DMRSTEREO | VC1 FS \n");
  processMbeFrame (opts, state, NULL, ambe_fr, NULL);
  processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
  processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
  skipDibit (opts, state, 144); //skip to next TDMA slot
  dmrMS (opts, state); //bootstrap into full TDMA frame

}

void dmrMSData (dsd_opts * opts, dsd_state * state)
{
  int i;
  int dibit;
  int *dibit_p;
  unsigned int burst;

  //add time to mirror sync
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

  //CACH + First Half Payload + Sync = 12 + 54 + 24
  //fprintf(stderr, "\n Full DMR Stereo Dump Dibits 90 From Buffer\n");
  for (i = 0; i < 90; i++) //90
  {
    //fprintf(stderr, "%X", state->dmr_stereo_payload[i]);
  }

  for (i = 0; i < 54; i++)
  {
    dibit = getDibit(opts, state);
    if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i+90] = dibit;

  }
  //fprintf(stderr, "\n");
  //print nice pretty lines
  fprintf (stderr, "MS MODE ");
  fprintf (stderr, "%s ", getTime());
  //fprintf (stderr, "Sync: +MS DATA              | Color Code=XX | DMRSTEREO | Data  ");
  fprintf (stderr, "Sync: +DMR  ");
  //fprintf (stderr, "\n                             ");

  //sprintf for slot 1, doesn't matter, just makes print out of data look uniform setting ahead of time
  sprintf(state->slot1light, "[slot1]");
  sprintf(state->slot2light, " slot2 ");

  //process data
  state->dmr_stereo = 1;
  state->dmr_ms_mode = 1;
  processDMRdata (opts, state);
  state->dmr_stereo = 0;
  state->dmr_ms_mode = 0;

  //get potential first half payload dibits and store them in the payload for the next repitition, MS voice or data.
  skipDibit (opts, state, 144);
  //CACH + First Half Payload = 12 + 54
  for (i = 0; i < 66; i++) //66
  {
    dibit = getDibit(opts, state);
    if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    state->dmr_stereo_payload[i] = dibit;
    //fprintf(stderr, "%X", state->dmr_stereo_payload[i]);
  }

}
