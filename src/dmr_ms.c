/*-------------------------------------------------------------------------------
 * dmr_ms.c
 * DMR MS/Simplex/Direct Mode Voice Handling and Data Gathering Routines
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "dmr_const.h"

//A subroutine for processing MS voice
void dmrMS (dsd_opts * opts, dsd_state * state)
{

  int i, j, k, l, dibit;
  int *dibit_p;
  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];
  char ambe_fr4[4][24];

  //memcpy of ambe_fr for late entry
  char m1[4][24];
  char m2[4][24];
  char m3[4][24];

  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[48];
  char EmbeddedSignalling[16];

  //cach
  char cachdata[25]; 
  int cachInterleave[24] =
  {0, 7, 8, 9, 1, 10,
   11, 12, 2, 13, 14,
   15, 3, 16, 4, 17, 18,
   19, 5, 20, 21, 22, 6, 23
  }; 

  //cach tact bits
  uint8_t tact_bits[7];

  uint8_t emb_ok = 0;
  uint8_t tact_okay = 0;
  uint8_t cach_err = 0;
  uint8_t EmbeddedSignallingOk = 0;

  uint8_t internalslot;
  uint8_t vc;

  //assign as nonsensical numbers
  uint8_t cc = 25;
  uint8_t power = 9; //power and pre-emption indicator
  uint8_t lcss = 9;

  //dummy bits to pass to dburst for link control
  uint8_t dummy_bits[196];
  memset (dummy_bits, 0, sizeof (dummy_bits));

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

  vc = 2;

  short int skipcount = 0;

  //Hardset variables for MS/Mono
  state->currentslot = 0; //0

  //Note: Manual dibit inversion required here since I didn't seperate inverted return from normal return in framesync,
  //so getDibit doesn't know to invert it before it gets here

  for (j = 0; j < 6; j++) { 
  state->dmrburstL = 16; 

  for(i = 0; i < 12; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

    cachdata[i] = dibit;
    state->dmr_stereo_payload[i] = dibit;
  }

  for (i = 0; i < 7; i++)
  {
    tact_bits[i] = cachdata[i];
  }

  tact_okay = 0;
  if ( Hamming_7_4_decode (tact_bits) ) tact_okay = 1;
  if (tact_okay != 1)
  {
    //do nothing since we aren't loop locked forever.
  }

  //internalslot = tact_bits[1];
  internalslot = 0;

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
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

    state->dmr_stereo_payload[i+12] = dibit;

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
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

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
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

    state->dmr_stereo_payload[i+66] = dibit;

    syncdata[(2*i)]   = (1 & (dibit >> 1));  // bit 1
    syncdata[(2*i)+1] = (1 & dibit);         // bit 0

    sync[i] = (dibit | 1) + 48;

    // load the superframe to do embedded signal processing
    if(vc > 1) //grab on vc1 values 2-5 B C D and E (F for Single/RC)
    {
      state->dmr_embedded_signalling[internalslot][vc-1][i*2]   = (1 & (dibit >> 1)); // bit 1
      state->dmr_embedded_signalling[internalslot][vc-1][i*2+1] = (1 & dibit); // bit 0
    }

  }

  sync[24] = 0;

  //test for RC sync pattern in MS sourced audio
  if ( strcmp (sync, DMR_RC_DATA_SYNC) == 0)
  {
    state->dmr_ms_rc = 1;
    fprintf (stderr, "%s ", getTime());
    if (opts->inverted_dmr == 0)
    {
      fprintf (stderr,"Sync: +DMR MS/DM MODE/MONO ");
    }
    else fprintf (stderr,"Sync: -DMR MS/DM MODE/MONO ");

    fprintf (stderr, "| RC ");
    fprintf (stderr, "\n");

  }

  for(i = 0; i < 8; i++) EmbeddedSignalling[i] = syncdata[i];
  for(i = 0; i < 8; i++) EmbeddedSignalling[i + 8] = syncdata[i + 40];
  
  EmbeddedSignallingOk = -1;
  if(QR_16_7_6_decode(EmbeddedSignalling))
  {
    EmbeddedSignallingOk = 1;
    cc = ((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) + EmbeddedSignalling[3]);
    power = EmbeddedSignalling[4];
    lcss = ((EmbeddedSignalling[5] << 1) + EmbeddedSignalling[6]);
  }

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

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
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;

    state->dmr_stereo_payload[i+108] = dibit;
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //'DSP' output to file
  if (opts->use_dsp_output == 1)
  {
    FILE * pFile; //file pointer
    pFile = fopen (opts->dsp_out_file, "a");
    fprintf (pFile, "\n%d 10 ", state->currentslot+1); //0x10 for "voice burst", forced to slot 1
    for (i = 6; i < 72; i++) //33 bytes, no CACH
    {
      int dsp_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
      fprintf (pFile, "%X", dsp_byte);
    }
    fclose (pFile);
  }

  state->dmr_ms_mode = 1;

  memcpy (ambe_fr4, ambe_fr2, sizeof(ambe_fr2));

  //copy ambe_fr frames first, running process mbe will correct them, 
  //but this also leads to issues extracting good le mi values when 
  //we go to do correction on them there too
  memcpy (m1, ambe_fr, sizeof(m1));
  memcpy (m2, ambe_fr2, sizeof(m2));
  memcpy (m3, ambe_fr3, sizeof(m3));

  if (state->dmr_ms_rc == 0)
  {
    if (state->directmode == 0)
    {
      processMbeFrame (opts, state, NULL, ambe_fr, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    }
    else
    {
      processMbeFrame (opts, state, NULL, ambe_fr4, NULL); //play duplicate of 2 here to smooth audio on tdma direct
      processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
      processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    }
  }
  
  if (vc == 6)
  {
    dmr_data_burst_handler(opts, state, (uint8_t *)dummy_bits, 0xEB);
    //check the single burst/reverse channel opportunity
    dmr_sbrc (opts, state, power);
    
    fprintf (stderr, "\n");
    dmr_alg_refresh (opts, state);
  }

  //collect the mi fragment
  dmr_late_entry_mi_fragment (opts, state, vc, m1, m2, m3);

  //errors in ms/mono since we skip the other slot
  // cach_err = dmr_cach (opts, state, cachdata);

  //update voice sync time for trunking purposes (particularly Con+)
  state->last_vc_sync_time = time(NULL);

  vc++;
  
  //reset emb components
  cc = 25;
  power = 9; //power and pre-emption indicator
  lcss = 9;

  //this is necessary because we need to skip and collect dibits, not just skip them
  if (vc > 6) goto END; 

  skipDibit (opts, state, 144); //skip to next tdma channel
  state->dmr_ms_rc = 0;

  //since we are in a loop, run ncursesPrinter here
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

 } // end loop

 END:
 //get first half payload dibits and store them in the payload for the next repitition
 skipDibit (opts, state, 144); //should we have two of these?

 //CACH + First Half Payload = 12 + 54
 for (i = 0; i < 66; i++) //66
 {
   dibit = getDibit(opts, state);
   if (opts->inverted_dmr == 1)
   {
     dibit = (dibit ^ 2) & 3;
   }
   state->dmr_stereo_payload[i] = dibit;

 }

 state->dmr_stereo = 0;
 state->dmr_ms_mode = 0;
 state->dmr_ms_rc = 0;
 state->directmode = 0; //flag off
  

}

//collect buffered 1st half and get 2nd half voice payload and then jump to full MS Voice decoding.
void dmrMSBootstrap (dsd_opts * opts, dsd_state * state)
{
  int i, j, k, l, dibit;
  int *dibit_p;

  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];
  char ambe_fr4[4][24]; //shim for crappy tdma direct, need more samples to see if this is really a necessary shim or not

  //memcpy of ambe_fr for late entry
  char m1[4][24];
  char m2[4][24];
  char m3[4][24];

  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[48];

  //cach
  char cachdata[25]; 
  int cachInterleave[24] =
  {0, 7, 8, 9, 1, 10,
   11, 12, 2, 13, 14,
   15, 3, 16, 4, 17, 18,
   19, 5, 20, 21, 22, 6, 23
  };

  int mutecurrentslot;
  int msMode;
  char cc[4];
  char EmbeddedSignalling[16];

  uint8_t emb_ok = 0;
  uint8_t tact_okay = 0;
  uint8_t cach_err = 0;
  uint8_t EmbeddedSignallingOk = 0;

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

  state->dmrburstL = 16; 
  state->currentslot = 0; //force to slot 0

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
      dibit = (dibit ^ 2) & 3;
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
      dibit = (dibit ^ 2) & 3;
    }
    state->dmr_stereo_payload[i+12] = dibit;
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
      dibit = (dibit ^ 2) & 3;
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
      dibit = (dibit ^ 2) & 3;
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
      dibit = (dibit ^ 2) & 3;
    }
    state->dmr_stereo_payload[i+108] = dibit;
    ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
    ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

    w++;
    x++;
    y++;
    z++;

  }

  //'DSP' output to file
  if (opts->use_dsp_output == 1)
  {
    FILE * pFile; //file pointer
    pFile = fopen (opts->dsp_out_file, "a");
    fprintf (pFile, "\n%d 10 ", state->currentslot+1); //0x10 for "voice burst", force to slot 1
    for (i = 6; i < 72; i++) //33 bytes, no CACH
    {
      int dsp_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
      fprintf (pFile, "%X", dsp_byte);
    }
    fclose (pFile);
  }

  fprintf (stderr, "%s ", getTime());
  if (opts->inverted_dmr == 0)
  {
    fprintf (stderr,"Sync: +DMR MS/DM MODE/MONO ");
  }
  else fprintf (stderr,"Sync: -DMR MS/DM MODE/MONO ");

  fprintf (stderr, "| VC* ");
  fprintf (stderr, "\n");

  //alg reset
  //dmr_alg_reset (opts, state);

  memcpy (ambe_fr4, ambe_fr2, sizeof(ambe_fr2));

  //copy ambe_fr frames first, running process mbe will correct them, 
  //but this also leads to issues extracting good le mi values when 
  //we go to do correction on them there too
  memcpy (m1, ambe_fr, sizeof(m1));
  memcpy (m2, ambe_fr2, sizeof(m2));
  memcpy (m3, ambe_fr3, sizeof(m3));

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

  //collect the mi fragment
  dmr_late_entry_mi_fragment (opts, state, 1, m1, m2, m3);

  //errors due to skipping other slot
  // cach_err = dmr_cach (opts, state, cachdata);

  skipDibit (opts, state, 144); //skip to next TDMA slot
  dmrMS (opts, state); //bootstrap into full TDMA frame

}

//simplied to a simple data collector, and then passed on to dmr_data_sync for the usual processing
void dmrMSData (dsd_opts * opts, dsd_state * state)
{
  int i, b, c;
  int dibit;
  int *dibit_p;

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
  dibit_p = state->dmr_payload_p - 90;
  for (i = 0; i < 90; i++) //90
  {
    dibit = *dibit_p;
    dibit_p++;
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;
    state->dmr_stereo_payload[i] = dibit;

  }

  for (i = 0; i < 54; i++)
  {
    dibit = getDibit(opts, state);
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;
    state->dmr_stereo_payload[i+90] = dibit;

  }

  fprintf (stderr, "%s ", getTime());
  if (opts->inverted_dmr == 0)
  {
    fprintf (stderr,"Sync: +DMR MS/DM MODE/MONO ");
  }
  else fprintf (stderr,"Sync: -DMR MS/DM MODE/MONO ");


  sprintf(state->slot1light, "%s", "");
  sprintf(state->slot2light, "%s", "");

  //process data
  state->dmr_stereo = 1;
  state->dmr_ms_mode = 1;

  dmr_data_sync (opts, state);

  state->dmr_stereo = 0;
  state->dmr_ms_mode = 0;
  state->directmode = 0; //flag off

  //should just be loaded in the dmr_payload_buffer instead now
  for (i = 0; i < 144; i++) //66
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i] = dibit;
  }
  //CACH + First Half Payload = 12 + 54
  for (i = 0; i < 66; i++) //66
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i] = dibit;
  }

}
