/*-------------------------------------------------------------------------------
 * dmr_bs.c
 * DMR BS Voice Handling and Data Gathering Routines - "DMR STEREO"
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "dmr_const.h"

//A subroutine for processing each TDMA frame individually to allow for
//processing voice and/or data on both BS slots (channels) simultaneously
void dmrBS (dsd_opts * opts, dsd_state * state)
{
  char * timestr = NULL;
  
  int i, dibit;
  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];

  //redundancy check (carrier signal loss event)
  char redundancyA[36];
  char redundancyB[36];

  //memcpy of ambe_fr for late entry
  uint8_t m1[4][24];
  uint8_t m2[4][24];
  uint8_t m3[4][24];
  
  const int *w, *x, *y, *z;
  char sync[25];
  uint8_t syncdata[48];
  memset (syncdata, 0, sizeof(syncdata));

  uint8_t emb_pdu[16];
  memset (emb_pdu, 0, sizeof(emb_pdu));

  uint8_t emb_ok = 0;
  uint8_t tact_okay = 0;
  uint8_t cach_err = 0;
  UNUSED(cach_err);

  uint8_t internalslot;
  uint8_t vc1;
  uint8_t vc2;

  //assign as nonsensical numbers
  uint8_t cc = 25;
  uint8_t power = 9; //power and pre-emption indicator
  uint8_t lcss = 9;
  UNUSED2(cc, lcss);
  
  //would be ideal to grab all dibits and break them into bits to pass to new data handler?
  uint8_t dummy_bits[196]; 
  memset (dummy_bits, 0, sizeof(dummy_bits));
  
  //Init slot lights
  sprintf (state->slot1light, " slot1 ");
  sprintf (state->slot2light, " slot2 ");

  //Init the color code status
  state->color_code_ok = 0;

  //if coming from the bootsrap, then the slot will still be assigned the last value
  //we want to set only that vc value to 2, the other to 7
  if (state->currentslot == 0)
  {
    vc1 = 2;
    vc2 = 7;
  }
  if (state->currentslot == 1)
  {
    vc1 = 7;
    vc2 = 2;
  }

  short int loop = 1;
  short int skipcount = 0;

  //cach
  uint8_t cachdata[25];
  int cachInterleave[24] =
  {0, 7, 8, 9, 1, 10,
   11, 12, 2, 13, 14,
   15, 3, 16, 4, 17, 18,
   19, 5, 20, 21, 22, 6, 23
  }; 

  //cach tact bits
  uint8_t tact_bits[7];

  //Run Loop while the getting is good
  while (loop == 1) {

  if (exitflag == 1)
  {
    cleanupAndExit (opts, state);
  }

  timestr = getTimeC();

  memset (ambe_fr, 0, sizeof(ambe_fr));
  memset (ambe_fr2, 0, sizeof(ambe_fr2));
  memset (ambe_fr3, 0, sizeof(ambe_fr3));
  memset (emb_pdu, 0, sizeof(emb_pdu));
  memset (syncdata, 0, sizeof(syncdata));

  internalslot = -1; //reset here so we know if this value is being set properly
  for(i = 0; i < 12; i++)
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i] = dibit;

    cachdata[cachInterleave[(i*2)]]   = (1 & (dibit >> 1)); // bit 1
    cachdata[cachInterleave[(i*2)+1]] = (1 & dibit);       // bit 0
  }

  for (i = 0; i < 7; i++)
  {
    tact_bits[i] = cachdata[i];
  }

  //disabled for dmr_cach testing
  tact_okay = 0;
  if ( Hamming_7_4_decode (tact_bits) ) tact_okay = 1;
  if (tact_okay != 1) goto END;


  internalslot = state->currentslot = tact_bits[1];

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

  //end redundancy test, set B to A
  memcpy (redundancyB, redundancyA, sizeof (redundancyA));

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

  //signaling data or sync
  for(i = 0; i < 24; i++)
  {
    dibit = getDibit(opts, state);
    state->dmr_stereo_payload[i+66] = dibit;
    sync[i] = (dibit | 1) + 48; //Sync Burst String as ones and threes

    syncdata[(2*i)]   = (1 & (dibit >> 1));  // bit 1
    syncdata[(2*i)+1] = (1 & dibit);         // bit 0

    //embedded link control
    if(internalslot == 0 && vc1 > 1 && vc1 < 7) //grab on vc1 values 2-5 B C D E, and F
    {
      state->dmr_embedded_signalling[internalslot][vc1-1][i*2]   = (1 & (dibit >> 1)); // bit 1
      state->dmr_embedded_signalling[internalslot][vc1-1][i*2+1] = (1 & dibit); // bit 0
    }

    if(internalslot == 1 && vc2 > 1 && vc2 < 7) //grab on vc2 values 2-5 B C D E, and F
    {
      state->dmr_embedded_signalling[internalslot][vc2-1][i*2]   = (1 & (dibit >> 1)); // bit 1
      state->dmr_embedded_signalling[internalslot][vc2-1][i*2+1] = (1 & dibit); // bit 0
    }

  }
  sync[24] = 0;

  for(i = 0; i < 8; i++) emb_pdu[i + 0] = syncdata[i];
  for(i = 0; i < 8; i++) emb_pdu[i + 8] = syncdata[i + 40];

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
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
    if (internalslot == 0) vc1 = 1;
    if (internalslot == 1) vc2 = 1;
  }

  //check for sync pattern here after collected the rest of the payload, decide what to do with it
  if ( strcmp (sync, DMR_BS_DATA_SYNC) == 0 )
  {
    
    fprintf (stderr,"%s ", timestr);
    if (internalslot == 0)
    {
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  ");
      }
      else fprintf (stderr,"Sync: -DMR  ");
      
      vc1 = 7; //set to 7 so we can see that we should not be on a VC unless a framesync comes in for it first

      //close MBEout file - slot 1
      if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state); 
    }
    if (internalslot == 1)
    {
      if (opts->inverted_dmr == 0)
      {
        fprintf (stderr,"Sync: +DMR  ");
      }
      else fprintf (stderr,"Sync: -DMR  ");

      vc2 = 7; //set to 7 so we can see that we should not be on a VC unless a framesync comes in for it first

      //close MBEout file - slot 2
      if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

    }
    dmr_data_sync (opts, state);
    skipcount++;
    goto SKIP;
  }

  //ETSI TS 102 361-1 V2.6.1 Table E.2 and E.4
  //check for a rc burst with bptc or 34 rate data in it (testing only)
  // #define RC_TESTING //disable if not in use
  #ifdef RC_TESTING
  if ( (strcmp (sync, DMR_BS_DATA_SYNC) != 0) && (strcmp (sync, DMR_BS_VOICE_SYNC) != 0) && 
       ( (internalslot == 0 && vc1 == 6) ||  (internalslot == 1 && vc2 == 6) )              )
  {

    //if the QR FEC is good, and the P/PI bit is on for RC
    if (QR_16_7_6_decode(emb_pdu) && emb_pdu[4])
    {
      fprintf (stderr,"%s ", timestr);

      if (opts->inverted_dmr == 0)
        fprintf (stderr,"Sync: +RC   ");
      else fprintf (stderr,"Sync: -RC   ");

      dmr_data_sync (opts, state);

      dmr_data_burst_handler(opts, state, (uint8_t *)dummy_bits, 0xEB);

      dmr_sbrc (opts, state, emb_pdu[4]);

      if (internalslot == 0)
        vc1 = 7;
      else vc2 = 7;

      goto SKIP;

    }

  }
  #endif //RC_TESTING

  //check to see if we are expecting a VC at this point vc > 7
  if (strcmp (sync, DMR_BS_DATA_SYNC) != 0 && internalslot == 0 && vc1 > 6)
  {
    fprintf (stderr,"%s ", timestr);

    //simplifying things
    // char polarity[3];
    char light[18];

    sprintf (light, "%s", " [SLOT1]  slot2  ");
    fprintf (stderr,"Sync:  DMR %s", light);
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "| Frame Sync Err: %d", vc1);
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
    vc1++;
    //this should give it enough time to find the next frame sync pattern, if it exists, if not, then trigger a resync
    if ( vc1 > 13 ) goto END;
    else goto SKIP;
  }

  //check to see if we are expecting a VC at this point vc > 7
  if (strcmp (sync, DMR_BS_DATA_SYNC) != 0 && internalslot == 1 && vc2 > 6)
  {
    fprintf (stderr,"%s ", timestr);

    //simplifying things
    // char polarity[3];
    char light[18];

    sprintf (light, "%s", "  slot1  [SLOT2] ");
    fprintf (stderr,"Sync:  DMR %s", light);
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "| Frame Sync Err: %d", vc2);
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
    vc2++;
    //this should give it enough time to find the next frame sync pattern, if it exists, if not, then trigger a resync
    if ( vc2 > 13 ) goto END;
    else goto SKIP;
  }

  //only play voice on no data sync, and VC values are within expected values 1-6
  if (strcmp (sync, DMR_BS_DATA_SYNC) != 0) //we already have a tact ecc check, so we won't get here without that, see if there is any other eccs we can run just to make sure 
  {

    //check the embedded signalling, if bad at this point, we probably aren't quite in sync 
    if(QR_16_7_6_decode(emb_pdu)) emb_ok = 1;
    else emb_ok = 0;

    //disable the goto END; if this causes more problems than fixing on late entry dual voices i.e., lots of forced resyncs
    if ( (strcmp (sync, DMR_BS_VOICE_SYNC) != 0) && emb_ok == 0) goto END; //fprintf (stderr, "EMB BAD? ");
    else if (emb_ok == 1)
    {
      cc = ((emb_pdu[0] << 3) + (emb_pdu[1] << 2) + (emb_pdu[2] << 1) + emb_pdu[3]);
      power = emb_pdu[4];
      lcss = ((emb_pdu[5] << 1) + emb_pdu[6]);
      state->dmr_color_code = state->color_code = cc;
    }


    skipcount = 0; //reset skip count if processing voice frames
    fprintf (stderr,"%s ", timestr);

    //simplifying things
    char polarity[3];
    char light[18];
    uint8_t vc;
    if (internalslot == 0)
    {
      state->dmrburstL = 16;
      vc = vc1;
      sprintf (light, "%s", " [SLOT1]  slot2  ");
      //open MBEout file - slot 1
      if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
    } 
    else
    {
      state->dmrburstR = 16;
      vc = vc2;
      sprintf (light, "%s", "  slot1  [SLOT2] ");
      //open MBEout file - slot 2
      if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_fR == NULL)) openMbeOutFileR (opts, state);
    } 
    if (opts->inverted_dmr == 0) sprintf (polarity, "%s", "+");
    else sprintf (polarity, "%s", "-");
    if (state->dmr_color_code != 16)
      fprintf (stderr,"Sync: %sDMR %s| Color Code=%02d | VC%d ", polarity, light, state->dmr_color_code, vc);
    else fprintf (stderr,"Sync: %sDMR %s| Color Code=XX | VC%d ", polarity, light, vc);
 
    if (internalslot == 0 && vc1 == 6) 
    {
      //process embedded link control
      fprintf (stderr, "\n");
      dmr_data_burst_handler(opts, state, (uint8_t *)dummy_bits, 0xEB);
      //check the single burst/reverse channel opportunity -- moved
      // dmr_sbrc (opts, state, power);

    }

    if (internalslot == 1 && vc2 == 6) 
    {
      //process embedded link control
      fprintf (stderr, "\n");
      dmr_data_burst_handler(opts, state, (uint8_t *)dummy_bits, 0xEB);
      //check the single burst/reverse channel opportunity -- moved
      // dmr_sbrc (opts, state, power);

    }

    if (opts->payload == 1) fprintf (stderr, "\n"); //extra line break necessary here

    //copy ambe_fr frames first, running process mbe will correct them, 
    //but this also leads to issues extracting good le mi values when 
    //we go to do correction on them there too
    memcpy (m1, ambe_fr, sizeof(m1));
    memcpy (m2, ambe_fr2, sizeof(m2));
    memcpy (m3, ambe_fr3, sizeof(m3));

    processMbeFrame (opts, state, NULL, ambe_fr, NULL);
    if(internalslot == 0)
    {
      memcpy(state->f_l4[0], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
      memcpy(state->s_l4[0], state->s_l, sizeof(state->s_l));
      memcpy(state->s_l4u[0], state->s_lu, sizeof(state->s_lu));
    }
      
    else
    {
      memcpy(state->f_r4[0], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
      memcpy(state->s_r4[0], state->s_r, sizeof(state->s_r));
      memcpy(state->s_r4u[0], state->s_ru, sizeof(state->s_ru));
    }
      
    
    processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
    if(internalslot == 0)
    {
      memcpy(state->f_l4[1], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
      memcpy(state->s_l4[1], state->s_l, sizeof(state->s_l));
      memcpy(state->s_l4u[1], state->s_lu, sizeof(state->s_lu));
    }
      
    else
    {
      memcpy(state->f_r4[1], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
      memcpy(state->s_r4[1], state->s_r, sizeof(state->s_r));
      memcpy(state->s_r4u[1], state->s_ru, sizeof(state->s_ru));
    }

    processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
    if(internalslot == 0)
    {
      memcpy(state->f_l4[2], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
      memcpy(state->s_l4[2], state->s_l, sizeof(state->s_l));
      memcpy(state->s_l4u[2], state->s_lu, sizeof(state->s_lu));
    }
      
    else
    {
      memcpy(state->f_r4[2], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
      memcpy(state->s_r4[2], state->s_r, sizeof(state->s_r));
      memcpy(state->s_r4u[2], state->s_ru, sizeof(state->s_ru));
    }


    //'DSP' output to file -- run before sbrc
    if (opts->use_dsp_output == 1)
    {
      FILE * pFile; //file pointer
      pFile = fopen (opts->dsp_out_file, "a");
      fprintf (pFile, "\n%d 98 ", internalslot+1); //'98' is CACH designation value
      for (i = 0; i < 6; i++) //3 byte CACH
      {
        int cach_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
        fprintf (pFile, "%X", cach_byte);
      }
      fprintf (pFile, "\n%d 10 ", internalslot+1); //0x10 for voice burst
      for (i = 6; i < 72; i++) //33 bytes, no CACH
      {
        int dsp_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
        fprintf (pFile, "%X", dsp_byte);
      }
      fclose (pFile);
    }

    //run sbrc here to look for the late entry key and alg after we observe potential errors in VC6
    if (internalslot == 0 && vc1 == 6) dmr_sbrc (opts, state, power);
    if (internalslot == 1 && vc2 == 6) dmr_sbrc (opts, state, power);

    cach_err = dmr_cach (opts, state, cachdata); 
    if (opts->payload == 0) fprintf (stderr, "\n");

    // run alg refresh after vc6 ambe processing
    if (internalslot == 0 && vc1 == 6) dmr_alg_refresh (opts, state);
    if (internalslot == 1 && vc2 == 6) dmr_alg_refresh (opts, state);

    dmr_late_entry_mi_fragment (opts, state, vc%7, m1, m2, m3);

    //increment the vc counters
    if (internalslot == 0) vc1++;
    if (internalslot == 1) vc2++;

    //update cc amd vc sync time for trunking purposes (particularly Con+)
    if (opts->p25_is_tuned == 1)
    {
      state->last_vc_sync_time = time(NULL);
      state->last_cc_sync_time = time(NULL);
    }

    //reset err checks
    cach_err = 1;
    tact_okay = 0;
    emb_ok = 0;
    
    //reset emb components
    cc = 25;
    power = 9;
    lcss = 9;

    //Extra safeguards to break loop
    // if ( (vc1 > 7 && vc2 > 7) ) goto END;
    if ( (vc1 > 14 || vc2 > 14) ) goto END;

  }

  SKIP:

  //both working now, will need support added for ENC audio and no key
  //NOTE: We want this to play regardless of whether the slot is voice or data, to play silence in one slot and voice in the second, or voices in both
  if (internalslot == 1 && opts->floating_point == 1 && opts->pulse_digi_rate_out == 8000)
    playSynthesizedVoiceFS3 (opts, state); //Float Stereo Mix 3v2
  if (internalslot == 1 && opts->floating_point == 0 && opts->pulse_digi_rate_out == 8000)
    playSynthesizedVoiceSS3 (opts, state); //Short Stereo Mix 3v2

  if (skipcount > 3) //after 3 onsecutive data frames, drop back to getFrameSync and process with dmr_data_sync (need one more in order to push last voice on slot 2 only voice)
  {
    //set tests to all good so we don't get a bogus/redundant voice error 
    cach_err = 0;
    tact_okay = 1;
    emb_ok = 1;
    goto END;
  }

  //since we are in a while loop, run ncursesPrinter here.
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

  //
  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }

 } // while loop

 END:
 state->dmr_stereo = 0;
 state->errs = 0;
 state->errs2 = 0;
 state->errs2R = 0;
 state->errs2 = 0;

 //close any open MBEout files
 if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
 if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

 //clear out any stale audio storage buffers
 memset (state->f_l4, 0.0f, sizeof(state->f_l4));
 memset (state->f_r4, 0.0f, sizeof(state->f_r4));
 memset (state->s_l4, 0, sizeof(state->s_l4));
 memset (state->s_r4, 0, sizeof(state->s_r4));

 //if we have a tact or emb err, then produce sync pattern/err message
 if (tact_okay != 1 || emb_ok != 1)
 {

  fprintf (stderr,"%s ", timestr);
  fprintf (stderr,"Sync:  DMR                  ");
  fprintf (stderr, "%s", KRED);
  fprintf (stderr, "| VOICE CACH/EMB ERR");
  fprintf (stderr, "%s", KNRM);
  fprintf (stderr, "\n");
  //run refresh if either slot had an active MI in it.
  if (state->payload_algid >= 0x21)
  {
    state->currentslot = 0;
    dmr_alg_refresh (opts, state);
  }
  if (state->payload_algidR >= 0x21) 
  {
    state->currentslot = 1;
    dmr_alg_refresh (opts, state);
  }
    
  //failsafe to reset all data header and blocks when bad tact or emb
  dmr_reset_blocks (opts, state); 
   
 }

 //
 if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }

}

//Process buffered half frame and 2nd half and then jump to full BS decoding
void dmrBSBootstrap (dsd_opts * opts, dsd_state * state)
{
  int i, dibit;
  int *dibit_p;
  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];

  memset (ambe_fr, 0, sizeof(ambe_fr));
  memset (ambe_fr2, 0, sizeof(ambe_fr2));
  memset (ambe_fr3, 0, sizeof(ambe_fr3));

  //memcpy of ambe_fr for late entry
  uint8_t m1[4][24];
  uint8_t m2[4][24];
  uint8_t m3[4][24];

  const int *w, *x, *y, *z;
  char sync[25];
  uint8_t tact_okay = 0;
  uint8_t cach_err = 0;
  uint8_t sync_okay = 1;
  UNUSED(cach_err);

  uint8_t internalslot;

  uint8_t cachdata[25];
  int cachInterleave[24] =
  {0, 7, 8, 9, 1, 10,
  11, 12, 2, 13, 14,
  15, 3, 16, 4, 17, 18,
  19, 5, 20, 21, 22, 6, 23
  };

  char * timestr = getTimeC();

  //payload buffer
  //CACH + First Half Payload + Sync = 12 + 54 + 24
  dibit_p = state->dmr_payload_p - 90;
  for (i = 0; i < 90; i++) //90
  {
    dibit = *dibit_p;
    dibit_p++;
    if(opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;
    state->dmr_stereo_payload[i] = dibit;
  }

  for(i = 0; i < 12; i++)
  {
    dibit = state->dmr_stereo_payload[i];
    cachdata[cachInterleave[(i*2)]]   = (1 & (dibit >> 1)); // bit 1
    cachdata[cachInterleave[(i*2)+1]] = (1 & dibit);       // bit 0
  }

  //cach tact bits
  uint8_t tact_bits[7];
  for (i = 0; i < 7; i++)
  {
    tact_bits[i] = cachdata[i];
  }

  //decode and correct tact and compare
  if ( Hamming_7_4_decode (tact_bits) ) tact_okay = 1;
  if (tact_okay != 1) goto END;

  internalslot = state->currentslot = tact_bits[1];

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

  if ( strcmp (sync, DMR_BS_VOICE_SYNC) != 0)
  {
    sync_okay = 0;
    goto END;
  }

  //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
  for(i = 0; i < 18; i++)
  {
    dibit = getDibit(opts, state);
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
    fprintf (pFile, "\n%d 98 ", internalslot+1); //'98' is CACH designation value
    for (i = 0; i < 6; i++) //3 byte CACH
    {
      int cach_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
      fprintf (pFile, "%X", cach_byte);
    }
    fprintf (pFile, "\n%d 10 ", internalslot+1); //0x10 for "voice burst"
    for (i = 6; i < 72; i++) //33 bytes, no CACH
    {
      int dsp_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
      fprintf (pFile, "%X", dsp_byte);
    }
    fclose (pFile);
  }

  fprintf (stderr,"%s ", timestr);
  char polarity[3];
  char light[18];

  if (state->currentslot == 0)
  {
    sprintf (light, "%s", " [SLOT1]  slot2  ");
    //open MBEout file - slot 1
    if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
  } 
  else
  {
    sprintf (light, "%s", "  slot1  [SLOT2] ");
    //open MBEout file - slot 2
    if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_fR == NULL)) openMbeOutFileR (opts, state);
  } 
  if (opts->inverted_dmr == 0) sprintf (polarity, "%s", "+");
  else sprintf (polarity, "%s", "-");
  if (state->dmr_color_code != 16)
    fprintf (stderr,"Sync: %sDMR %s| Color Code=%02d | VC1*", polarity, light, state->dmr_color_code);
  else fprintf (stderr,"Sync: %sDMR %s| Color Code=XX | VC1*", polarity, light);

  dmr_alg_reset (opts, state);

  //copy ambe_fr frames first, running process mbe will correct them, 
  //but this also leads to issues extracting good le mi values when 
  //we go to do correction on them there too
  memcpy (m1, ambe_fr, sizeof(m1));
  memcpy (m2, ambe_fr2, sizeof(m2));
  memcpy (m3, ambe_fr3, sizeof(m3));

  if (opts->payload == 1) fprintf (stderr, "\n"); //extra line break necessary here
  // processMbeFrame (opts, state, NULL, ambe_fr, NULL);
  // processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
  // processMbeFrame (opts, state, NULL, ambe_fr3, NULL);

  processMbeFrame (opts, state, NULL, ambe_fr, NULL);
  if(internalslot == 0)
  {
    memcpy(state->f_l4[0], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
    memcpy(state->s_l4[0], state->s_l, sizeof(state->s_l));
    memcpy(state->s_l4u[0], state->s_lu, sizeof(state->s_lu));
  }
    
  else
  {
    memcpy(state->f_r4[0], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
    memcpy(state->s_r4[0], state->s_r, sizeof(state->s_r));
    memcpy(state->s_r4u[0], state->s_ru, sizeof(state->s_ru));
  }
    

  processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
  if(internalslot == 0)
  {
    memcpy(state->f_l4[1], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
    memcpy(state->s_l4[1], state->s_l, sizeof(state->s_l));
    memcpy(state->s_l4u[1], state->s_lu, sizeof(state->s_lu));
  }
    
  else
  {
    memcpy(state->f_r4[1], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
    memcpy(state->s_r4[1], state->s_r, sizeof(state->s_r));
    memcpy(state->s_r4u[1], state->s_ru, sizeof(state->s_ru));
  }

  processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
  if(internalslot == 0)
  {
    memcpy(state->f_l4[2], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
    memcpy(state->s_l4[2], state->s_l, sizeof(state->s_l));
    memcpy(state->s_l4u[2], state->s_lu, sizeof(state->s_lu));
  }
    
  else
  {
    memcpy(state->f_r4[2], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
    memcpy(state->s_r4[2], state->s_r, sizeof(state->s_r));
    memcpy(state->s_r4u[2], state->s_ru, sizeof(state->s_ru));
  }

  //collect the mi fragment
  dmr_late_entry_mi_fragment (opts, state, 1, m1, m2, m3);

  cach_err = dmr_cach (opts, state, cachdata);
  if (opts->payload == 0) fprintf (stderr, "\n");

  //update voice sync time for trunking purposes (particularly Con+)
  if (opts->p25_is_tuned == 1) state->last_vc_sync_time = time(NULL);

  //NOTE: Only play on slot 1, if slot 0, then it'll play after the next TDMA frame in the BS loop instead
  if (internalslot == 1 && opts->floating_point == 1 && opts->pulse_digi_rate_out == 8000)
    playSynthesizedVoiceFS3 (opts, state); //Float Stereo Mix 3v2
  if (internalslot == 1 && opts->floating_point == 0 && opts->pulse_digi_rate_out == 8000)
    playSynthesizedVoiceSS3 (opts, state); //Short Stereo Mix 3v2

  dmrBS (opts, state); //bootstrap into full TDMA frame for BS mode
  END:
  //if we have a tact err, then produce sync pattern/err message
  if (tact_okay != 1 || sync_okay != 1)
  {
    fprintf (stderr,"%s ", timestr);
    fprintf (stderr,"Sync:  DMR                  ");
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "| VOICE CACH/SYNC ERR");
    fprintf (stderr, "%s", KNRM);
    fprintf (stderr, "\n");
    //run refresh if either slot had an active MI in it.
    if (state->payload_algid >= 0x21)
    {
      state->currentslot = 0;
      dmr_alg_refresh (opts, state);
    }
    if (state->payload_algidR >= 0x21) 
    {
      state->currentslot = 1;
      dmr_alg_refresh (opts, state);
    }
    
    //failsafe to reset all data header and blocks when bad tact or emb
    dmr_reset_blocks (opts, state); 
  }

  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }

}
