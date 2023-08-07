/*-------------------------------------------------------------------------------
 * EDACS-FME 
 * A program for decoding EDACS (ported to DSD-FME)
 * https://github.com/lwvmobile/edacs-fm
 *
 * Portions of this software originally from:
 * https://github.com/sp5wwp/ledacs
 * XTAL Labs
 * 30 IV 2016
 * Many thanks to SP5WWP for permission to use and modify this software
 *
 * Encoder/decoder for binary BCH codes in C (Version 3.1)
 * Robert Morelos-Zaragoza
 * 1994-7
 *
 * LWVMOBILE
 * 2022-10 Version EDACS-FM Florida Man Edition
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

char * getDateE(void) {
  #ifdef AERO_BUILD
  char datename[80];
  #else
  char datename[99];
  #endif
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y-%m-%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

char * getTimeE(void) //get pretty hh:mm:ss timestamp
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

void edacs(dsd_opts * opts, dsd_state * state)
{
  //changed to ulli here for 32-bit cygwin (not sure if was an issue, but playing it safe)
  unsigned long long int fr_1 = 0xFFFFFFFFFF; //40-bit frames
  unsigned long long int fr_2 = 0; //each is a 40 bit frame that repeats 3 times
  unsigned long long int fr_3 = 0; //two messages per frame
  unsigned long long int fr_4 = 0xFFFFFFFFFF; 
  unsigned long long int fr_5 = 0;
  unsigned long long int fr_6 = 0;

  //BCH stuff
  unsigned long long int fr_1m = 0xFFFFFFF; //28-bit 7X message portion to pass to bch handler
  unsigned long long int fr_1t = 0xFFFFFFFFFF; //40 bit return from BCH with poly attached
  unsigned long long int fr_4m = 0xFFFFFFF; //28-bit 7X message portion to pass to bch handler
  unsigned long long int fr_4t = 0xFFFFFFFFFF; //40 bit return from BCH with poly attached

  unsigned char command = 0xFF;
  unsigned char mt1 = 0x1F;
  unsigned char mt2 = 0xF;
  unsigned char mta = 0;
  unsigned char lcn = 0;

  //commands; may not use these anymore
  unsigned int vcmd = 0xEE; //voice command variable
  unsigned int idcmd = 0xFD;
  unsigned int peercmd = 0xF88; //using for EA detection test
  unsigned int netcmd = 0xF3; //using for Networked Test

  state->edacs_vc_lcn = -1; //init on negative for ncurses and tuning

  int i, j;
  int edacs_bit[241] = {0}; //zero out bit array and collect bits into it.

  for (i = 0; i < 240; i++) //288 bits every transmission minus 48 bit (24 dibit) sync pattern
  {
    edacs_bit[i] = getDibit (opts, state); //getDibit returns binary 0 or 1 on GFSK signal (Edacs and PV)
  }

  //push the edacs_bit array into fr format from EDACS-FM
  for (i = 0; i < 40; i++)
  {
    //only fr_1 and fr4 are going to matter
    fr_1 = fr_1 << 1;
    fr_1 = fr_1 | edacs_bit[i]; 
    fr_2 = fr_2 << 1;
    fr_2 = fr_2 | edacs_bit[i+40];
    fr_3 = fr_3 << 1;
    fr_3 = fr_3 | edacs_bit[i+80];
    
    fr_4 = fr_4 << 1; 
    fr_4 = fr_4 | edacs_bit[i+120]; 
    fr_5 = fr_5 << 1;
    fr_5 = fr_5 | edacs_bit[i+160];
    fr_6 = fr_6 << 1;
    fr_6 = fr_6 | edacs_bit[i+200]; 

  }

  fr_1 = fr_1 & 0xFFFFFFFFFF;
  fr_4 = fr_4 & 0xFFFFFFFFFF;

  fr_1m = (fr_1 & 0xFFFFFFF000) >> 12;
  fr_4m = (fr_4 & 0xFFFFFFF000) >> 12;

  //take our message and create a new crc for it, if it matches the old crc, then we have a good frame
  fr_1t = edacs_bch (fr_1m);
  fr_4t = edacs_bch (fr_4m);

  fr_1t = fr_1t & 0xFFFFFFFFFF;
  fr_4t = fr_4t & 0xFFFFFFFFFF;

  if (fr_1 != fr_1t || fr_4 != fr_4t)
  {
    fprintf (stderr, " BCH FAIL ");
  }
  else //BCH Pass, continue from here.
  {

    //ESK on/off detection, I honestly don't remember the logic for this anymore, but it works fine
	  if ( (((fr_1t & 0xF000000000) >> 36) != 0xB)  && (((fr_1t & 0xF000000000) >> 36) != 0x1) && (((fr_1t & 0xFF00000000) >> 32) != 0xF3) )
	  {
      //experimenting with values here, not too high, and not too low
		  if ( (((fr_1t & 0xF000000000) >> 36) <= 0x8 ))
      { 
		    state->esk_mask = 0xA0;
      }
      //ideal value would be 5, but some other values exist that don't allow it
		  if ( (((fr_1t & 0xF000000000) >> 36) > 0x8 ) )
      { 
		    state->esk_mask = 0x0;
      }
	  }

    //Standard/Networked Auto Detection
	  //if (command == netcmd) //netcmd is F3 Standard/Networked (I think)
    if ( (fr_1t >> 32 == netcmd) || (fr_1t >> 32 == (netcmd ^ 0xA0)) ) 
    { 
		  state->ea_mode = 0; //disable extended addressing mode
	  }

    //EA Auto detection //peercmd is 0xF88 peer site relay 0xFF80000000 >> 28
	  if (fr_1t >> 28 == peercmd || fr_1t >> 28 == (peercmd ^ 0xA00) )
    {
		  state->ea_mode = 1; //enable extended addressing mode
	  }

    //Start Extended Addressing Mode 
    if (state->ea_mode == 1)
    {
      command = ((fr_1t & 0xFF00000000) >> 32) ^ state->esk_mask;
      mt1 = (command & 0xF8) >> 3;
      mt2 = (fr_1t & 0x780000000) >> 31;
      lcn = (fr_1t & 0x3E0000000) >> 29; //only valid during calls, status 

      //Site ID
      unsigned long long int site_id = 0; //we probably could just make this an int as well as the state variables
      if (mt1 == 0x1F && mt2 == 0xA)
      {
        site_id = ((fr_1 & 0x1F000) >> 12) | ((fr_1 & 0x1F000000) >> 19);
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Site ID [%02llX][%03lld] Extended Addressing", site_id, site_id);
        fprintf (stderr, "%s", KNRM);
        state->edacs_site_id = site_id;
      }
      //Patch Groups
      else if (mt1 == 0x1F && mt2 == 0xC)
      {
        int patch_site = ((fr_4t & 0xFF00000000) >> 32); //is site info valid, 0 for all sites? else patch only good on site listed?
        int sourcep = ((fr_1t & 0xFFFF000) >> 12);
        int targetp = ((fr_4t & 0xFFFF000) >> 12);
        fprintf (stderr, " Patch -- Site [%d] Source [%d] Target [%d] ", patch_site, sourcep, targetp);
      }
      //Adjacent Sites
      else if (mt1 == 0x1F && mt2 == 0x1)
      {
        fprintf (stderr, " Adjacent Site");
        if ( ((fr_1t & 0xFF000) >> 12) > 0 )
        {
          int adj = (fr_1t & 0xFF000) >> 12;
          int adj_l = (fr_1t & 0x1F000000) >> 24;
          fprintf (stderr, " [%02X][%03d] on CC LCN [%02d]", adj, adj, adj_l);
        }
      }
      //Control Channel LCN
      else if (mt1 == 0x1F && mt2 == 0x8)
      {
        fprintf (stderr, " Control Channel LCN");
        if (((fr_4t >> 12) & 0x1F) != 0)
        {
          state->edacs_cc_lcn = ((fr_4t >> 12) & 0x1F);
          if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) //26, or 27. shouldn't matter don't think cc lcn will give a status lcn val 
          {
            state->edacs_lcn_count = state->edacs_cc_lcn;
          }
          fprintf (stderr, " [%d]", state->edacs_cc_lcn);

          //check for control channel lcn frequency if not provided in channel map or in the lcn list
          if (state->trunk_lcn_freq[state->edacs_cc_lcn-1] == 0)
          {
            long int lcnfreq = 0;
            //if using rigctl, we can ask for the currrent frequency
            if (opts->use_rigctl == 1)
            {
              lcnfreq = GetCurrentFreq (opts->rigctl_sockfd);
              if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
            }
            //if using rtl input, we can ask for the current frequency tuned
            if (opts->audio_in_type == 3)
            {
              lcnfreq = (long int)opts->rtlsdr_center_freq;
              if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
            }
          }

          //set trunking cc here so we know where to come back to
          if (opts->p25_trunk == 1 && state->trunk_lcn_freq[state->edacs_cc_lcn-1] != 0)
          {
            state->p25_cc_freq = state->trunk_lcn_freq[state->edacs_cc_lcn-1]; //index starts at zero, lcn's locally here start at 1
          }
        }
      }
      //disabling kick command, data looks like its just FFFF, no actual values, can't verify accuracy
      // else if (mt1 == 0x1F && mt2 == 0xB) //KICK LISTING for EA?? Unverified, but probably observed in Unitrunker way back when.
      // {
      //   int kicked = (fr_4t & 0xFFFFF000) >> 12;
      //   fprintf (stderr, " FR_1 [%010llX]", fr_1t);
      //   fprintf (stderr, " FR_3 [%010llX]", fr_3);
      //   fprintf (stderr, " FR_4 [%010llX]", fr_4t);
      //   fprintf (stderr, " FR_6 [%010llX]", fr_6);
      //   fprintf (stderr, " Kick Command?");
      // }
      //Voice Call Grant Update
      // mt1 0x3 is Digital group voice call, 0x2 Group Data Channel, 0x1 TDMA call
      else if (mt1 >= 0x1 && mt1 <= 0x3)
      {
        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        if (group != 0)  state->lasttg = group;
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " Group [%05d] Source [%08d] LCN[%02d]", group, source, lcn);

        char mode[8]; //allow, block, digital enc

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == group)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
          }
        }

        if (mt1 == 0x1) fprintf (stderr, " TDMA Call"); //never observed, wonder if any EDACS systems ever carried a TDMA signal (X2-TDMA?)
        if (mt1 == 0x2) fprintf (stderr, " Group Data Call"); //Never Seen this one before
        if (mt1 == 0x3) fprintf (stderr, " Digital Call"); //ProVoice, this is what we always get on SLERS EA
        fprintf (stderr, "%s", KNRM);

        //this is working now with the new import setup
        if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block 
        {
          if (lcn < 26 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s pV Site %lld TG %d SRC %d.wav", getDateE(), getTimeE(), state->edacs_site_id, group, source);
              openWavOutFile (opts, state);
            }
            
            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
      		    SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1; 
              
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              #endif
            }

          }
          
        }
      }
      else //print frames for debug/analysis
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Unknown Command");
      }

    }
    //Start Standard or Networked Mode
    else if (state->ea_mode == 0)
    {
      //standard or networked
      command = ((fr_1t & 0xFF00000000) >> 32) ^ state->esk_mask;
      lcn     = (fr_1t & 0xF8000000) >> 27;

      //site ID and CC LCN
      if (command == 0xFD)
      {
        int site_id = (fr_1t & 0xFF000) >> 12;
        int cc_lcn = (fr_1t & 0x1F000000) >> 24;

        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Site ID [%02X][%03d] on CC LCN [%02d] Standard/Networked", site_id, site_id, cc_lcn);
        fprintf (stderr, "%s", KNRM);
        state->edacs_site_id = site_id;
        state->edacs_cc_lcn = cc_lcn;

        if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = state->edacs_cc_lcn;
        }

        //check for control channel lcn frequency if not provided in channel map or in the lcn list
        if (state->trunk_lcn_freq[state->edacs_cc_lcn-1] == 0)
        {
          long int lcnfreq = 0;
          //if using rigctl, we can ask for the currrent frequency
          if (opts->use_rigctl == 1)
          {
            lcnfreq = GetCurrentFreq (opts->rigctl_sockfd);
            if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
          }
          //if using rtl input, we can ask for the current frequency tuned
          if (opts->audio_in_type == 3)
          {
            lcnfreq = (long int)opts->rtlsdr_center_freq;
            if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
          }
        }

        //set trunking cc here so we know where to come back to
        if (opts->p25_trunk == 1 && state->trunk_lcn_freq[state->edacs_cc_lcn-1] != 0)
        {
          state->p25_cc_freq = state->trunk_lcn_freq[state->edacs_cc_lcn-1]; //index starts at zero, lcn's locally here start at 1
        }

      }

      //voice call assignment
      else if (command == 0xEE || command == 0xEF)
      {

        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }
        state->edacs_vc_lcn = lcn;
        //just going to use the default 4-4-3 A-FS scheme, making it user configurable would be a pain
        int afs = (fr_1t & 0x7FF000) >> 12;
        int a = afs >> 7; 
        int fs = afs & 0x7F;
        int status  = (fr_1t & 0xF00000000) >> 32;
        if (afs > 0) state->lastsrc = afs; 
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " AFS [0x%03X] [%02d-%03d] LCN [%02d]", afs, a, fs, lcn);

        char mode[8]; //allow, block, digital enc

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == afs)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
          }
        }

        if (command == 0xEE)
        {
          //no handling for raw audio yet...that works properly atleast
          fprintf (stderr, " Analog"); 
        }
        else //Digital Call (ProVoice, hopefully not Aegis in 2022)
        {
          fprintf (stderr, " Digital");
          //this is working now with the new import setup
          if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
          {
            if (lcn < 26 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
            {
              //openwav file and do per call right here
              if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
              {
                sprintf (opts->wav_out_file, "./WAV/%s %s pV Site %lld AFS %X.wav", getDateE(), getTimeE(), state->edacs_site_id, afs);
                openWavOutFile (opts, state);
              }

              if (opts->use_rigctl == 1)
              {
                if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
      		      SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1; 
              }

              if (opts->audio_in_type == 3) //rtl dongle
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1;
                #endif
              }

            }
          
          }
        }
        fprintf (stderr, "%s", KNRM);
      }
      //Network Command, Not sure of how to handle, but just show frames
      else if (command == 0xF3)
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Network Command");
      }

      else //print frames for debug/analysis
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Unknown Command");
      }

    } //end Standard or Networked

  } 

  fprintf (stderr, "\n");

}

 