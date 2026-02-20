/*-------------------------------------------------------------------------------
* dsd_ncurses_handler.c
* DSD-FME ncurses terminal user input handler
*
* LWVMOBILE
* 2025-05 DSD-FME Florida Man Edition
*-----------------------------------------------------------------------------*/

#include "dsd.h"

uint8_t ncurses_input_handler(dsd_opts * opts, dsd_state * state, int c)
{

  struct stat st_wav = {0};

  // //debug char value
  // if (c != -1)
  //   fprintf (stderr, "\n User Input: %i / %c ;;", c, c);

  //TEST: Handle Escape Characters
  //if issues arise, just go back to the way it was before
  //this is primarly a fix for scroll wheel activatign the menu
  //and Windows Powershell right-click doing a copy and paste 
  //and sendign tons of garbage getch chars here and changing things
  if (c == '\033')
  {
    //TODO: Find way to getch all the chars immediately after, or just
    //run getch in a loop in case of somebody copying and pasting a ton of things accidentally
    for (int i = 0; i < 100000; i++) //wonder how slow this will be
       getch();
    c = -1;
    return 1;
  }

  //keyboard shortcuts - codes same as ascii codes
  if (c == 10) //Return / Enter key, open menu
    ncursesMenu (opts, state);

  //use k and l keys to test tg hold toggles on slots 1 and slots 2
  if (c == 107) //'k' key, hold tg on slot 1 for trunking purposes, or toggle clear
  {
    if (state->tg_hold == 0)
      state->tg_hold = state->lasttg;
    else state->tg_hold = 0;

    if ( (opts->frame_nxdn48 == 1 || opts->frame_nxdn96 == 1) && (state->tg_hold == 0) )
      state->tg_hold = state->nxdn_last_tg;

    else if (opts->frame_provoice == 1 && state->ea_mode == 0)
      state->tg_hold = state->lastsrc;
  }

  if (c == 108) //'l' key, hold tg on slot 2 for trunking purposes, or toggle clear
  {
    if (state->tg_hold == 0)
      state->tg_hold = state->lasttgR;
    else state->tg_hold = 0;
  }

  //toggling when 48k/1 OSS still has some lag -- needed to clear out the buffer when switching
  if (c == 49) // '1' key, toggle slot1 on
  {
    //switching, but want to control each seperately plz
    if (opts->slot1_on == 1)
    {
      opts->slot1_on = 0; if (opts->slot_preference == 0) opts->slot_preference = 2;
      // opts->slot_preference = 1; //slot 2
      //clear any previously buffered audio
      state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
      state->audio_out_buf_p = state->audio_out_buf + 100;
      memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
      memset (state->audio_out_buf, 0, 100 * sizeof (short));
      state->audio_out_idx2 = 0;
      state->audio_out_idx = 0;
    }
    else if (opts->slot1_on == 0)
    {
      opts->slot1_on = 1;
      if (opts->audio_out_type == 5) //OSS 48k/1
      {
        opts->slot_preference = 0; //slot 1
        // opts->slot2_on = 0; //turn off slot 2
      }
    }
  }

  if (c == 50) // '2' key, toggle slot2 on
  {
    //switching, but want to control each seperately plz
    if (opts->slot2_on == 1)
    {
      opts->slot2_on = 0;
      opts->slot_preference = 0; //slot 1
      //clear any previously buffered audio
      state->audio_out_float_buf_pR = state->audio_out_float_bufR + 100;
      state->audio_out_buf_pR = state->audio_out_bufR + 100;
      memset (state->audio_out_float_bufR, 0, 100 * sizeof (float));
      memset (state->audio_out_bufR, 0, 100 * sizeof (short));
      state->audio_out_idx2R = 0;
      state->audio_out_idxR = 0;
    }
    else if (opts->slot2_on == 0)
    {
      opts->slot2_on = 1;
      if (opts->audio_out_type == 5) //OSS 48k/1
      {
        opts->slot_preference = 1; //slot 2
        // opts->slot1_on = 0; //turn off slot 1
      }
    }
  }

  // if (c == 51) //'3' key, toggle slot preference on 48k/1
  // {
  //   if (opts->slot_preference == 1) opts->slot_preference = 0;
  //   else opts->slot_preference = 1;
  // }

  if (c == 51) //'3' key, cycle slot preference
  {
    if (opts->slot_preference == 0 || opts->slot_preference == 1) opts->slot_preference++;
    else opts->slot_preference = 0;
  }

  if (c == 43) //+ key, increment audio_gain
  {

    if (opts->audio_gain < 50)
      opts->audio_gain++;

    state->aout_gain = opts->audio_gain;
    state->aout_gainR = opts->audio_gain;

    opts->audio_gainR = opts->audio_gain;

  }

  if (c == 45) //- key, decrement audio_gain
  {

    if (opts->audio_gain > 0)
      opts->audio_gain--;

    state->aout_gain = opts->audio_gain;
    state->aout_gainR = opts->audio_gain;

    //reset to default on 0 for auto
    if (opts->audio_gain == 0)
    {
      state->aout_gain = 25;
      state->aout_gainR = 25;
    }

    opts->audio_gainR = opts->audio_gain;

  }

  if (c == 42) // * key, increment audio_gainA
  {
    if (opts->audio_gainA < 100)
      opts->audio_gainA++;
  }

  if (c == 47) // / key, decrement audio_gainA
  {
    if (opts->audio_gainA > 0)
      opts->audio_gainA--;
  }

  if (c == 122) //'z' key, toggle payload to console
  {
    if (opts->payload == 1) opts->payload = 0;
    else opts->payload = 1;
  }

  if (c == 99) //'c' key, toggle compact mode
  {
    if (opts->ncurses_compact == 1) opts->ncurses_compact = 0;
    else opts->ncurses_compact = 1;
  }

  if (c == 116) //'t' key, toggle trunking
  {
    if (opts->p25_trunk == 1) opts->p25_trunk = 0;
    else opts->p25_trunk = 1;
  }

  if (c == 121) //'y' key, toggle scanner mode
 {
  if (opts->scanner_mode == 1) opts->scanner_mode = 0;
  else opts->scanner_mode = 1;
  opts->p25_trunk = 0; //turn off trunking mode
 }

  if (c == 97) //'a' key, toggle call alert beep
  {
    if (opts->call_alert == 1) opts->call_alert = 0;
    else opts->call_alert = 1;
  }

  if (c == 104) //'h' key, cycle history off, short, long
  {
    opts->ncurses_history++;
    opts->ncurses_history %= 3;
  }

  if (c == 113) //'q' key, quit
  {
    exitflag = 1;
  }

  if (c == 52) // '4' key, toggle force privacy key over fid and svc (dmr)
  {
    if (state->forced_alg_id == 1 || state->forced_alg_id == 0x21) state->forced_alg_id = 0;
    else state->forced_alg_id = 1;
  }

  if (c == 54) // '6' key, toggle force rc4 key over missing pi header/late entry
  {
    if (state->forced_alg_id == 1 || state->forced_alg_id == 0x21) state->forced_alg_id = 0;
    else state->forced_alg_id = 0x21;
  }

  if (c == 105) //'i' key, toggle signal inversion on inverted types
  {
    //Set all signal for inversion or uninversion
    if (opts->inverted_dmr == 0)
    {
      opts->inverted_dmr = 1;
      opts->inverted_dpmr = 1;
      opts->inverted_x2tdma = 1;
      opts->inverted_ysf = 1;
      opts->inverted_nxdn = 1;
    }
    else
    {
      opts->inverted_dmr = 0;
      opts->inverted_dpmr = 0;
      opts->inverted_x2tdma = 0;
      opts->inverted_ysf = 0;
      opts->inverted_nxdn = 0;
    }
  }

  if (c == 109) //'m' key, toggle qpsk/c4fm - everything but phase 2
  {
    if (state->rf_mod == 0)
    {
      opts->mod_c4fm = 0;
      opts->mod_qpsk = 1;
      opts->mod_gfsk = 0;
      state->rf_mod = 1;
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }
    else
    {
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }
  }

  if (c == 77) //'M' key, toggle qpsk - phase 2 6000 sps
  {
    if (state->rf_mod == 0)
    {
      opts->mod_c4fm = 0;
      opts->mod_qpsk = 1;
      opts->mod_gfsk = 0;
      state->rf_mod = 1;
      state->samplesPerSymbol = 8;
      state->symbolCenter = 3;
    }
    else
    {
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      state->samplesPerSymbol = 8;
      state->symbolCenter = 3;
    }
  }

  if (c == 82) //'R', save symbol capture bin with date/time string as name
  {
    //for filenames (no colons, etc)
    char * timestr = getTime();
    char * datestr = getDate();
    sprintf (opts->symbol_out_file, "%s_%s_dibit_capture.bin", datestr, timestr);
    openSymbolOutFile (opts, state);

    //add a system event to echo in the event history
    state->event_history_s[0].Event_History_Items[0].color_pair = 4;
    char event_str[2000]; memset (event_str, 0, sizeof(event_str));
    sprintf (event_str, "DSD-FME Dibit Capture File Started: %s;", opts->symbol_out_file);
    watchdog_event_datacall (opts, state, 0xFFFFFF, 0xFFFFFF, event_str, 0);
    state->lastsrc = 0; //this could wipe a call src if they hit 'R' while call in slot 1 in progress
    watchdog_event_history(opts, state, 0);
    watchdog_event_current(opts, state, 0);

    //allocated memory pointer needs to be free'd
    if (timestr != NULL)
    {
      free (timestr);
      timestr = NULL;
    }
    if (datestr != NULL)
    {
      free (datestr);
      datestr = NULL;
    }
    opts->symbol_out_file_creation_time = time(NULL);
    opts->symbol_out_file_is_auto = 1;
  }

  if (c == 114) //'r' key, stop capturing symbol capture bin file
  {
    if (opts->symbol_out_f)
    {
      closeSymbolOutFile (opts, state);
      sprintf (opts->audio_in_dev, "%s", opts->symbol_out_file);

      //add a system event to echo in the event history
      state->event_history_s[0].Event_History_Items[0].color_pair = 4;
      char event_str[2000]; memset (event_str, 0, sizeof(event_str));
      sprintf (event_str, "DSD-FME Dibit Capture File  Closed: %s;", opts->symbol_out_file);
      watchdog_event_datacall (opts, state, 0xFFFFFF, 0xFFFFFF, event_str, 0);
      state->lastsrc = 0; //this could wipe a call src if they hit 'R' while call in slot 1 in progress
      watchdog_event_history(opts, state, 0);
      watchdog_event_current(opts, state, 0);

    }
    opts->symbol_out_file_is_auto = 0;
  }

 #ifdef __CYGWIN__
 //do nothing
 #else
 if (c == 32) //'space bar' replay last bin file (rework to do wav files too?)
 {
  struct stat stat_buf;
  if (stat(opts->audio_in_dev, &stat_buf) != 0)
  {
    fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
    goto SKIPR;
  }
  if (S_ISREG(stat_buf.st_mode))
  {
    opts->symbolfile = fopen(opts->audio_in_dev, "r");
    opts->audio_in_type = 4; //symbol capture bin files
  }
  SKIPR: ; //do nothing
 }
 #endif

 if (c == 80) //'P' key - start per call wav files //TODO: Fix
 {
  char wav_file_directory[1024];
  sprintf (wav_file_directory, "%s", opts->wav_out_dir);
  wav_file_directory[1023] = '\0';
  if (stat(wav_file_directory, &st_wav) == -1)
  {
    fprintf (stderr, "%s wav file directory does not exist\n", wav_file_directory);
    fprintf (stderr, "Creating directory %s to save decoded wav files\n", wav_file_directory);
    mkdir(wav_file_directory, 0700);
  }
  fprintf (stderr,"\n Per Call Wav File Enabled to Directory: %s;.\n", opts->wav_out_dir);
  srand(time(NULL)); //seed random for filenames (so two filenames aren't the exact same datetime string on initailization)
  opts->wav_out_f  = open_wav_file(opts->wav_out_dir, opts->wav_out_file, 8000, 0);
  opts->wav_out_fR = open_wav_file(opts->wav_out_dir, opts->wav_out_fileR, 8000, 0);
  opts->dmr_stereo_wav = 1;
 }

  if (c == 112) //'p' key - stop all per call wav files //TODO: Fix
  {
    //TODO: Add Closing of RAW files as well?
    opts->wav_out_f = close_and_rename_wav_file(opts->wav_out_f, opts->wav_out_file, opts->wav_out_dir, &state->event_history_s[0]);
    opts->wav_out_fR = close_and_rename_wav_file(opts->wav_out_fR, opts->wav_out_fileR, opts->wav_out_dir, &state->event_history_s[1]);
    opts->wav_out_file[0] = 0; //Bugfix for decoded wav file display after disabling
    opts->wav_out_fileR[0] = 0;
    opts->dmr_stereo_wav = 0;
  }

  //
  #ifdef __CYGWIN__ //this might be okay on Aero as well, will need to look into and/or test
  //
  #else
  if (c == 115) //'s' key, stop playing wav or symbol in files
  {
    if (opts->symbolfile != NULL)
    {
      if (opts->audio_in_type == 4)
      {
        fclose(opts->symbolfile);
      }
    }

    if (opts->audio_in_type == 2) //wav input file
    {
      sf_close(opts->audio_in_file);
    }

    if (opts->audio_out_type == 0)
    {
      opts->audio_in_type = 0;
      openPulseInput(opts);
    }
    else opts->audio_in_type = 5; //exitflag = 1;

  }
  #endif

  //makes buzzing sound when locked out in new audio config and short, probably something to do with processaudio running or not running
  if (state->lasttg != 0 && opts->frame_provoice != 1 && c == 33) //SHIFT+'1' key (exclamation point), lockout slot 1 or conventional tg from tuning/playback during session
  {
    state->group_array[state->group_tally].groupNumber = state->lasttg;
    sprintf (state->group_array[state->group_tally].groupMode, "%s", "B");
    sprintf (state->group_array[state->group_tally].groupName, "%s", "LOCKOUT");
    state->group_tally++;

    sprintf (state->event_history_s[0].Event_History_Items[0].internal_str, "Target: %d; has been locked out; User Lock Out.", state->lasttg);
    watchdog_event_current(opts, state, 0);
    sprintf (state->call_string[0], "%s", "                     "); //21 spaces

    //if we have an opened group file, let's write a group lock out into it to make it permanent
    if (opts->group_in_file[0] != 0) //file is available
    {
      FILE * pFile; //file pointer
      //open file by name that is supplied in the ncurses terminal, or cli
      pFile = fopen (opts->group_in_file, "a");
      fprintf (pFile, "%d,B,LOCKOUT,%02X\n", state->lasttg, state->payload_algid);
      fclose (pFile);
    }

    //extra safeguards due to sync issues with NXDN
    memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

    memset (state->active_channel, 0, sizeof(state->active_channel));

    //reset dmr blocks
    dmr_reset_blocks (opts, state);

    //zero out additional items
    state->lasttg = 0;
    state->lasttgR = 0;
    state->lastsrc = 0;
    state->lastsrcR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_miP = 0;
    state->payload_miN = 0;
    opts->p25_is_tuned = 0;
    state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;

    //tune back to the control channel -- NOTE: Doesn't work correctly on EDACS Analog Voice
    //RIGCTL
    if (opts->p25_trunk == 1 && opts->use_rigctl == 1)
    {
      //drop all items (failsafe)
      noCarrier(opts, state);
      if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
    }

    //rtl
    #ifdef USE_RTLSDR
    if (opts->p25_trunk == 1 && opts->audio_in_type == 3)
    {
      //drop all items (failsafe)
      noCarrier(opts, state);
      rtl_dev_tune (opts, state->p25_cc_freq);
    }
    #endif

    state->last_cc_sync_time = time(NULL);

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }

  }

  if (state->lasttgR != 0 && opts->frame_provoice != 1 && c == 64) //SHIFT+'2' key (@ at sign), lockout slot 2 tdma tgR from tuning/playback during session
  {
    state->group_array[state->group_tally].groupNumber = state->lasttgR;
    sprintf (state->group_array[state->group_tally].groupMode, "%s", "B");
    sprintf (state->group_array[state->group_tally].groupName, "%s", "LOCKOUT");
    state->group_tally++;

    sprintf (state->event_history_s[1].Event_History_Items[0].internal_str, "Target: %d; has been locked out; User Lock Out.", state->lasttgR);
    watchdog_event_current(opts, state, 1);
    sprintf (state->call_string[1], "%s", "                     "); //21 spaces

    //if we have an opened group file, let's write a group lock out into it to make it permanent
    if (opts->group_in_file[0] != 0) //file is available
    {
      FILE * pFile; //file pointer
      //open file by name that is supplied in the ncurses terminal, or cli
      pFile = fopen (opts->group_in_file, "a");
      fprintf (pFile, "%d,B,LOCKOUT,%02X\n", state->lasttgR, state->payload_algidR);
      fclose (pFile);
    }

    //extra safeguards due to sync issues with NXDN
    memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

    memset (state->active_channel, 0, sizeof(state->active_channel));

    //reset dmr blocks
    dmr_reset_blocks (opts, state);

    //zero out additional items
    state->lasttg = 0;
    state->lasttgR = 0;
    state->lastsrc = 0;
    state->lastsrcR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_miP = 0;
    state->payload_miN = 0;
    opts->p25_is_tuned = 0;
    state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;

    //tune back to the control channel -- NOTE: Doesn't work correctly on EDACS Analog Voice
    //RIGCTL
    if (opts->p25_trunk == 1 && opts->use_rigctl == 1)
    {
      //drop all items (failsafe)
      noCarrier(opts, state);
      if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
    }

    //rtl
    #ifdef USE_RTLSDR
    if (opts->p25_trunk == 1 && opts->audio_in_type == 3)
    {
      //drop all items (failsafe)
      noCarrier(opts, state);
      rtl_dev_tune (opts, state->p25_cc_freq);
    }
    #endif

    state->last_cc_sync_time = time(NULL);

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }

  }

  if (opts->p25_trunk == 1 && c == 119) //'w' key, toggle white list/black list mode
  {
    if (opts->trunk_use_allow_list == 1) opts->trunk_use_allow_list = 0;
    else opts->trunk_use_allow_list = 1;
  }

  if (opts->p25_trunk == 1 && c == 117) //'u' key, toggle tune private calls
  {
    if (opts->trunk_tune_private_calls == 1) opts->trunk_tune_private_calls = 0;
    else opts->trunk_tune_private_calls = 1;
  }

  if (opts->p25_trunk == 1 && c == 100) //'d' key, toggle tune data calls
  {
    if (opts->trunk_tune_data_calls == 1) opts->trunk_tune_data_calls = 0;
    else opts->trunk_tune_data_calls = 1;
  }

  if (opts->p25_trunk == 1 && c == 101) //'e' key, toggle tune enc calls (P25 only on certain grants)
  {
    if (opts->trunk_tune_enc_calls == 1) opts->trunk_tune_enc_calls = 0;
    else opts->trunk_tune_enc_calls = 1;
  }

  if (opts->p25_trunk == 1 && c == 84) //'T' key, toggle tune group calls
  {
    if (opts->trunk_tune_group_calls == 1) opts->trunk_tune_group_calls = 0;
    else opts->trunk_tune_group_calls = 1;
  }

  if (c == 70) //'F' key - toggle agressive sync/crc failure/ras
  {
    if (opts->aggressive_framesync == 0) opts->aggressive_framesync = 1;
    else opts->aggressive_framesync = 0;
  }

  if (c == 68) //'D' key - Reset DMR Site Parms/Call Strings, etc.
  {
    //dmr trunking/ncurses stuff
    state->dmr_rest_channel = -1; //init on -1
    state->dmr_mfid = -1; //

    //dmr mfid branding and site parms
    sprintf(state->dmr_branding_sub, "%s", "");
    sprintf(state->dmr_branding, "%s", "");
    sprintf (state->dmr_site_parms, "%s", "");

    //DMR Location Area - DMRLA B***S***
    opts->dmr_dmrla_is_set = 0;
    opts->dmr_dmrla_n = 0;

    //reset NXDN info
    state->nxdn_location_site_code = 0;
    state->nxdn_location_sys_code = 0;
    sprintf (state->nxdn_location_category, "%s", " ");

    state->nxdn_last_ran = -1; //0
    state->nxdn_ran = 0; //0

    state->nxdn_rcn = 0;
    state->nxdn_base_freq = 0;
    state->nxdn_step = 0;
    state->nxdn_bw = 0;

  }

  //Debug/Troubleshooting Option
  if (c == 90) //'Z' key - Simulate NoCarrier/No VC/CC sync to zero out more stuff (capital Z)
  {
    // opts->p25_is_tuned = 0;
    state->last_cc_sync_time = 0;
    state->last_vc_sync_time = 0;
    noCarrier(opts, state);
  }

  if (c == 93) //']' key - increment event history indexer
  {
      state->eh_index++;
  }

  if (c == 91) //'[' key - decrement event history indexer
  {
      state->eh_index--;
  }

  if (c == 92) //'\' key - toggle events for slot displayed, and reset eh_index
  {
    state->eh_slot ^= 1;
    state->eh_index = 0;
  }

  //attempt retry to TCP Audio server
  if (c == 56) // '8' key, try audio in type 8 (TCP Audio Server connection) using defaults OR whatever the user last specified
  {

    opts->tcp_sockfd = Connect(opts->tcp_hostname, opts->tcp_portno);
    if (opts->tcp_sockfd != 0)
    {
      //reset audio input stream
      opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
      opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
      opts->audio_in_file_info->channels=1;
      opts->audio_in_file_info->seekable=0;
      opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
      opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);

      if(opts->tcp_file_in == NULL)
      {
        fprintf(stderr, "Error, couldn't Connect to TCP with libsndfile: %s\n", sf_strerror(NULL));
      }
      else
      {
        //close pulse input if it is currently open
        if (opts->audio_in_type == 0) closePulseInput(opts);
        fprintf (stderr, "TCP Socket Connected Successfully.\n");
        opts->audio_in_type = 8;
      }
    }
    else fprintf (stderr, "TCP Socket Connection Error.\n");
  }

  if (c == 57) //'9' key, try rigctl connection with default values
  {
    //use same or last specified host for TCP audio sink for connection
    memcpy (opts->rigctlhostname, opts->tcp_hostname, sizeof (opts->rigctlhostname) );
    opts->rigctl_sockfd = Connect(opts->rigctlhostname, opts->rigctlportno);
    if (opts->rigctl_sockfd != 0) opts->use_rigctl = 1;
    else opts->use_rigctl = 0;
  }

  //if trunking and user wants to just go back to the control channel and skip this call
  if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && c == 67) //Capital C key - Return to CC
  {

    //extra safeguards due to sync issues with NXDN
    memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

    memset (state->active_channel, 0, sizeof(state->active_channel));

    //reset dmr blocks
    dmr_reset_blocks (opts, state);

    //zero out additional items
    state->lasttg = 0;
    state->lasttgR = 0;
    state->lastsrc = 0;
    state->lastsrcR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_miP = 0;
    state->payload_miN = 0;
    opts->p25_is_tuned = 0;
    state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;

    //tune back to the control channel -- NOTE: Doesn't work correctly on EDACS Analog Voice
    //RIGCTL
    if (opts->p25_trunk == 1 && opts->use_rigctl == 1)
    {
      if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
    }

    //rtl
    #ifdef USE_RTLSDR
    if (opts->p25_trunk == 1 && opts->audio_in_type == 3) rtl_dev_tune (opts, state->p25_cc_freq);
    #endif

    state->last_cc_sync_time = time(NULL);

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }

    //if P25p1 Data Revert on P25p2 TDMA CC, flip symbolrate
    if (state->p25_cc_is_tdma == 1)
    {
      state->samplesPerSymbol = 8;
      state->symbolCenter = 3;
    }

    fprintf (stderr, "\n User Activated Return to CC; \n ");

  }

  //if trunking or scanning, manually cycle forward through channels loaded (can be run without trunking or scanning enabled)
  if ( (opts->use_rigctl == 1 || opts->audio_in_type == 3) && c == 76) //Capital L key - Cycle Channels Forward
  {

    //extra safeguards due to sync issues with NXDN
    memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

    memset (state->active_channel, 0, sizeof(state->active_channel));

    //reset dmr blocks
    dmr_reset_blocks (opts, state);

    //zero out additional items
    state->lasttg = 0;
    state->lasttgR = 0;
    state->lastsrc = 0;
    state->lastsrcR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_miP = 0;
    state->payload_miN = 0;
    opts->p25_is_tuned = 0;
    state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;

    //just copy and pasted the cycle logic for CC/signal hunting on no sync
    if (state->lcn_freq_roll >= state->lcn_freq_count) //fixed this to skip the extra wait out at the end of the list
    {
      state->lcn_freq_roll = 0; //reset to zero
    }

    //roll an extra value up if the current is the same as what's already loaded -- faster hunting on Cap+, etc
    if (state->lcn_freq_roll != 0)
    {
      if (state->trunk_lcn_freq[state->lcn_freq_roll-1] == state->trunk_lcn_freq[state->lcn_freq_roll])
      {
        state->lcn_freq_roll++;
        //check roll again if greater than expected, then go back to zero
        if (state->lcn_freq_roll >= state->lcn_freq_count)
        {
          state->lcn_freq_roll = 0; //reset to zero
        }
      }
    }

    //check that we have a non zero value first, then tune next frequency
    if (state->trunk_lcn_freq[state->lcn_freq_roll] != 0)
    {
      //rigctl
      if (opts->use_rigctl == 1)
      {
        if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
        SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[state->lcn_freq_roll]);
      }
      //rtl
      if (opts->audio_in_type == 3)
      {
        #ifdef USE_RTLSDR
        rtl_dev_tune (opts, state->trunk_lcn_freq[state->lcn_freq_roll]);
        #endif
      }

      fprintf (stderr, "\n User Activated Channel Cycle;");
      fprintf (stderr, "  Tuning to Frequency: %.06lf MHz\n",
                (double)state->trunk_lcn_freq[state->lcn_freq_roll]/1000000);

    }
    state->lcn_freq_roll++;
    state->last_cc_sync_time = time(NULL);


    //may need to test to see if we want to do the conditional below or not for symbol rate flipping

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
    }

    //if P25p1 Data Revert on P25p2 TDMA CC, flip symbolrate
    if (state->p25_cc_is_tdma == 1)
    {
      state->samplesPerSymbol = 8;
      state->symbolCenter = 3;
    }

  }

  if (c == 118 && opts->audio_in_type == 3) //'v' key, cycle rtl volume multiplier, when active
  {
    if (opts->rtl_volume_multiplier == 1 || opts->rtl_volume_multiplier == 2) opts->rtl_volume_multiplier++;
    else opts->rtl_volume_multiplier = 1;
  }

  if (c == 86) // 'V' Key, toggle LPF
  {
    if (opts->use_lpf == 0) opts->use_lpf = 1;
    else opts->use_lpf = 0;
  }

  if (c == 66) // 'B' Key, toggle HPF
  {
    if (opts->use_hpf == 0) opts->use_hpf = 1;
    else opts->use_hpf = 0;
  }

  if (c == 78) // 'N' Key, toggle PBF
  {
    if (opts->use_pbf == 0) opts->use_pbf = 1;
    else opts->use_pbf = 0;
  }

  if (c == 72) // 'H' Key, toggle HPF on digital
  {
    if (opts->use_hpf_d == 0) opts->use_hpf_d = 1;
    else opts->use_hpf_d = 0;
  }

  if(opts->frame_provoice == 1 && c == 65) //'A' Key, toggle ESK mask 0xA0
  {
    if (state->esk_mask == 0) state->esk_mask = 0xA0;
    else state->esk_mask = 0;
  }

  if(opts->frame_provoice == 1 && c == 83) //'S' Key, toggle STD or EA mode and reset
  {
    if (state->ea_mode == -1) state->ea_mode = 0;
    else if (state->ea_mode == 0) state->ea_mode = 1;
    else state->ea_mode = 0;

    //reset -- test to make sure these don't do weird things when reset
    state->edacs_site_id = 0;
    state->edacs_lcn_count = 0;
    state->edacs_cc_lcn = 0;
    state->edacs_vc_lcn = 0;
    state->edacs_tuned_lcn = -1;
    state->edacs_vc_call_type = 0;
    state->p25_cc_freq = 0;
    opts->p25_is_tuned = 0;
    state->lasttg = 0;
    state->lastsrc = 0;

  }

  //RTL PPM Manual Adjustment
  if (c == 125)
    opts->rtlsdr_ppm_error++;

  if (c == 123)
    opts->rtlsdr_ppm_error--;

  //RTL Gain Manual Adjustment
  if (c == 103)
  {
    if (opts->rtl_gain_value > 0)
    {
      opts->rtl_gain_value--;
      opts->rtl_gain_actual = 0;
    }
  }

  if (c == 71)
  {
    if (opts->rtl_gain_value < 49)
    {
      opts->rtl_gain_value++;
      opts->rtl_gain_actual = 0;
    }
  }
    

  //anything with an entry box will need the inputs and outputs stopped first
  //so probably just write a function to handle c input, and when c = certain values
  //needing an entry box, then stop all of those

  return 0;

} //end ncursesPrinter