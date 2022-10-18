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
  char ambe_fr4[4][24] = {0};
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
  unsigned char EmbeddedSignallingInverse[16];
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
  state->currentslot = 0; //0
  internalslot = 0;
  activeslot = 0;

  //Run Loop while the getting is good
  while (loop == 1) {

  short int m = 0;

  state->dmrburstL = 16; //Use 16 for Voice

  for(i = 0; i < 12; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
    cachdata[i] = dibit;
    state->dmr_stereo_payload[i] = dibit;

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

  // for (i = 0; i < 16; i++)
  // {
  //   EmbeddedSignallingInverse[i] = EmbeddedSignalling[i] ^ 1;
  // }

  if(QR_16_7_6_decode(EmbeddedSignalling))
  {
    EmbeddedSignallingOk = 1;
  }

  //test for inverted signal - this should move to data, and have a slottype check here perhaps?
  // else if (QR_16_7_6_decode(EmbeddedSignallingInverse))
  // {
  //   if (opts->inverted_dmr == 0)
  //   {
  //     opts->inverted_dmr = 1; 
  //   }
  //   else opts->inverted_dmr = 0;
  //   goto END;
  // }

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


  }

  if ( ((strcmp (sync, DMR_MS_VOICE_SYNC) == 0) && opts->inverted_dmr == 0) ||
       ((strcmp (sync, DMR_MS_DATA_SYNC) == 0) && opts->inverted_dmr == 1) )

  {
    if (internalslot == 0)
    {
      vc1 = 1;
      state->DMRvcL = 0; //reset here if jitter/sync forces reset
      fprintf (stderr, "MS Slot 1 Voice Sync \n");
    }

    if (internalslot == 1)
    {
      vc2 = 1;
      state->DMRvcR = 0; //reset here if jitter/sync forces reset
    }

    state->dmr_ms_mode = 1; //set to 1 here??
  }

  state->dmr_ms_mode = 1; //set to 1 here?

  //don't know if we will catch an RC sync in here or not, should be on VC6 if it occurs
  if ( (strcmp (rcsync, DMR_RC_DATA_SYNC) == 0) || (strcmp (sync, DMR_RC_DATA_SYNC) == 0) ) //should be in same position as sync
  {
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");
      //fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      fprintf (stderr,"Sync: +DMR MS/DM MODE | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc1 = 1;
    }
    if (internalslot == 1)
    {
      sprintf(state->slot2light, "[slot2]");
      sprintf(state->slot1light, " slot1 ");
      //fprintf (stderr,"Sync: +DMR   slot1  [slot2] | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      fprintf (stderr,"Sync: +DMR MS/DM MODE | Color Code=%02d | DMRSTEREO | RC  ", state->color_code);
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc2 = 1;
    }
    fprintf (stderr, "\n                     "); //21 spaces to line up the sync
    //processDMRdata (opts, state);
    state->dmr_ms_rc = 1; //don't know what to do with this, if anything...
    goto SKIP;
  }

  //check for sync pattern here after collected the rest of the payload, decide what to do with it
  //if ( (strcmp (sync, DMR_MS_DATA_SYNC) == 0) )
  //fixed to compensate for inverted signal
  if ( ((strcmp (sync, DMR_MS_DATA_SYNC) == 0) && opts->inverted_dmr == 0) ||
       ((strcmp (sync, DMR_MS_VOICE_SYNC) == 0) && opts->inverted_dmr == 1) )
  {
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0)
    {
      sprintf(state->slot1light, "[slot1]");
      sprintf(state->slot2light, " slot2 ");
      //fprintf (stderr,"Sync: +DMR  [slot1]  slot2  | Color Code=%02d | DMRSTEREO | MS Data  ", state->color_code);
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  ");
      }
      else fprintf (stderr,"Sync: -DMR  ");
      //test with vc1 reset disabled, if all is well, leave disabled
      //vc1 = 1;
    }
    if (internalslot == 1)
    {
      sprintf(state->slot2light, "[slot2]");
      sprintf(state->slot1light, " slot1 ");
      //fprintf (stderr,"Sync: +DMR   slot1  [slot2] | Color Code=%02d | DMRSTEREO | Data  ", state->color_code);
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR MS/DM MODE ");
      }
      else fprintf (stderr,"Sync: -DMR MS/DM MODE ");
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

  if( ( ( ((strcmp (sync, DMR_MS_DATA_SYNC) != 0) && opts->inverted_dmr == 0) ||
          ((strcmp (sync, DMR_MS_VOICE_SYNC) != 0) && opts->inverted_dmr == 1)  ) )
            && internalslot == activeslot && vc1 < 7)

  {

    skipcount = 0; //reset skip count if processing voice frames
    fprintf (stderr,"%s ", getTime());
    if (internalslot == 0 && opts->inverted_dmr == 0)
    {
      fprintf (stderr,"Sync: +DMR MS/DM MODE | Color Code=%02d | DMRSTEREO | VC%d ", state->color_code, vc1);
      if (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x10)
      {
        fprintf (stderr, "%s", KYEL);
        fprintf(stderr, " PrK %lld", state->K);
        fprintf (stderr, "%s", KNRM);
      }
      if (state->DMRvcL == 0 && state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        fprintf(stderr, " SPT %016llX", state->K1);
        if (state->K2 != 0)
        {
          fprintf(stderr, " %016llX", state->K2);
        }
        if (state->K3 != 0)
        {
          fprintf(stderr, " %016llX", state->K3);
        }
        if (state->K4 != 0)
        {
          fprintf(stderr, " %016llX", state->K4);
        }
        fprintf (stderr, "%s", KNRM);
      }
      fprintf (stderr, "\n");
    }

    if (internalslot == 0 && opts->inverted_dmr == 1)
    {
      fprintf (stderr,"Sync: -DMR MS/DM MODE | Color Code=%02d | DMRSTEREO | VC%d ", state->color_code, vc1);
      if (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x10)
      {
        fprintf (stderr, "%s", KYEL);
        fprintf(stderr, " PrK %lld", state->K);
        fprintf (stderr, "%s", KNRM);
      }
      if (state->DMRvcL == 0 && state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        fprintf(stderr, " SPT %016llX", state->K1);
        if (state->K2 != 0)
        {
          fprintf(stderr, " %016llX", state->K2);
        }
        if (state->K3 != 0)
        {
          fprintf(stderr, " %016llX", state->K3);
        }
        if (state->K4 != 0)
        {
          fprintf(stderr, " %016llX", state->K4);
        }
        fprintf (stderr, "%s", KNRM);
      }
      fprintf (stderr, "\n");
    }


    if (internalslot == 0 && vc1 == 6) //presumably when full (and no sync issues)
    {
      //process voice burst
      ProcessVoiceBurstSync(opts, state);
      fprintf (stderr, "\n");
    }

    state->dmr_ms_mode == 1;
    memcpy (ambe_fr4, ambe_fr2, sizeof(ambe_fr2));

    //state->directmode = 0; //flag off for testing

    if (state->directmode == 0)
    {
      processMbeFrame (opts, state, NULL, ambe_fr, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    }
    else
    {
      processMbeFrame (opts, state, NULL, ambe_fr4, NULL); //play duplicate of 2 here to smooth audio
      processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    }

    if (vc1 == 6 && state->payload_algid >= 21)
    {
       state->DMRvcL = 0;
       if (state->payload_mi != 0 && state->payload_algid >= 0x21)
       {
         LFSR(state);
       }
       fprintf (stderr, "\n");
     }
  }
  vc1++;
  //6 seems optimal for MS, allows to quickly exit and resync if wonky or something
  if ( (vc1 > 6) || skipcount > 2) //escape to find new sync if deadspins without a renewed voice frame sync inside of loop
  {
    goto END;
  }

  SKIP:
  skipDibit (opts, state, 144); //skip to next tdma channel
  state->dmr_ms_rc = 0;

  //since we are in a repetitive loop, run ncursesPrinter here
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

 } // end while loop
 //reset these variables when exiting MS/DM mode.
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

 }

 state->dmr_stereo = 0;
 state->dmr_ms_mode = 0;
 state->dmr_ms_rc = 0;
 state->directmode = 0; //flag off
 state->DMRvcL = 0;

}

//collect buffered 1st half and get 2nd half voice payload and then jump to full MS Voice decoding.
void dmrMSBootstrap (dsd_opts * opts, dsd_state * state)
{
  int i, j, k, l, dibit;
  int *dibit_p;
  char ambe_fr[4][24] = {0};
  char ambe_fr2[4][24] = {0};
  char ambe_fr3[4][24] = {0};
  char ambe_fr4[4][24] = {0}; //shim for playing back direct mode samples where ambe1 is bad, duplicate 2 to 4 and play both to smooth audio
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
  state->dmrburstL = 16; //Use 16 for Voice?
  dibit_p = state->dmr_payload_p - 90;

  //CACH + First Half Payload + Sync = 12 + 54 + 24
  for (i = 0; i < 90; i++) //90
  {
    state->dmr_stereo_payload[i] = *dibit_p;
    dibit_p++;
  }

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
  //work around to set erroneous PI header values to 0 if K is active
  if (state->K > 0 || state->K1 > 0)
  {
    state->payload_keyid = 0; //just for testing
    state->payload_keyidR = 0; //just for testing
  }

  fprintf (stderr,"%s ", getTime());

  if (opts->inverted_dmr == 0)
  {
    fprintf (stderr,"Sync: +DMR MS/DM MODE |  Frame Sync   | DMRSTEREO | VC1 ");
    if ( (state->K > 0 && state->dmr_so  & 0x40 && state->payload_keyid  == 0 && state->dmr_fid == 0x10) ||
         (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fid == 0x10)  )
    {
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " PrK %lld", state->K);
      fprintf (stderr, "%s", KNRM);
    }
    if ( (state->DMRvcL == 0 && state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68) ||
         (state->DMRvcR == 0 && state->K1 > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fid == 0x68)  )
    {
      fprintf (stderr, "\n");
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " SPT %016llX", state->K1);
      if (state->K2 != 0)
      {
        fprintf(stderr, " %016llX", state->K2);
      }
      if (state->K3 != 0)
      {
        fprintf(stderr, " %016llX", state->K3);
      }
      if (state->K4 != 0)
      {
        fprintf(stderr, " %016llX", state->K4);
      }
    }
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
  }
  else
  {
    fprintf (stderr,"Sync: -DMR MS/DM MODE |  Frame Sync   | DMRSTEREO | VC1 ");
    if ( (state->K > 0 && state->dmr_so  & 0x40 && state->payload_keyid  == 0 && state->dmr_fid == 0x10) ||
         (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fid == 0x10) )
    {
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " PrK %lld", state->K);
      fprintf (stderr, "%s", KNRM);
    }
    if ( (state->DMRvcL == 0 && state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68) ||
         (state->DMRvcR == 0 && state->K1 > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fid == 0x68)  )
    {
      fprintf (stderr, "\n");
      fprintf (stderr, "%s", KYEL);
      fprintf(stderr, " SPT %016llX", state->K1);
      if (state->K2 != 0)
      {
        fprintf(stderr, " %016llX", state->K2);
      }
      if (state->K3 != 0)
      {
        fprintf(stderr, " %016llX", state->K3);
      }
      if (state->K4 != 0)
      {
        fprintf(stderr, " %016llX", state->K4);
      }
    }
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
  }

  state->dmr_ms_mode = 1;
  state->DMRvcL = 0;

  memcpy (ambe_fr4, ambe_fr2, sizeof(ambe_fr2));

  //state->directmode = 0; //flag off for testing samples

  if (state->directmode == 0)
  {
    processMbeFrame (opts, state, NULL, ambe_fr, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
  }
  else
  {
    processMbeFrame (opts, state, NULL, ambe_fr4, NULL); //play duplicate of 2 here to smooth audio
    processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
    processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
  }
  

  skipDibit (opts, state, 144); //skip to next TDMA slot
  dmrMS (opts, state); //bootstrap into full TDMA frame

}

void dmrMSData (dsd_opts * opts, dsd_state * state)
{
  int i, b, c;
  int dibit;
  int *dibit_p;
  unsigned int burst;

  unsigned char MSCach[24] = {0}; //12 dibits = 24 bits
  unsigned char MSSync[48] = {0}; //24 dibits = 48 bits
  unsigned char MSSlot[20] = {0}; //10 dibits = 20 bits
  unsigned char MSBurst[5] = {0};
  int cachInterleave[24]   = {0, 7, 8, 9, 1, 10, 11, 12, 2, 13, 14, 15, 3, 16, 4, 17, 18, 19, 5, 20, 21, 22, 6, 23};

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
  b = 0;
  c = 0;

  //CACH + First Half Payload + Sync = 12 + 54 + 24
  dibit_p = state->dmr_payload_p - 90;
  for (i = 0; i < 90; i++) //90
  {
    dibit = *dibit_p;
    dibit_p++;
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2); 
    }
    state->dmr_stereo_payload[i] = dibit;
    //load cach
    if (i < 12)
    {
      MSCach[cachInterleave[(i*2)]]   = (1 & (dibit >> 1)); // bit 1
      MSCach[cachInterleave[(i*2)+1]] = (1 & dibit);       // bit 0
    }
    //load slot, first half
    if (i > 60 && i < 66)
    {
      MSSlot[(b*2)]   = (1 & (dibit >> 1)); // bit 1
      MSSlot[(b*2)+1] = (1 & dibit);       // bit
      b++;
    }
    //load sync
    if (i > 66)
    {
      MSSync[(c*2)]   = (1 & (dibit >> 1)); // bit 1
      MSSync[(c*2)+1] = (1 & dibit);       // bit 0
      c++;
    }
  }

  for (i = 0; i < 54; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2); 
    }
    state->dmr_stereo_payload[i+90] = dibit;
    //load slot, second half
    if (i < 6)
    {
      MSSlot[(b*2)]   = (1 & (dibit >> 1)); // bit 1
      MSSlot[(b*2)+1] = (1 & dibit);       // bit
      b++;
    }

  }

  //state->directmode = 0; //flag off for testing

  //with FEC fix, now we can (hopefully) feel confident in running MS data
  if (1 == 1) //opts->payload == 1 && state->directmode == 0
  {
    fprintf (stderr, "%s ", getTime());
    if (opts->inverted_dmr == 0)
    {
      fprintf (stderr,"Sync: +DMR MS/DM MODE ");
    }
    else fprintf (stderr,"Sync: -DMR MS/DM MODE ");
  }

  // decode and correct cach and slot
  // golay (20,8) hamming-weight of 6 reliably corrects at most 2 bit-errors
  int cach_okay = -1;
  if ( Hamming_7_4_decode (MSCach) )
  {
    cach_okay = 1;
  }
  if (cach_okay == 1)
  {
    //fprintf (stderr, "CACH Okay ");
  }
  else
  {
    //fprintf (stderr, "CACH ERR ");
    if (opts->aggressive_framesync == 1) //may not worry about it on data
    {
      goto END;
    }
  }

  int slot_okay = -1;
  if(Golay_20_8_decode(MSSlot))
  {
    slot_okay = 1;
  }
  if (slot_okay == 1)
  {
    //fprintf (stderr, "Slot Okay ");
  }
  else goto END;


  sprintf(state->slot1light, "%s", "");
  sprintf(state->slot2light, "%s", "");

  //process data
  state->dmr_stereo = 1;
  state->dmr_ms_mode = 1;

  //with FEC fix, now we can (hopefully) feel confident in running MS data
  if (1 == 1) //opts->payload == 1 && state->directmode == 0
  {
    processDMRdata (opts, state);
  }

  END:
  if (slot_okay != 1 || cach_okay != 1)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "| ** CACH or Burst Type FEC ERR ** ");
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
  }

  state->dmr_stereo = 0;
  state->dmr_ms_mode = 0;
  state->directmode = 0; //flag off

  //get potential first half payload dibits and store them in the payload for the next repitition, MS voice or data.
  skipDibit (opts, state, 144);

  //CACH + First Half Payload = 12 + 54
  for (i = 0; i < 66; i++) //66
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i] = dibit;    
  }

}
