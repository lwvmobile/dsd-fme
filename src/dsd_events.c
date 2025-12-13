/*-------------------------------------------------------------------------------
* dsd_events.c
* DSD-FME event history init, watchdog, push, and related functions
*
*
* LWVMOBILE
* 2025-05 DSD-FME Florida Man Edition
*-----------------------------------------------------------------------------*/

#include "dsd.h" 
#include "git_ver.h"
#include "synctype.h"

//init each event history struct passed into here
void init_event_history (Event_History_I * event_struct, uint8_t start, uint8_t stop)
{
  for (uint8_t i = start; i < stop; i++)
  {
    event_struct->Event_History_Items[i].write = 0;
    event_struct->Event_History_Items[i].color_pair = 4;
    event_struct->Event_History_Items[i].systype = -1;
    event_struct->Event_History_Items[i].subtype = -1;
    event_struct->Event_History_Items[i].sys_id1 = 0;
    event_struct->Event_History_Items[i].sys_id2 = 0;
    event_struct->Event_History_Items[i].sys_id3 = 0;
    event_struct->Event_History_Items[i].sys_id4 = 0;
    event_struct->Event_History_Items[i].sys_id5 = 0;
    event_struct->Event_History_Items[i].gi = 0;
    event_struct->Event_History_Items[i].enc = 0;
    event_struct->Event_History_Items[i].enc_alg = 0;
    event_struct->Event_History_Items[i].enc_key = 0;
    event_struct->Event_History_Items[i].mi = 0;
    event_struct->Event_History_Items[i].svc = 0;
    event_struct->Event_History_Items[i].source_id = 0;
    event_struct->Event_History_Items[i].target_id = 0;
    sprintf (event_struct->Event_History_Items[i].src_str, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].tgt_str, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].t_name,  "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].s_name,  "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].t_mode,  "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].s_mode,  "%s", "BUMBLEBEETUNA");
    event_struct->Event_History_Items[i].channel = 0;
    event_struct->Event_History_Items[i].event_time = 0;

    memset  (event_struct->Event_History_Items[i].pdu, 0, sizeof(event_struct->Event_History_Items[0].pdu));
    sprintf (event_struct->Event_History_Items[i].sysid_string, "%s", "");
    sprintf (event_struct->Event_History_Items[i].alias, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].gps_s, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].text_message, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].event_string, "%s", "BUMBLEBEETUNA");
    sprintf (event_struct->Event_History_Items[i].internal_str, "%s", "BUMBLEBEETUNA");
  }
}

void push_event_history (Event_History_I * event_struct)
{

  //Fixed, had it going in the wrong direction first time
  for (uint8_t i = 254; i >= 1; i--)
  {
    event_struct->Event_History_Items[i].write = event_struct->Event_History_Items[i-1].write;
    event_struct->Event_History_Items[i].color_pair = event_struct->Event_History_Items[i-1].color_pair;
    event_struct->Event_History_Items[i].systype = event_struct->Event_History_Items[i-1].systype;
    event_struct->Event_History_Items[i].subtype = event_struct->Event_History_Items[i-1].subtype;
    event_struct->Event_History_Items[i].sys_id1 = event_struct->Event_History_Items[i-1].sys_id1;
    event_struct->Event_History_Items[i].sys_id2 = event_struct->Event_History_Items[i-1].sys_id2;
    event_struct->Event_History_Items[i].sys_id3 = event_struct->Event_History_Items[i-1].sys_id3;
    event_struct->Event_History_Items[i].sys_id4 = event_struct->Event_History_Items[i-1].sys_id4;
    event_struct->Event_History_Items[i].sys_id5 = event_struct->Event_History_Items[i-1].sys_id5;
    event_struct->Event_History_Items[i].gi = event_struct->Event_History_Items[i-1].gi;
    event_struct->Event_History_Items[i].enc = event_struct->Event_History_Items[i-1].enc;
    event_struct->Event_History_Items[i].enc_alg = event_struct->Event_History_Items[i-1].enc_alg;
    event_struct->Event_History_Items[i].enc_key = event_struct->Event_History_Items[i-1].enc_key;
    event_struct->Event_History_Items[i].mi = event_struct->Event_History_Items[i-1].mi;
    event_struct->Event_History_Items[i].svc = event_struct->Event_History_Items[i-1].svc;
    event_struct->Event_History_Items[i].source_id = event_struct->Event_History_Items[i-1].source_id;
    event_struct->Event_History_Items[i].target_id = event_struct->Event_History_Items[i-1].target_id;
    sprintf (event_struct->Event_History_Items[i].src_str, "%s", event_struct->Event_History_Items[i-1].src_str);
    sprintf (event_struct->Event_History_Items[i].tgt_str, "%s", event_struct->Event_History_Items[i-1].tgt_str);
    sprintf (event_struct->Event_History_Items[i].t_name, "%s", event_struct->Event_History_Items[i-1].t_name);
    sprintf (event_struct->Event_History_Items[i].s_name, "%s", event_struct->Event_History_Items[i-1].s_name);
    sprintf (event_struct->Event_History_Items[i].t_mode, "%s", event_struct->Event_History_Items[i-1].t_mode);
    sprintf (event_struct->Event_History_Items[i].s_mode, "%s", event_struct->Event_History_Items[i-1].s_mode);
    event_struct->Event_History_Items[i].channel = event_struct->Event_History_Items[i-1].channel;
    event_struct->Event_History_Items[i].event_time = event_struct->Event_History_Items[i+1].event_time;

    memcpy  (event_struct->Event_History_Items[i].pdu, event_struct->Event_History_Items[i-1].pdu, sizeof(event_struct->Event_History_Items[0].pdu));
    sprintf (event_struct->Event_History_Items[i].sysid_string, "%s", event_struct->Event_History_Items[i-1].sysid_string);
    sprintf (event_struct->Event_History_Items[i].alias, "%s", event_struct->Event_History_Items[i-1].alias);
    sprintf (event_struct->Event_History_Items[i].gps_s, "%s", event_struct->Event_History_Items[i-1].gps_s);
    sprintf (event_struct->Event_History_Items[i].text_message, "%s", event_struct->Event_History_Items[i-1].text_message);
    sprintf (event_struct->Event_History_Items[i].event_string, "%s", event_struct->Event_History_Items[i-1].event_string);
    sprintf (event_struct->Event_History_Items[i].internal_str, "%s", event_struct->Event_History_Items[i-1].internal_str);
  }
}

void write_event_to_log_file (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t swrite, char * event_string) //pass completed event string here that is in the struct
{

  //open log file
  FILE * event_log_file;
  event_log_file = fopen(opts->event_out_file, "a");

  fprintf (event_log_file, "%s ", event_string);
  if (swrite == 1)
    fprintf (event_log_file, "Slot %d; ", slot+1);
  fprintf (event_log_file,"\n");

  char text_string[2000]; sprintf (text_string, "%s", "BUMBLEBEETUNA");
  if (strncmp(text_string, state->event_history_s[slot].Event_History_Items[0].text_message, 13) != 0)
    fprintf (event_log_file, "%s \n", state->event_history_s[slot].Event_History_Items[0].text_message);
  if (strncmp(text_string, state->event_history_s[slot].Event_History_Items[0].alias, 13) != 0)
    fprintf (event_log_file, " Talker Alias: %s \n", state->event_history_s[slot].Event_History_Items[0].alias);
  if (strncmp(text_string, state->event_history_s[slot].Event_History_Items[0].gps_s, 13) != 0)
    fprintf (event_log_file, " GPS: %s \n", state->event_history_s[slot].Event_History_Items[0].gps_s);
  if (strncmp(text_string, state->event_history_s[slot].Event_History_Items[0].internal_str, 13) != 0)
    fprintf (event_log_file, " DSD-FME: %s \n", state->event_history_s[slot].Event_History_Items[0].internal_str);

  //flush and close log file
  fflush (event_log_file);
  fclose (event_log_file);
}

// run once per loop to check for and push and update event history
void watchdog_event_history (dsd_opts * opts, dsd_state * state, uint8_t slot)
{

  //create a pointer to the current slot event history
  Event_History_I * event_struct = &state->event_history_s[slot];

  //is this a TDMA slot (append Slot value to end of written event history)
  uint8_t swrite = 0;

  //who is currently talking
  uint32_t source_id = 0;

  //last values pulled from the event history
  uint32_t last_source_id = event_struct->Event_History_Items[0].source_id;

  if (slot == 0)
    source_id = state->lastsrc;
  else
    source_id = state->lastsrcR;

  //if DMR BS or P25P2, then flag the swrite, so write can append slot value to event history log //|| state->lastsynctype == 32 || state->lastsynctype == 33 || state->lastsynctype == 34　MS
  if (state->lastsynctype == 10 || state->lastsynctype == 11 || state->lastsynctype == 12 || state->lastsynctype == 13)
    swrite = 1;
  
  else if (state->lastsynctype == 35 || state->lastsynctype == 36)
    swrite = 1;

  if (slot == 0) //BUGFIX: generic catch on FDMA systems so that we don't write duplicate data to slot 2 event history
  {
    //NXDN RID (TODO: Changeover to lastsrc later on)
    if (state->lastsynctype == 28 || state->lastsynctype == 29)
      source_id = state->nxdn_last_rid;

    if (state->lastsynctype == 30 || state->lastsynctype == 31) //YSF Fusion
    {
      source_id = 0;
      if (strncmp(state->ysf_src, "          ", 10) != 0) //if this field does not have ten spaces in it
      {
        for (uint8_t i = 0; i < 11; i++)
          source_id += state->ysf_src[i]; //convert to sum value to make a distinct enough src value
      }
    }

    if (state->lastsynctype == 8 || state->lastsynctype == 9 || state->lastsynctype == 16 || state->lastsynctype == 17) //M17 STR
    {
      source_id = (uint32_t)state->m17_src;
    }

    if (state->lastsynctype == 6 || state->lastsynctype == 7 || state->lastsynctype == 18 || state->lastsynctype == 19) //DSTAR
    {
      source_id = 0;
      for (uint8_t i = 0; i < 12; i++)
        source_id += state->dstar_src[i]; //convert to sum value to make a distinct enough src value

      //need a strncmp here for 8 spaces in this field first so we don't blip a blank into the event history
      if (strncmp(state->dstar_src, "        ", 8) == 0)
        source_id = 0;
    }

    if (state->lastsynctype == 20 || state->lastsynctype == 24 || state->lastsynctype == 21 || state->lastsynctype == 25 || state->lastsynctype == 22 || state->lastsynctype == 26 || state->lastsynctype == 23 || state->lastsynctype == 27) //dPMR
    {
      source_id = 0;
      for (uint8_t i = 0; i < 20; i++)
        source_id += state->dpmr_caller_id[i]; //convert to sum value to make a distinct enough src value

      //need a strncmp here for 8 spaces in this field first so we don't blip a blank into the event history
      if (strncmp(state->dpmr_caller_id, "      ", 6) == 0)
        source_id = 0;
    }

    if (state->lastsynctype == 14 || state->lastsynctype == 15 || state->lastsynctype == 37 || state->lastsynctype == 38) //EDACS Calls
    {
      source_id = 0;
      if (opts->p25_is_tuned == 1)
        source_id = state->lastsrc;
    }

  }

  //call alert beep when new call detected
  if (last_source_id == 0 && source_id != 0 && opts->call_alert == 1)
    beeper (opts, state, slot, 40, 86, 3);
  
  if (source_id != last_source_id && last_source_id != 0)
  {

    if (opts->event_out_file[0] != 0)
      write_event_to_log_file(opts, state, slot, swrite, event_struct->Event_History_Items[0].event_string);

    event_struct->Event_History_Items[0].write = 1; //written, or pushed at this point

    if (opts->static_wav_file == 0)
    {

      if (slot == 0 && opts->wav_out_f != NULL)
      {
        opts->wav_out_f = close_and_rename_wav_file(opts->wav_out_f, opts->wav_out_file, opts->wav_out_dir, event_struct);
        opts->wav_out_f = open_wav_file(opts->wav_out_dir, opts->wav_out_file, 8000, 0);
      }
        
      else if (slot == 1 && opts->wav_out_fR != NULL)
      {
        opts->wav_out_fR = close_and_rename_wav_file(opts->wav_out_fR, opts->wav_out_fileR, opts->wav_out_dir, event_struct);
        opts->wav_out_fR = open_wav_file(opts->wav_out_dir, opts->wav_out_fileR, 8000, 0);
      }

    }
      
    push_event_history (event_struct);
    init_event_history (event_struct, 0, 1);

    //clear out some strings and things
    memset(state->ysf_txt, 0, sizeof(state->ysf_txt));
    memset(state->dstar_gps, 0, sizeof(state->dstar_gps));
    memset(state->dstar_txt, 0, sizeof(state->dstar_txt));
    state->gi[slot] = -1; //return to an unset value

    //end of voice call alert
    if (opts->call_alert == 1)
      beeper (opts, state, slot, 40, 86, 3);
  }

}

//similar to above, but constantly testing and checking the most recent event only
//this will hopefully be more useful when dealing with an ongoing event with 
//features that update over time with embedded signalling, etc
void watchdog_event_current (dsd_opts * opts, dsd_state * state, uint8_t slot)
{

  //create a pointer to the current slot event history
  Event_History_I * event_struct = &state->event_history_s[slot];

  //ncurses color pairs
  uint8_t color_pair = 4; //default voice color (unknown gi)
  if (state->gi[slot] == 1)
    color_pair = 4; //default private voice color
  else if (state->gi[slot] == 0)
    color_pair = 4; //default group voice color

  //TODO: Flesh out more later on.
  uint32_t source_id = 0;
  uint32_t target_id = 0;
  char src_str[200]; memset (src_str, 0, sizeof(src_str));
  sprintf (src_str, "%s", "BUMBLEBEETUNA");
  char tgt_str[200]; memset (tgt_str, 0, sizeof(tgt_str));
  sprintf (tgt_str, "%s", "BUMBLEBEETUNA");

  //group import items
  char t_name[200]; memset (t_name, 0, sizeof(t_name));
  sprintf (t_name, "%s", "BUMBLEBEETUNA");
  char s_name[200]; memset (s_name, 0, sizeof(s_name));
  sprintf (s_name, "%s", "BUMBLEBEETUNA");
  char t_mode[200]; memset (t_mode, 0, sizeof(t_mode));
  sprintf (t_mode, "%s", "BUMBLEBEETUNA");
  char s_mode[200]; memset (s_mode, 0, sizeof(s_mode));
  sprintf (s_mode, "%s", "BUMBLEBEETUNA");

  uint16_t svc_opts = 0;
  uint8_t  subtype = 0;
  uint8_t  mfid = 0;
  uint32_t sys_id1 = 0;
  uint32_t sys_id2 = 0;
  uint32_t sys_id3 = 0;
  uint32_t sys_id4 = 0;
  uint32_t sys_id5 = 0;

  uint32_t channel = 0;

  uint8_t  enc    = 0;
  uint8_t  alg_id = 0;
  uint16_t key_id = 0;
  unsigned long long int mi;

  char sysid_string[200]; memset(sysid_string, 0, sizeof(sysid_string));
  sprintf (sysid_string, "%s", "");

  if (slot == 0)
  {
    source_id = state->lastsrc;
    target_id = state->lasttg;
    
    subtype = state->dmrburstL;
    mfid = state->dmr_fid;
    
    svc_opts = state->dmr_so;
    enc = (svc_opts >> 6) & 1;
    alg_id = state->payload_algid;
    key_id = (uint16_t)state->payload_keyid;

    mi = state->payload_mi;
  }
  else
  {
    source_id = state->lastsrcR;
    target_id = state->lasttgR;

    subtype = state->dmrburstR;
    mfid = state->dmr_fidR;

    svc_opts = state->dmr_soR;
    enc = (svc_opts >> 6) & 1;

    alg_id = state->payload_algidR;
    key_id = (uint16_t)state->payload_keyidR;
    mi = state->payload_miR;
  }

  //if P25 (if not P25, then these will all be zero anyways)
  sys_id1 = state->p2_wacn;
  sys_id2 = state->p2_sysid;
  if (state->nac != 0)
    sys_id3 = state->nac; //same as state->p2_cc, but zeroes out when no signal or error
  else sys_id3 = state->p2_cc;
  sys_id4 = state->p2_rfssid;
  sys_id5 = state->p2_siteid;

  if (sys_id1)
    sprintf (sysid_string, "P25_%05X%03X%03X_%d_%d", sys_id1, sys_id2, sys_id3, sys_id4, sys_id5);
  else sprintf (sysid_string, "P25_%03X", sys_id3);

  if (state->lastsynctype == 10 || state->lastsynctype == 11 || state->lastsynctype == 12 || state->lastsynctype == 13 ||
     state->lastsynctype == 32 || state->lastsynctype == 33 || state->lastsynctype == 34                                  )
  {
    sys_id1 = state->dmr_t3_syscode;
    sys_id2 = state->dmr_color_code;

    if (sys_id1)
      sprintf (sysid_string, "DMR_%X_CC_%d", sys_id1, sys_id2);
    else sprintf (sysid_string, "DMR_CC_%d", sys_id2);
  }

  if (slot == 0) //BUGFIX: generic catch on FDMA systems so that we don't write duplicate data to slot 2 event history
  {
    //NXDN RID (TODO: Changeover to lastsrc and lasttg later on)
    if (state->lastsynctype == 28 || state->lastsynctype == 29)
    {
      source_id = state->nxdn_last_rid;
      target_id = state->nxdn_last_tg;
      if (state->nxdn_cipher_type != 0)
        enc = 1;
      alg_id = state->nxdn_cipher_type;
      key_id = state->nxdn_key;

      sys_id1 = state->nxdn_location_site_code;
      sys_id2 = state->nxdn_location_sys_code;
      sys_id3 = state->nxdn_last_ran; //might be an issue on conventional systems that have a different RAN on the tx_rel or idle data bursts

      if (sys_id1)
        sprintf (sysid_string, "NXDN_%d_%d_RAN_%d", sys_id2, sys_id1, sys_id3);
      else sprintf (sysid_string, "NXDN_RAN_%d", sys_id3);
    }

    if (state->lastsynctype == 30 || state->lastsynctype == 31) //YSF Fusion
    {
      source_id = 0;
      if (strncmp(state->ysf_src, "          ", 10) != 0) //if this field does not have ten spaces in it
      {
        for (uint8_t i = 0; i < 11; i++)
          source_id += state->ysf_src[i]; //convert to sum value to make a distinct enough src value
      }

      //WIP: If Text, compile it here (still having issues with an empty txt string making a line break)
      uint8_t k = 0; char ysf_emp[21][21]; memset(ysf_emp, 0, sizeof(ysf_emp));
      if (memcmp(ysf_emp, state->ysf_txt, sizeof(state->ysf_txt)) != 0)
      {
        for (uint8_t i = 4; i < 8; i++)
        {
          for (uint8_t j = 0; j < 20; j++)
          {
            if (state->ysf_txt[i][j] != 0x2A)
              event_struct->Event_History_Items[0].text_message[k++] = state->ysf_txt[i][j];
            else event_struct->Event_History_Items[0].text_message[k++] = 0x20; //space

          }
          event_struct->Event_History_Items[0].text_message[k] = 0; //terminate
        }
      }
      else sprintf (event_struct->Event_History_Items[0].text_message, "%s", "BUMBLEBEETUNA");

      sprintf (sysid_string, "%s", "YSF");

      char temp_str[20];
      memset(temp_str, 0, sizeof(temp_str));

      //set src string as a non-spaced non-garbo char string
      for (uint8_t i = 0; i < 10; i++)
      {
        if (state->ysf_src[i] == 0x20) //spaces to underscore
          temp_str[i] = 0x5F;
        else if (state->ysf_src[i] > 0x20 && state->ysf_src[i] < 0x7F) //copy normal ascii range characters
          temp_str[i] = state->ysf_src[i];
        else if (state->ysf_src[i] == 0) break; //hit the terminator, so stop
        else temp_str[i] = 0x5F; //unknown to underscore
      }
      sprintf (src_str, "%s", temp_str);

      //same for tgt str
      memset(temp_str, 0, sizeof(temp_str));
      for (uint8_t i = 0; i < 10; i++)
      {
        if (state->ysf_tgt[i] == 0x20) //spaces to underscore
          temp_str[i] = 0x5F;
        else if (state->ysf_tgt[i] > 0x20 && state->ysf_tgt[i] < 0x7F) //copy normal ascii range characters
          temp_str[i] = state->ysf_tgt[i];
        else if (state->ysf_tgt[i] == 0) break; //hit the terminator, so stop
        else temp_str[i] = 0x5F; //unknown to underscore
      }
      sprintf (tgt_str, "%s", temp_str);

    }

    if (state->lastsynctype == 8 || state->lastsynctype == 9 || state->lastsynctype == 16 || state->lastsynctype == 17) //M17 STR
    {
      target_id = (uint32_t)state->m17_dst;
      source_id = (uint32_t)state->m17_src;
      sys_id1 = state->m17_can;
      sprintf (sysid_string, "M17_CAN_%d", sys_id1);
      sprintf (src_str, "%s", state->m17_src_csd);
      sprintf (tgt_str, "%s", state->m17_dst_csd);
    }

    if (state->lastsynctype == 6 || state->lastsynctype == 7 || state->lastsynctype == 18 || state->lastsynctype == 19) //DSTAR
    {
      source_id = 0;
      for (uint8_t i = 0; i < 12; i++)
        source_id += state->dstar_src[i]; //convert to sum value to make a distinct enough src value

      //need a strncmp here for 8 spaces in this field first so we don't blip a blank into the event history
      if (strncmp(state->dstar_src, "        ", 8) == 0)
        source_id = 0;

      sprintf (sysid_string, "%s", "DSTAR");

      char temp_str[20];
      memset(temp_str, 0, sizeof(temp_str));

      //set src string as a non-spaced non-garbo char string
      for (uint8_t i = 0; i < 12; i++)
      {
        if (state->dstar_src[i] == 0x20) //spaces to underscore
          temp_str[i] = 0x5F;
        else if (state->dstar_src[i] > 0x20 && state->dstar_src[i] < 0x7F) //copy normal ascii range characters
          temp_str[i] = state->dstar_src[i];
        else if (state->dstar_src[i] == 0) break; //hit the terminator, so stop
        else temp_str[i] = 0x5F; //unknown to underscore
      }
      sprintf (src_str, "%s", temp_str);

      //same for tgt str
      memset(temp_str, 0, sizeof(temp_str));
      for (uint8_t i = 0; i < 8; i++)
      {
        if (state->dstar_dst[i] == 0x20) //spaces to underscore
          temp_str[i] = 0x5F;
        else if (state->dstar_dst[i] > 0x20 && state->dstar_dst[i] < 0x7F) //copy normal ascii range characters
          temp_str[i] = state->dstar_dst[i];
        else if (state->dstar_dst[i] == 0) break; //hit the terminator, so stop
        else temp_str[i] = 0x5F; //unknown to underscore
      }
      sprintf (tgt_str, "%s", temp_str);
    }

    if (state->lastsynctype == 20 || state->lastsynctype == 24 || state->lastsynctype == 21 || state->lastsynctype == 25 || state->lastsynctype == 22 || state->lastsynctype == 26 || state->lastsynctype == 23 || state->lastsynctype == 27) //dPMR
    {
      source_id = 0;
      for (uint8_t i = 0; i < 20; i++)
        source_id += state->dpmr_caller_id[i]; //convert to sum value to make a distinct enough src value

      //need a strncmp here for 8 spaces in this field first so we don't blip a blank into the event history
      if (strncmp(state->dpmr_caller_id, "      ", 6) == 0)
        source_id = 0;

      sprintf (sysid_string, "DPMR_CC_%d", state->dpmr_color_code);

      sprintf (src_str, "%s", state->dpmr_caller_id);
      sprintf (tgt_str, "%s", state->dpmr_target_id);
    }

    if (state->lastsynctype == 14 || state->lastsynctype == 15 || state->lastsynctype == 37 || state->lastsynctype == 38) //EDACS Calls
    {
      source_id = 0;
      if (opts->p25_is_tuned == 1)
      {
        source_id = state->lastsrc;
        channel = state->edacs_tuned_lcn;
      }

      sys_id1 = state->edacs_site_id;
      sys_id2 = state->edacs_area_code;
      sys_id3 = state->edacs_sys_id;
      svc_opts = state->edacs_vc_call_type;
      char sup_str[200]; memset (sup_str, 0, sizeof(sup_str));
      sprintf (sup_str, "%s", "_");
      if (svc_opts & 0x02)
        strcat (sup_str, "Digital_");
      else strcat (sup_str, "Analog_");
      if (svc_opts & 0x04)
        strcat (sup_str, "Emergency_");
      if (svc_opts & 0x08)
        strcat (sup_str, "Group_");
      if (svc_opts & 0x10)
        strcat (sup_str, "I_");
      if (svc_opts & 0x20)
        strcat (sup_str, "ALL_");
      if (svc_opts & 0x40)
        strcat (sup_str, "INTER_");
      if (svc_opts & 0x80)
        strcat (sup_str, "TEST_");
      if (svc_opts & 0x100)
        strcat (sup_str, "AGENCY_");
      if (svc_opts & 0x200)
        strcat (sup_str, "FLEET_");
      if (svc_opts & 0x01)
        strcat (sup_str, "Voice_");
      strcat (sup_str, "Call");
      
      sprintf (sysid_string, "EDACS_SITE_%03d", sys_id1);
      strcat (sysid_string, sup_str);

      if (state->ea_mode == 0)
      {
        int afs = state->lasttg;
        sprintf(src_str, "%s", ""); sprintf(tgt_str, "%s", "");
        int a = (afs >> state->edacs_a_shift) & state->edacs_a_mask;
        int f = (afs >> state->edacs_f_shift) & state->edacs_f_mask;
        int s = afs & state->edacs_s_mask;
        sprintf (tgt_str, "%03d_AFS_%02d_%02d%01d", afs, a, f, s);
        if (state->lastsrc != 0x800 && state->lastsrc != 0)
          sprintf (src_str, "LID_%d", state->lastsrc);
        else sprintf (src_str, "LID_UNK");
      }
      
    }

  }

  //if we have a group_array import, search and load it here
  //will search and load both target values, and src values if available
  uint8_t t_name_loaded = 0;
  uint8_t s_name_loaded = 0;
  if (target_id != 0)
  {
    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == target_id)
      {
        sprintf (t_name, "%s", state->group_array[i].groupName);
        sprintf (t_mode, "%s", state->group_array[i].groupMode);
        t_name_loaded = 1;
        break;
      }
    }
  }

  if (source_id != 0) //&& state->gi[slot] == 1
  {
    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == source_id)
      {
        sprintf (s_name, "%s", state->group_array[i].groupName);
        sprintf (s_mode, "%s", state->group_array[i].groupMode);
        s_name_loaded = 1;
        break;
      }
    }
  }

  //system type string (P25, DMR, etc)
  const char * sys_string = "Digital";
  if (state->lastsynctype != -1)
    sys_string = SyncTypes[state->lastsynctype];
  
  //date and time strings
  char * timestr = getTimeN(time(NULL));
  char * datestr = getDateN(time(NULL));

  if (source_id != 0)
  {
    event_struct->Event_History_Items[0].write = 0;
    state->event_history_s[slot].Event_History_Items[0].color_pair = color_pair;
    if (state->lastsynctype != -1)
      event_struct->Event_History_Items[0].systype = state->lastsynctype;
    else event_struct->Event_History_Items[0].systype = 39; //generic digital call
    event_struct->Event_History_Items[0].subtype = subtype; //voice
    event_struct->Event_History_Items[0].gi = state->gi[slot]; //need this add this to link control messages
    event_struct->Event_History_Items[0].sys_id1 = sys_id1;
    event_struct->Event_History_Items[0].sys_id2 = sys_id2;
    event_struct->Event_History_Items[0].sys_id3 = sys_id3;
    event_struct->Event_History_Items[0].sys_id4 = sys_id4;
    event_struct->Event_History_Items[0].sys_id5 = sys_id5;
    event_struct->Event_History_Items[0].enc = enc;
    event_struct->Event_History_Items[0].enc_alg = alg_id;
    event_struct->Event_History_Items[0].enc_key = key_id;
    event_struct->Event_History_Items[0].mi = mi;
    event_struct->Event_History_Items[0].svc = svc_opts;
    event_struct->Event_History_Items[0].source_id = source_id;
    event_struct->Event_History_Items[0].target_id = target_id;
    event_struct->Event_History_Items[0].channel = channel; //need to add this to trunking messages, if tuned from call grant
    if (opts->playfiles == 0) //if playing back .mbe files with a time in it, don't set this
      event_struct->Event_History_Items[0].event_time = time(NULL);
    sprintf (event_struct->Event_History_Items[0].sysid_string, "%s", sysid_string);
    sprintf (event_struct->Event_History_Items[0].src_str, "%s", src_str);
    sprintf (event_struct->Event_History_Items[0].tgt_str, "%s", tgt_str);

    sprintf (event_struct->Event_History_Items[0].t_name, "%s", t_name);
    sprintf (event_struct->Event_History_Items[0].s_name, "%s", s_name);
    sprintf (event_struct->Event_History_Items[0].t_mode, "%s", t_mode);
    sprintf (event_struct->Event_History_Items[0].s_mode, "%s", s_mode);
    
  }

  //Craft an event string for ncurses event history, and a more complex string for logging
  char event_string[2000]; memset(event_string, 0, sizeof(event_string));

  //WIP: Seperate Voice Call Event Strings when SRC/TGT values are numerical,
  //and a seperate one for when they are string values (M17, YSF, DSTAR, and dPMR, or use special formatting)
  if (state->lastsynctype == 30 || state->lastsynctype == 31) //YSF Fusion //TODO: Data calls dumping a lot of events as VOICE
  {
    //TODO: See if we can add some decoded data as well in the future to an event string
    sprintf (event_string, "%s %s %s TGT: %s SRC: %s ", datestr, timestr, sys_string, state->ysf_tgt, state->ysf_src);
  }
  else if (state->lastsynctype == 16 || state->lastsynctype == 17) //M17
  {
    //TODO: See if we can add some decoded data as well in the future to an event string
    if (state->m17_dst == 0xFFFFFFFFFFFF)
      sprintf (event_string, "%s %s %s TGT: %s SRC: %s CAN: %02d;", datestr, timestr, sys_string, "BROADCAST", state->m17_src_str, state->m17_can);
    else
      sprintf (event_string, "%s %s %s TGT: %s SRC: %s CAN: %02d;", datestr, timestr, sys_string, state->m17_dst_str, state->m17_src_str, state->m17_can);
  }
  else if (state->lastsynctype == 6 || state->lastsynctype == 7 || state->lastsynctype == 18 || state->lastsynctype == 19) //DSTAR
  {
    //TODO: See if we can add some decoded data as well in the future to an event string
    sprintf (event_string, "%s %s %s TGT: %s SRC: %s ", datestr, timestr, sys_string, state->dstar_dst, state->dstar_src);
  }
  else if (state->lastsynctype == 20 || state->lastsynctype == 24 || state->lastsynctype == 21 || state->lastsynctype == 25 || state->lastsynctype == 22 || state->lastsynctype == 26 || state->lastsynctype == 23 || state->lastsynctype == 27) //dPMR
  {
    //TODO: See if we can add some decoded data as well in the future to an event string
    sprintf (event_string, "%s %s %s CC: %02d; TGT: %s; SRC: %s; ", datestr, timestr, sys_string, state->dpmr_color_code, state->dpmr_target_id, state->dpmr_caller_id);
    if (state->dPMRVoiceFS2Frame.Version[0] == 3)
      strcat (event_string, "Scrambler Enc; ");
  }
  else if (state->lastsynctype == 14 || state->lastsynctype == 15 || state->lastsynctype == 37 || state->lastsynctype == 38) //EDACS Calls
  {
    svc_opts = state->edacs_vc_call_type;
    char sup_str[200]; memset (sup_str, 0, sizeof(sup_str));
    sprintf (sup_str, "%s", "");
    if (svc_opts & 0x02)
      strcat (sup_str, "Digital ");
    else strcat (sup_str, "Analog ");
    if (svc_opts & 0x04)
      strcat (sup_str, "Emergency ");
    if (svc_opts & 0x08)
      strcat (sup_str, "Group ");
    if (svc_opts & 0x10)
      strcat (sup_str, "I ");
    if (svc_opts & 0x20)
      strcat (sup_str, "ALL ");
    if (svc_opts & 0x40)
      strcat (sup_str, "INTER ");
    if (svc_opts & 0x80)
      strcat (sup_str, "TEST ");
    if (svc_opts & 0x100)
      strcat (sup_str, "AGENCY ");
    if (svc_opts & 0x200)
      strcat (sup_str, "FLEET ");
    if (svc_opts & 0x01)
      strcat (sup_str, "Voice ");
    strcat (sup_str, "Call");

    if (state->ea_mode == 1)
    {
      sprintf (event_string, "%s %s %s TGT: %07d; SRC: %07d; LCN: %02d; SITE: %d:%d.%04X; %s; ", datestr, timestr, sys_string, target_id, source_id, channel, sys_id1, sys_id2, sys_id3, sup_str);
    }
    else
    {
      int afs = state->lasttg;
      int a = (afs >> state->edacs_a_shift) & state->edacs_a_mask;
      int f = (afs >> state->edacs_f_shift) & state->edacs_f_mask;
      int s = afs & state->edacs_s_mask;
      char afs_str[8];
      getAfsString(state, afs_str, a, f, s);
      char lid_str[20]; memset(lid_str, 0, sizeof(lid_str));
      sprintf(lid_str, "%s", "");
      if (state->lastsrc != 0 && state->lastsrc != 0x800)
        sprintf (lid_str, "LID: %05d;", state->lastsrc);
      else sprintf (lid_str, "LID: __UNK;");

      sprintf (event_string, "%s %s %s AFS: %s (%04d); %s LCN: %02d; Site: %d; %s; ", datestr, timestr, sys_string, afs_str, afs, lid_str, channel, sys_id1, sup_str);
    }
  }
  else if (state->lastsynctype == 10 || state->lastsynctype == 11 || state->lastsynctype == 12 || state->lastsynctype == 13 ||
     state->lastsynctype == 32 || state->lastsynctype == 33 || state->lastsynctype == 34                                  ) //DMR
  {
    if (sys_id1)
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; CC: %02d; SYS: %X; ", datestr, timestr, sys_string, target_id, source_id, sys_id2, sys_id1);
    else
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; CC: %02d; ", datestr, timestr, sys_string, target_id, source_id, sys_id2);
    if (enc)
      strcat(event_string, "ENC; ");
    if (alg_id != 0)
    {
      char ess_str[30];
      sprintf (ess_str, "ALG: %02X; KID: %02X; ", alg_id, key_id);
      strcat(event_string, ess_str);
    }

    //monitor for misc link control that may set a SO without having SO inside of it,
    //those could cause misc issues here, will need to observe and make adjustments
    if (svc_opts & 0x80)
        strcat (event_string, "Emergency; ");

    if (svc_opts & 0x08)
      strcat (event_string, "Broadcast; ");

    if (svc_opts & 0x04)
      strcat (event_string, "OVCM; ");

    if (state->gi[slot] == 0)
      strcat (event_string, "Group; ");
    else if (state->gi[slot] == 1)
      strcat (event_string, "Private; ");

    if (mfid == 0x10)
    {
      if (svc_opts & 0x20)
        strcat (event_string, "TXI; ");
      else if (svc_opts & 0x10)
        strcat (event_string, "TXI; "); //this is the svc opt bit that tells you when the next VC6 will be pre-empted, but not helpful here

      if (svc_opts & 0x03)
        strcat (event_string, "PRIORITY; "); //need to break this apart into each one, but need to double check the decoded value is accurate
    }
    
  }
  else if (state->lastsynctype == 0 || state->lastsynctype == 1 || state->lastsynctype == 35 || state->lastsynctype == 36)
  {
    if (sys_id1)
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; NAC: %03X; NET_STS: %05X:%03X:%d.%d; ", datestr, timestr, sys_string, target_id, source_id, sys_id3, sys_id1, sys_id2, sys_id4, sys_id5);
    else
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; NAC: %03X; ", datestr, timestr, sys_string, target_id, source_id, sys_id3);
    if (alg_id != 0 && alg_id != 0x80)
    {
      char ess_str[30];
      sprintf (ess_str, "ENC; ALG: %02X; KID: %04X; ", alg_id, key_id);
      strcat(event_string, ess_str);
    }
    if (svc_opts & 0x80)
      strcat (event_string, "Emergency; ");
    if (state->gi[slot] == 0)
      strcat (event_string, "Group; ");
    else if (state->gi[slot] == 1)
      strcat (event_string, "Private; ");
  }

  else if (state->lastsynctype == 28 || state->lastsynctype == 29)
  {
    if (sys_id1)
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; RAN: %02d; SYS: %d.%d; ", datestr, timestr, sys_string, target_id, source_id, sys_id3, sys_id2, sys_id1);
    else
      sprintf (event_string, "%s %s %s TGT: %08d; SRC: %08d; RAN: %02d; ", datestr, timestr, sys_string, target_id, source_id, sys_id3);
    if (enc)
      strcat(event_string, "ENC; ");
    if (alg_id != 0)
    {
      char ess_str[30];
      sprintf (ess_str, "ALG: %d; KID: %02X; ", alg_id, key_id);
      strcat(event_string, ess_str);
    }
    if (state->gi[slot] == 0)
      strcat (event_string, "Group; ");
    else if (state->gi[slot] == 1)
      strcat (event_string, "Private; ");
  }

  if (t_name_loaded)
  {
    char group[420];
    sprintf (group, "TName: %s; Mode: %s; ", t_name, t_mode);
    strcat (event_string, group);
  }
  if (s_name_loaded)
  {
    char private[420];
    sprintf (private, "SName: %s; Mode: %s; ", s_name, s_mode);
    strcat (event_string, private);
  }

  sprintf (event_struct->Event_History_Items[0].event_string, "%s", event_string);

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
}

void watchdog_event_datacall (dsd_opts * opts, dsd_state * state, uint32_t src, uint32_t dst, char * data_string, uint8_t slot)
{
  UNUSED(opts);
  state->event_history_s[slot].Event_History_Items[0].write = 0;
  if (state->event_history_s[slot].Event_History_Items[0].color_pair == 4) //if not set previously by specific decoder //don't touch this one
    state->event_history_s[slot].Event_History_Items[0].color_pair = 4; //default data color //you can change this one
  state->event_history_s[slot].Event_History_Items[0].systype = state->lastsynctype;
  state->event_history_s[slot].Event_History_Items[0].subtype = 6; //data
  state->event_history_s[slot].Event_History_Items[0].gi = state->gi[slot];
  state->event_history_s[slot].Event_History_Items[0].enc = 0;
  state->event_history_s[slot].Event_History_Items[0].enc_alg = 0;
  state->event_history_s[slot].Event_History_Items[0].enc_key = 0;
  state->event_history_s[slot].Event_History_Items[0].mi = 0;
  state->event_history_s[slot].Event_History_Items[0].svc = 0;
  state->event_history_s[slot].Event_History_Items[0].source_id = src;
  state->event_history_s[slot].Event_History_Items[0].target_id = dst;
  state->event_history_s[slot].Event_History_Items[0].channel = 0;
  state->event_history_s[slot].Event_History_Items[0].event_time = time(NULL);

  //date and time strings //getTimeN(time(NULL)); //getDateN(time(NULL));
  char * timestr = getTimeN(time(NULL));
  char * datestr = getDateN(time(NULL));

  char event_string[2000]; memset (event_string, 0, sizeof(event_string));
  sprintf (event_string, "%s %s ", datestr, timestr);
  strcat (event_string, data_string); //may need to check for potential overflow of this
  sprintf (state->event_history_s[slot].Event_History_Items[0].event_string, "%s", event_string); //could change this to a strncpy to prevent potential overflow

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

  //call alert on data calls
  if (opts->call_alert)
    beeper (opts, state, slot, 80, 20, 3);
}
