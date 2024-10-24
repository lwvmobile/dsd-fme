/*-------------------------------------------------------------------------------
 * dmr_csbk.c
 * DMR Control Signal Data PDU (CSBK, MBC) Handler and Related Functions
 *
 * Portions of Connect+ code reworked from Boatbod OP25
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * Portions of Capacity+ code reworked from Eric Cottrell
 * Source: https://github.com/LinuxSheeple-E/dsd/blob/Feature/DMRECC/dmr_csbk.c
 *
 * LWVMOBILE
 * 2023-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#define PCLEAR_TUNE_AWAY //disable if slower return is preferred
//function for handling Control Signalling PDUs (CSBK, MBC) messages
void dmr_cspdu (dsd_opts * opts, dsd_state * state, uint8_t cs_pdu_bits[], uint8_t cs_pdu[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors)
{
  
  int  csbk_lb   = 0;
  int  csbk_pf   = 0;
  int  csbk_o    = 0;
  int  csbk_fid  = 0;

  long int ccfreq = 0;

  csbk_lb  = ( (cs_pdu[0] & 0x80) >> 7 );
  csbk_pf  = ( (cs_pdu[0] & 0x40) >> 6 );
  csbk_o   =    cs_pdu[0] & 0x3F; 
  csbk_fid =    cs_pdu[1]; //feature set id
  UNUSED(csbk_lb);

  //check, regardless of CRC err
  if (IrrecoverableErrors == 0)
  {
    //Hytera XPT CSBK Check -- if bits 0 and 1 are used as lcss, gi, ts, then the pf bit may be set on
    if ( csbk_fid == 0x68 && (csbk_o == 0x0A || csbk_o == 0x0B) ) csbk_pf = 0; 
    if (csbk_pf == 1) //check the protect flag, don't run if set
    {
      fprintf (stderr, "%s", KRED); 
      fprintf (stderr, "\n Protected Control Signalling Block(s)");
      fprintf (stderr, "%s", KNRM);
    }
  }
  
  if(IrrecoverableErrors == 0 && CRCCorrect == 1) 
  {
    //clear stale Active Channel messages here
    if ( ((time(NULL) - state->last_active_time) > 3) && ((time(NULL) - state->last_vc_sync_time) > 3))
    {
      memset (state->active_channel, 0, sizeof(state->active_channel));
    }

    //update time to prevent random 'Control Channel Signal Lost' hopping
    //in the middle of voice call on current Control Channel (con+ and t3)
    state->last_cc_sync_time = time(NULL); 

    if (csbk_pf == 0) //okay to run
    {

      //set overarching manufacturer in use when non-standard feature id set is up
      if (csbk_fid != 0) state->dmr_mfid = csbk_fid; 

      fprintf (stderr, "%s", KYEL); 
      
      //7.1.1.1.1 Channel Grant CSBK/MBC PDU
      if (csbk_o >= 48 && csbk_o <= 56 )
      {

        //maintain this to allow users to hardset the cc freq as map[0]; otherwise, set from rigctl or rtl freq at c_aloha_sys_parms
        // if (state->p25_cc_freq == 0 && state->trunk_chan_map[0] != 0) state->p25_cc_freq = state->trunk_chan_map[0];
        
        //initial line break
        fprintf (stderr, "\n");

        long int freq = 0;

        //all of these messages share the same format (thankfully)
        if (csbk_o == 48) fprintf (stderr, " Private Voice Channel Grant (PV_GRANT)");
        if (csbk_o == 49) fprintf (stderr, " Talkgroup Voice Channel Grant (TV_GRANT)");
        if (csbk_o == 50) fprintf (stderr, " Broadcast Voice Channel Grant (BTV_GRANT)");  //listed as private in the appendix (error?)
        if (csbk_o == 51) fprintf (stderr, " Private Data Channel Grant: Single Item (PD_GRANT)");
        if (csbk_o == 52) fprintf (stderr, " Talkgroup Data Channel Grant: Single Item (TD_GRANT)");
        if (csbk_o == 53) fprintf (stderr, " Duplex Private Voice Channel Grant (PV_GRANT_DX)");
        if (csbk_o == 54) fprintf (stderr, " Duplex Private Data Channel Grant (PD_GRANT_DX)");
        if (csbk_o == 55) fprintf (stderr, " Private Data Channel Grant: Multi Item (PD_GRANT)");
        if (csbk_o == 56 && state->synctype != 33) //when not MS Data Sync (shares same OPcode as BS_DWN_ACT)
          fprintf (stderr, " Talkgroup Data Channel Grant: Multi Item (TD_GRANT)");

        //Logical Physical Channel Number
        uint16_t lpchannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 12); 
        if (lpchannum == 0) fprintf (stderr, " - Invalid Channel"); //invalid channel, not sure why this would even be transmitted
        else if (lpchannum == 0xFFF) fprintf (stderr, " - Absolute"); //This is from an MBC, signalling an absolute and not a logical
        else fprintf (stderr, " - Logical");

        //dsdplus channel values
        uint16_t pluschannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 13); //lcn bit included
        pluschannum += 1; //add a one for good measure

        //LCN conveyed here is the tdma timeslot variety, and not the RF frequency variety
        uint8_t lcn = cs_pdu_bits[28];
        //the next three bits can have different meanings depending on which item above for context
        uint8_t st1 = cs_pdu_bits[29]; //late_entry, hi_rate, reserved(dx)
        uint8_t st2 = cs_pdu_bits[30]; //emergency
        uint8_t st3 = cs_pdu_bits[31]; //offset, call direction
        //target and source are always the same bits
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        UNUSED2(st1, st3);

        //broadcast calls show no tgt/src info in ETSI manaual, but seem to correspond with same values in embedded link control
        // if (csbk_o == 50)
        // {
        //   //seeting to all MS units and Dispatch? May need to just set to 0 and see what emb pulls out of it
        //   target = 0xFFFFFF; //ALLMSI
        //   source = 0xFFFECB; //DISPATI
        // }

        //move mbc variables out of if statement
        uint8_t mbc_lb = 0; //
        uint8_t mbc_pf = 0;
        uint8_t mbc_csbko = 0;
        uint8_t mbc_res = 0;
        uint8_t mbc_cc = 0;
        uint8_t mbc_cdeftype = 0;
        uint8_t mbc_res2 = 0;
        unsigned long long int mbc_cdefparms = 0;
        uint16_t mbc_lpchannum = 0;
        uint16_t mbc_abs_tx_int = 0;
        uint16_t mbc_abs_tx_step = 0;
        uint16_t mbc_abs_rx_int = 0;
        uint16_t mbc_abs_rx_step = 0;
        UNUSED5(mbc_lb, mbc_pf, mbc_csbko, mbc_res, mbc_cc);
        UNUSED4(mbc_res2, mbc_cdefparms, mbc_abs_tx_int, mbc_abs_tx_step);

        fprintf (stderr, "\n");
        //rewrote this to make it clear which is the lpcn, timeslot (lcn), and the combination of both (DSDPlus style combined LSN) on channel numbering
        //the TS is displayed as a +1 since while the bit values are 0 or 1, the slot numbering in the manual specifically states TDMA channel (slot) 1 or TDMA channel (slot) 2
        fprintf (stderr, "  LPCN: %04d; TS: %d; LPCN+TS: %04d; Target: %08d - Source: %08d ", lpchannum, lcn+1, pluschannum, target, source);

        if (st2) fprintf (stderr, "Emergency; ");

        //check for special gateway identifiers (probably just on broadcast?)
        dmr_gateway_identifier (source, target);

        if (lpchannum == 0xFFF) //This is from an MBC, signalling an absolute and not a logical
        {
          //7.1.1.1.2 Channel Grant Absolute Parameters CG_AP appended MBC PDU
          mbc_lb = cs_pdu_bits[96]; //
          mbc_pf = cs_pdu_bits[97];
          mbc_csbko = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[98], 6);
          mbc_res = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[104], 4);
          mbc_cc = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[108], 4);
          mbc_cdeftype = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[112], 4); //see 7.2.19.7 = 0 for channel parms, 1 through FFFF reserved
          mbc_res2 = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[116], 2);
          long long int mbc_cdefparms = (unsigned long long int)ConvertBitIntoBytes(&cs_pdu_bits[118], 58); //see 7.2.19.7.1

          //this is how you read the 58 parm bits according to the appendix 7.2.19.7.1
          if (mbc_cdeftype == 0) //if 0, then absolute channel parms
          {
            mbc_lpchannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[118], 12);
            mbc_abs_tx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[130], 10);
            mbc_abs_tx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[140], 13);
            mbc_abs_rx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[153], 10);
            mbc_abs_rx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[163], 13);

            //tx_int (Mhz) + (tx_step * 125) = tx_freq
            //rx_int (Mhz) + (rx_step * 125) = rx_freq
            fprintf (stderr, "\n");
            fprintf (stderr, "  RX APCN: %04d; RX INT: %d; RX STEP: %d;", mbc_lpchannum, mbc_abs_rx_int, mbc_abs_rx_step );
            //The Frequency we want to tune is the RX Frequency
            freq = (mbc_abs_rx_int * 1000000 ) + (mbc_abs_rx_step * 125); 
          }
          else fprintf (stderr, "\n  MBC Channel Grant - Unknown Parms: %015llX", mbc_cdefparms); //for any reserved values
        }

        //print frequency from absolute
        if (freq != 0 && lpchannum == 0xFFF)
          fprintf (stderr, "\n  Frequency: %.6lf MHz", (double)freq/1000000);

        //run external channel map function on logical
        if (lpchannum != 0 && lpchannum != 0xFFF)
        {
          freq = state->trunk_chan_map[lpchannum];
          if (freq != 0)
            fprintf (stderr, "\n  Frequency: %.6lf MHz", (double)freq/1000000);
          else fprintf (stderr, "\n  Frequency Not Found in Channel Map;");
        }

        //add active channel string to display
        if (lpchannum != 0 && lpchannum != 0xFFF)
        {
          if (csbk_o == 49 || csbk_o == 50) sprintf (state->active_channel[lcn], "Active Group Ch: %d TG: %d; ", lpchannum, target);
          else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) sprintf (state->active_channel[lcn], "Active Data Ch: %d TG: %d; ", lpchannum, target);
          else sprintf (state->active_channel[lcn], "Active Private Ch: %d TG: %d; ", lpchannum, target);
        } 
        else if (lpchannum == 0xFFF)
        {
          if (csbk_o == 49 || csbk_o == 50) sprintf (state->active_channel[lcn], "Active Group Ch: %d TG: %d; ", mbc_lpchannum, target);
          else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) sprintf (state->active_channel[lcn], "Active Data Ch: %d TG: %d; ", mbc_lpchannum, target);
          else sprintf (state->active_channel[lcn], "Active Private Ch: %d TG: %d; ", mbc_lpchannum, target);
        }
        //update last active channel time
        state->last_active_time = time(NULL);

        //Skip tuning group calls if group calls are disabled
        if (opts->trunk_tune_group_calls == 0 && csbk_o == 49) goto SKIPCALL; //TV_GRANT
        if (opts->trunk_tune_group_calls == 0 && csbk_o == 50) goto SKIPCALL; //BTV_GRANT
        if (csbk_o == 50) csbk_o = 49; //flip to normal group call for group tuning

        //Allow tuning of data calls if user wishes by flipping the csbk_o to a group voice call
        int data_call = 0;
        if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56)
        {
          data_call = 1; //don't add data calls to call history until redone
          if (opts->trunk_tune_data_calls == 1) csbk_o = 49;
        }

        //Skip tuning private calls if private calls are disabled
        if (opts->trunk_tune_private_calls == 0 && csbk_o != 49) goto SKIPCALL;
         
        //if not a data channel grant (only tuning to voice channel grants)
        if (csbk_o == 48 || csbk_o == 49 || csbk_o == 50 || csbk_o == 53) //48, 49, 50 are voice grants, 51 and 52 are data grants, 53 Duplex Private Voice, 54 Duplex Private Data
        {
          
          //if tg hold is specified and matches target, allow for a call pre-emption by nullifying the last vc sync time
          if (state->tg_hold != 0 && state->tg_hold == target)
            state->last_vc_sync_time = 0;

          //TIII tuner fix if voice assignment occurs to the control channel itself,
          //then it may not want to resume tuning due to no framesync loss after call ends
          if ( (time(NULL) - state->last_vc_sync_time > 2) ) 
          {
            opts->p25_is_tuned = 0;
            //zero out vc frequencies
            state->p25_vc_freq[0] = 0;
            state->p25_vc_freq[1] = 0;
          } 

          //shim in here for ncurses freq display when not trunking (playback, not live)
          if (opts->p25_trunk == 0 && freq != 0)
          {
            //just set to both for now, could go on tslot later
            state->p25_vc_freq[0] = freq;
            state->p25_vc_freq[1] = freq;
          }

          //don't tune if currently a vc on the control channel
          if ( (time(NULL) - state->last_vc_sync_time > 2) ) 
          {
            char mode[8]; //allow, block, digital, enc, etc
            sprintf (mode, "%s", "");

            //if we are using allow/whitelist mode, then write 'B' to mode for block
            //comparison below will look for an 'A' to write to mode if it is allowed
            if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

            for (int i = 0; i < state->group_tally; i++)
            {
              if (state->group_array[i].groupNumber == target)
              {
                fprintf (stderr, " [%s]", state->group_array[i].groupName);
                strcpy (mode, state->group_array[i].groupMode);
                break;
              }
            }

            //TG hold on DMR T3 Systems -- block non-matching target, allow matching target
            if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
            if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");

            if (state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "B") != 0) && (strcmp(mode, "DE") != 0)) 
            {
              if (freq != 0) //if we have a valid frequency
              {
                //RIGCTL
                if (opts->use_rigctl == 1)
                {
                  //we will want to set these values here, some Tier 3 systems prefer P_Protects (Tait) over VLC and TLC headers
                  //and is faster than waiting on good embedded link control
                  if (lcn == 0 && data_call == 0)
                  {
                    state->lasttg = target;
                    state->lastsrc = source;

                    //fix call string for per call, etc
                    sprintf (state->call_string[0], " Trunked "); //catch all
                    if (csbk_o == 49 || csbk_o == 50)
                      sprintf (state->call_string[0], "   Group ");
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                    else sprintf (state->call_string[0], " Private ");
                    if (st2)
                    {
                      if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                      else strcat (state->call_string[0], " Emergency  ");
                    }
                    else strcat (state->call_string[0], "            ");

                  }

                  if (lcn == 1 && data_call == 0)
                  {
                    state->lasttgR = target;
                    state->lastsrcR = source;

                    //fix call string for per call, etc
                    sprintf (state->call_string[1], " Trunked "); //catch all
                    if (csbk_o == 49 || csbk_o == 50)
                      sprintf (state->call_string[1], "   Group ");
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                    else sprintf (state->call_string[1], " Private ");
                    if (st2)
                    {
                      if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                      else strcat (state->call_string[1], " Emergency  ");
                    }
                    else strcat (state->call_string[1], "            ");

                  }
                  //Guess I forgot to add this condition here
                  if (GetCurrentFreq(opts->rigctl_sockfd) != freq)
                    dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency
                  if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                  if (GetCurrentFreq(opts->rigctl_sockfd) != freq)
                    SetFreq(opts->rigctl_sockfd, freq);
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
                  opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                  state->last_vc_sync_time = time(NULL);
                  state->last_t3_tune_time = time(NULL); //set here so a random p_clear on the opposite slot doesn't send us back to the CC
                  
                }

                //rtl
                else if (opts->audio_in_type == 3)
                {
                  #ifdef USE_RTLSDR
                  //we will want to set these values here, some Tier 3 systems prefer P_Protects (Tait) over VLC and TLC headers
                  //and is faster than waiting on good embedded link control
                  if (lcn == 0 && data_call == 0)
                  {
                    state->lasttg = target;
                    state->lastsrc = source;

                    //fix call string for per call, etc
                    sprintf (state->call_string[0], " Trunked "); //catch all
                    if (csbk_o == 49 || csbk_o == 50)
                      sprintf (state->call_string[0], "   Group ");
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                    else sprintf (state->call_string[0], " Private ");
                    if (st2)
                    {
                      if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                      else strcat (state->call_string[0], " Emergency  ");
                    }
                    else strcat (state->call_string[0], "            ");

                  }
                  if (lcn == 1 && data_call == 0)
                  {
                    state->lasttgR = target;
                    state->lastsrcR = source;

                    //fix call string for per call, etc
                    sprintf (state->call_string[1], " Trunked "); //catch all
                    if (csbk_o == 49 || csbk_o == 50)
                      sprintf (state->call_string[1], "   Group ");
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                    else sprintf (state->call_string[1], " Private ");
                    if (st2)
                    {
                      if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54 || csbk_o == 55 || csbk_o == 56) {} //do nothing
                      else strcat (state->call_string[1], " Emergency  ");
                    }
                    else strcat (state->call_string[1], "            ");

                  }
                  //Guess I forgot to add this condition here
                  uint32_t tempf = (uint32_t)freq;
                  if (opts->rtlsdr_center_freq != tempf)
                    dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency
                  if (opts->rtlsdr_center_freq != tempf)
                    rtl_dev_tune (opts, freq);
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
                  opts->p25_is_tuned = 1;
                  state->last_vc_sync_time = time(NULL);
                  state->last_t3_tune_time = time(NULL); //set here so a random p_clear on the opposite slot doesn't send us back to the CC

                  #endif
                }

              }
            }
          }

        }

        SKIPCALL: ; //do nothing
        
      }

      //Move
      if (csbk_o == 57)
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Move (C_MOVE) ");
      } 

      //Aloha 
      if (csbk_o == 25)
      {
        //initial line break
        fprintf (stderr, "\n");

        dmr_decode_syscode(opts, state, cs_pdu_bits, csbk_fid, 0);

        //if using rigctl we can set an unknown or updated cc frequency 
        //by polling rigctl for the current frequency
        if (opts->use_rigctl == 1 && opts->p25_is_tuned == 0) //&& state->p25_cc_freq == 0
        {
          ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
          if (ccfreq != 0) state->p25_cc_freq = ccfreq;
        }

        //if using rtl input, we can ask for the current frequency tuned
        if (opts->audio_in_type == 3 && opts->p25_is_tuned == 0) //&& state->p25_cc_freq == 0
        {
          ccfreq = (long int)opts->rtlsdr_center_freq;
          if (ccfreq != 0) state->p25_cc_freq = ccfreq;
        }
        
      } 

      //P_CLEAR
      if (csbk_o == 46)
      {

        //NOTE: Do not zero out lasttg/lastsrc in TLC FLCO when using p_clear, otherwise,
        //it will affect the conditions below and fail to trigger on tg_hold

        //test misc conditions to trigger a clear and immediately return to CC, or remain on current VC;
        int clear = 0;
        int pslot = state->currentslot; //this slot
        int oslot = (state->currentslot ^ 1) & 1; //opposite slot
        pslot++; oslot++; //set to 1 and 2 and not 0 and 1

        //if the other slot is IDLE or TLC condition -- May consider disabling this one
        // if (state->currentslot == 0 && (state->dmrburstR == 9 && state->dmrburstR == 7)) clear = 1;
        // if (state->currentslot == 0 && (state->dmrburstL == 9 && state->dmrburstL == 7)) clear = 1;

        //if no voice header, pi header, or voice sync in the opposite slot currently
        if (state->currentslot == 0 && (state->dmrburstR != 16 && state->dmrburstR != 0 && state->dmrburstR != 1)) clear = 2;
        if (state->currentslot == 1 && (state->dmrburstL != 16 && state->dmrburstL != 0 && state->dmrburstL != 1)) clear = 3;

        //if tuning and decoding data is desired, then do a secondary check here for data headers and blocks in the opposite slot
        if (opts->trunk_tune_data_calls == 1)
        {
          //if no data header or data block sync in the opposite slot currently
          if (state->currentslot == 0 && (state->dmrburstR == 6 || state->dmrburstR == 7 || state->dmrburstR == 8 || state->dmrburstR == 10)) clear = 21;
          if (state->currentslot == 1 && (state->dmrburstL == 6 || state->dmrburstL == 7 || state->dmrburstL == 8 || state->dmrburstL == 10)) clear = 22;
        }

        //if we have a tg hold in place that matches traffic that was just on this slot (this will override other calls in the opposite slot)
        if (state->currentslot == 0 && state->tg_hold == state->lasttg && state->tg_hold != 0)  clear = 4;
        if (state->currentslot == 1 && state->tg_hold == state->lasttgR && state->tg_hold != 0) clear = 5;

        //make sure we aren't sent back immediately by an p_clear condition upon first tuning (i.e., random ENC LO, or end of Data Call)
        //one second default on hangtime should be an optimal time, but let it be user configurable with hangtime option
        if ( time(NULL)-state->last_t3_tune_time < opts->trunk_hangtime) clear = 0;

        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Clear (P_CLEAR) ");
        #ifdef PCLEAR_TUNE_AWAY

        if (opts->p25_trunk == 1)
        {

          //check the p_clear logic and report status
          if      (clear && csbk_fid == 255)  fprintf (stderr, " Slot %d No Encrypted Call Trunking; Slot %d Free; Return to CC; ", pslot, oslot);
          else if (!clear && csbk_fid == 255) fprintf (stderr, " Slot %d No Encrypted Call Trunking; Slot %d Busy; Remain on VC;", pslot, oslot);
          else if (clear && csbk_fid == 254)  fprintf (stderr, " Cap+ Rest LSN Change: %d; Slot %d Free; Slot %d Free; Go To Rest LSN;", state->dmr_rest_channel, pslot, oslot); //disabled
          else if (!clear && csbk_fid == 254) fprintf (stderr, " Cap+ Rest LSN Change: %d; Slot %d Free; Slot %d Busy; Remain on LSN;", state->dmr_rest_channel, pslot, oslot); //disabled
          else if (clear && csbk_fid == 253)  fprintf (stderr, " Cap+ Rest LSN Change: %d; No CSBK Channel Activity; Go To Rest LSN;", state->dmr_rest_channel);
          else if (!clear && csbk_fid == 253) fprintf (stderr, " Cap+ Rest LSN Change: %d; CSBK Channel Activity; Remain on LSN;", state->dmr_rest_channel); //this should never happen in code
          else if (!clear && csbk_fid == 12)  fprintf (stderr, " Con+ Slot %d Termination: Slot %d Busy Voice or Data Call;", pslot, oslot); //Con+ test clears based on the Call Termination CSBK
          else if (clear && csbk_fid == 12)   fprintf (stderr, " Con+ Slot %d Termination: Slot %d Clear or Control CSBK;", pslot, oslot); //Con+ test clears based on the Call Termination CSBK
          else if (!clear)                    fprintf (stderr, " Slot %d Clear; Slot %d Busy; Remain on VC;", pslot, oslot);
          else if (clear == 1)                fprintf (stderr, " Slot %d Clear; Slot %d Idle; Return to CC;", pslot, oslot);
          else if (clear == 2 || clear == 3)  fprintf (stderr, " Slot %d Clear; Slot %d Free; Return to CC;", pslot, oslot);
          else if (clear == 4 || clear == 5)  fprintf (stderr, " Slot %d Clear w/ TG Hold %d; Slot %d Activity Override; Return to CC; ", pslot, state->tg_hold, oslot);
          //NOTE: Below clears are just conditions for reporting, and not clear to tune away, so they are set back to zero
          else if (clear == 21 || clear == 22)
          {
            fprintf (stderr, " Slot %d Clear; Slot %d Data; Remain on DC;", pslot, oslot);
            clear = 0; //flag as 0 so we won't tune away until data call is completed
          }

          if (clear)
          {
            if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && opts->p25_is_tuned == 1)
            {

              //display/le/buzzer bug fix when p_clear activated (unsure why this was disabled)
              //clear only the current slot initially, then clear both if tuning to a different freq
              if (state->currentslot == 0 && csbk_fid != 253) //don't reset on Cap+ since we aren't testing based on the current TS 
              {
                state->payload_mi = 0;
                state->payload_algid = 0;
                state->payload_keyid = 0;
                state->dmr_so = 0;
              }
              if (state->currentslot == 1 && csbk_fid != 253)
              {
                state->payload_miR = 0;
                state->payload_algidR = 0;
                state->payload_keyidR = 0;
                state->dmr_soR = 0;
              }
              
              //rigctl
              if (opts->use_rigctl == 1)
              {
                //Guess I forgot to add this condition here
                if (GetCurrentFreq(opts->rigctl_sockfd) != state->p25_cc_freq)
                  dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency

                //clear both if tuning away to another frequency
                if (GetCurrentFreq(opts->rigctl_sockfd) != state->p25_cc_freq)
                {
                  state->payload_mi = 0;
                  state->payload_algid = 0;
                  state->payload_keyid = 0;
                  state->dmr_so = 0;

                  state->payload_miR = 0;
                  state->payload_algidR = 0;
                  state->payload_keyidR = 0;
                  state->dmr_soR = 0;
                }

                //reset some strings
                sprintf (state->call_string[state->currentslot], "%s", "                     "); //21 spaces
                sprintf (state->active_channel[state->currentslot], "%s", "");
                state->last_vc_sync_time = 0;
                state->last_cc_sync_time = time(NULL);
                opts->p25_is_tuned = 0;
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
                if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                if (GetCurrentFreq(opts->rigctl_sockfd) != state->p25_cc_freq)
                  SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);        
              }

              //rtl
              else if (opts->audio_in_type == 3)
              {
                #ifdef USE_RTLSDR
                //Guess I forgot to add this condition here
                uint32_t tempf = (uint32_t)state->p25_cc_freq;
                if (opts->rtlsdr_center_freq != tempf)
                  dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency

                //clear both if tuning away to another frequency
                if (opts->rtlsdr_center_freq != tempf)
                {
                  state->payload_mi = 0;
                  state->payload_algid = 0;
                  state->payload_keyid = 0;
                  state->dmr_so = 0;

                  state->payload_miR = 0;
                  state->payload_algidR = 0;
                  state->payload_keyidR = 0;
                  state->dmr_soR = 0;
                }

                //reset some strings
                sprintf (state->call_string[state->currentslot], "%s", "                     "); //21 spaces
                sprintf (state->active_channel[state->currentslot], "%s", "");
                state->last_cc_sync_time = time(NULL);
                state->last_vc_sync_time = 0;
                opts->p25_is_tuned = 0;
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
                if (opts->rtlsdr_center_freq != tempf)
                  rtl_dev_tune (opts, state->p25_cc_freq);
                #endif
              }

            }
          }
        } //end if trunking is enabled
        #else
        UNUSED (clear); UNUSED(pslot); UNUSED(oslot);
        #endif //end if PCLEAR_TUNE_AWAY is enabled in code 
      } 

      //(P_PROTECT)
      if (csbk_o == 47)
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Protect (P_PROTECT) -");
        uint16_t reserved = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 12);
        uint8_t p_kind = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[28], 3);
        uint8_t gi = cs_pdu_bits[31];
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        UNUSED(reserved);

        if (gi)  fprintf (stderr, " Group");
        if (!gi) fprintf (stderr, " Private");

        if (p_kind == 0) fprintf (stderr, " Disable Target PTT (DIS_PTT)");
        if (p_kind == 1) fprintf (stderr, " Enable Target PTT (EN_PTT)");
        if (p_kind == 2) fprintf (stderr, " Call (ILLEGALLY_PARKED)");
        if (p_kind == 3) fprintf (stderr, " Enable Target MS PTT (EN_PTT_ONE_MS)");

        fprintf (stderr, "\n");
        fprintf (stderr, "  Source: %08d; Target: %08d; ", source, target);

        //check the source and/or target for special gateway identifiers
        dmr_gateway_identifier (source, target);

        //change this slot burst type to VLC so the revamped p_clear doesn't tune away 
        if (opts->p25_trunk == 1)
        {
          if (gi && opts->trunk_tune_group_calls == 1)
          {
            if (state->currentslot == 0) state->dmrburstL = 1;
            else state->dmrburstR = 1;
          }
          if (!gi && opts->trunk_tune_private_calls == 1)
          {
            if (state->currentslot == 0) state->dmrburstL = 1;
            else state->dmrburstR = 1;
          }
        }

      }

      if (csbk_o == 40) //0x28
      {
        int i;
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Announcements (C_BCAST)");
        uint8_t a_type = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 5);
        if (a_type == 0) fprintf (stderr, " Announce/Withdraw TSCC (Ann_WD_TSCC)");
        if (a_type == 1) fprintf (stderr, " Specify Call Timer Parameters (CallTimer_Parms)");
        if (a_type == 2) fprintf (stderr, " Vote Now Advice (Vote_Now)");
        if (a_type == 3) fprintf (stderr, " Broadcast Local Time (Local_Time)");
        if (a_type == 4) fprintf (stderr, " Mass Registration (MassReg)");
        if (a_type == 5) fprintf (stderr, " Announce Logical Channel/Frequency Relationship (Chan_Freq)");
        if (a_type == 6) fprintf (stderr, " Adjacent Site Information (Adjacent_Site)");
        if (a_type == 7) fprintf (stderr, " General Site Parameters (Gen_Site_Params)");
        if (a_type > 7 && a_type < 0x1E) fprintf (stderr, " Reserved: %02X", a_type);
        if (a_type == 0x1E || a_type == 0x1F) fprintf (stderr, " Manufacturer Specific: %02X", a_type);

        //parms1 start 21, len 14
        uint16_t bparms1 = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[21], 14);
        uint8_t bpbits1[14]; for (i = 0; i < 14; i++) bpbits1[i] = cs_pdu_bits[21+i];

        //registration required
        uint8_t reg_req = cs_pdu_bits[35];

        //backoff number
        uint8_t backoff = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[36], 4);

        //syscode start 40, len 16
        uint16_t syscode = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 14);
        
        //parms2 start 56, len 24
        uint32_t bparms2 = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        uint8_t bpbits2[24]; for (i = 0; i < 24; i++) bpbits2[i] = cs_pdu_bits[56+i];

        //MBC parms for when they will be needed
        uint8_t mbc_lb = cs_pdu_bits[96]; UNUSED(mbc_lb);
        uint8_t mbc_pf = cs_pdu_bits[97]; UNUSED(mbc_pf);
        uint8_t mbc_csbko = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[98], 6);
        uint8_t mbc_res = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[104], 4);
        uint8_t mbc_cc = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[108], 4);
        uint8_t mbc_cdeftype = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[112], 4);
        uint8_t mbc_res2 = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[116], 2);
        unsigned long long int mbc_cdefparms = (unsigned long long int)ConvertBitIntoBytes(&cs_pdu_bits[118], 58);
        uint16_t mbc_lpchannum = 0;
        uint16_t mbc_abs_tx_int = 0;
        uint16_t mbc_abs_tx_step = 0;
        uint16_t mbc_abs_rx_int = 0;
        uint16_t mbc_abs_rx_step = 0;
        long int freqt, freqr = 0;

        //common value among some a_types
        uint16_t a_channel = (uint16_t)ConvertBitIntoBytes(&bpbits2[12], 12);

        //Ann_WD_TSCC
        if (a_type == 0)
        {
          //Color Codes and Flags
          uint8_t ann_res = (uint8_t)ConvertBitIntoBytes(&bpbits1[0], 4);
          uint8_t cc_ch1 = (uint8_t)ConvertBitIntoBytes(&bpbits1[4], 4);
          uint8_t cc_ch2 = (uint8_t)ConvertBitIntoBytes(&bpbits1[8], 4);
          uint8_t ch1_flag = bpbits1[13];
          uint8_t ch2_flag = bpbits1[14];

          uint16_t bcast_ch1 = (uint16_t)ConvertBitIntoBytes(&bpbits2[0], 12);
          uint16_t bcast_ch2 = (uint16_t)ConvertBitIntoBytes(&bpbits2[12], 12);

          fprintf (stderr, "\n");
          if (ann_res) fprintf (stderr, " Res: %X;", ann_res);
          fprintf (stderr, " LPCN CH1: %d; CC: %d;", bcast_ch1, cc_ch1);
          if (ch1_flag == 0) fprintf (stderr, " Add;");
          if (ch1_flag == 1) fprintf (stderr, " Remove;");

          fprintf (stderr, " LPCN CH2: %d; CC: %d;", bcast_ch2, cc_ch2);
          if (ch2_flag == 0) fprintf (stderr, " Add;");
          if (ch2_flag == 1) fprintf (stderr, " Remove;");
        }

        //CallTimer_parms
        if (a_type == 1)
        {
          //7.9.19.2 Table 7.70
          uint16_t t_emerg_timer = (uint16_t)ConvertBitIntoBytes(&bpbits1[0], 9);
          uint8_t t_packet_timer = (uint8_t)ConvertBitIntoBytes(&bpbits1[9], 5);

          uint16_t t_msms_timer = (uint16_t)ConvertBitIntoBytes(&bpbits1[0], 9);
          uint16_t t_msline_timer = (uint16_t)ConvertBitIntoBytes(&bpbits1[0], 9);

          //just doing the raw values here, and not the decoded values, see clause A.1, Tables A.2, A.3, A.4, A.5
          fprintf (stderr, "\n");
          fprintf (stderr, " Timers - Emergency: %d; Packet: %d; MS-MS: %d; Line: %d; ", t_emerg_timer, t_packet_timer, t_msms_timer, t_msline_timer);
        }

        //Local_Time
        if (a_type == 3)
        {
          uint8_t lt_day = (uint8_t)ConvertBitIntoBytes(&bpbits1[0], 5); //1-31
          uint8_t lt_mon = (uint8_t)ConvertBitIntoBytes(&bpbits1[5], 4); //1-12, or 0 if not broadcast
          uint8_t lt_off = (uint8_t)ConvertBitIntoBytes(&bpbits1[9], 4); //0-14 hours, or 15 if not broadcast
          uint8_t lt_off_sign = bpbits1[13]; //0 is positive offset, 1 is negative offset
          
          uint8_t lt_hour = (uint8_t)ConvertBitIntoBytes(&bpbits2[0], 5); //0-23
          uint8_t lt_mins = (uint8_t)ConvertBitIntoBytes(&bpbits2[5], 6); //0-59
          uint8_t lt_secs = (uint8_t)ConvertBitIntoBytes(&bpbits2[11], 6); //0-59
          uint8_t lt_dofw = (uint8_t)ConvertBitIntoBytes(&bpbits2[17], 3); //day of week
          uint8_t lt_off_fr = (uint8_t)ConvertBitIntoBytes(&bpbits2[20], 2); //0 = 0; 1 = +15 mins; 2 = +30 mins; 3 = +45 mins;
          uint8_t lt_res = (uint8_t)ConvertBitIntoBytes(&bpbits2[22], 2);

          int localhour = lt_hour;
          int localmin  = lt_mins;
          int offset = lt_off;
          if (lt_off_sign) offset *= -1;

          //I wonder if local offset will require rollover or rollunder
          if (lt_off_sign == 1) localhour = lt_hour - lt_off;
          if (lt_off_sign == 0) localhour = lt_hour + lt_off;
          
          if (lt_off_fr == 1) localmin += 15;
          if (lt_off_fr == 2) localmin += 30;
          if (lt_off_fr == 3) localmin += 45;

          fprintf (stderr, "\n");
          if (lt_mon != 0 && lt_day != 0) 
            fprintf (stderr, " Date: %d.%d;", lt_mon, lt_day);
          //day of the week, 1 is Sunday, 7 is Saturday, 0 not broadcasted
          if (lt_dofw == 1) fprintf (stderr, " Sunday;");
          if (lt_dofw == 2) fprintf (stderr, " Monday;");
          if (lt_dofw == 3) fprintf (stderr, " Tuesday;");
          if (lt_dofw == 4) fprintf (stderr, " Wednesday;");
          if (lt_dofw == 5) fprintf (stderr, " Thursday;");
          if (lt_dofw == 6) fprintf (stderr, " Friday;");
          if (lt_dofw == 7) fprintf (stderr, " Saturday;");
          fprintf (stderr, " UTC Time: %02d:%02d:%02d;", lt_hour, lt_mins, lt_secs);
          if (lt_off != 15) fprintf (stderr, " Local: %02d:%02d:%02d;", localhour, localmin, lt_secs);
          if (lt_off != 15) fprintf (stderr, " Offset: %d;", offset);
          if (lt_res) fprintf (stderr, " Res: %d;", lt_res);

        }

        //Chan_Freq
        if (a_type == 5)
        {
          // if (a_channel != 0xFFF) fprintf (stderr, " LPCN: %d;", a_channel);
          if (a_channel == 0) fprintf (stderr, " LPCN: Null;");
          if (a_channel != 0)
          {
            if (mbc_cdeftype == 0) //if 0, then absolute channel parms
            {
              mbc_lpchannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[118], 12);
              mbc_abs_tx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[130], 10);
              mbc_abs_tx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[140], 13);
              mbc_abs_rx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[153], 10);
              mbc_abs_rx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[163], 13);
              freqr = (mbc_abs_rx_int * 1000000 ) + (mbc_abs_rx_step * 125);
              freqt = (mbc_abs_tx_int * 1000000 ) + (mbc_abs_tx_step * 125);

              if (a_channel == 0xFFF) fprintf (stderr, "\n APCN: %04d;", mbc_lpchannum); //absolute physical channel number
              else if (a_channel != 0) fprintf (stderr, "\n LPCN: %04d;", mbc_lpchannum); //logical physical channel number
              fprintf (stderr, " RX Base: %d; RX Step: %d; RX Freq: %ld;", mbc_abs_rx_int * 1000000, mbc_abs_rx_step * 125, freqr);
              fprintf (stderr, "\n            "); //12 spaces
              fprintf (stderr, " TX Base: %d; TX Step: %d; TX Freq: %ld;", mbc_abs_tx_int * 1000000, mbc_abs_tx_step * 125, freqt);

              //experimental -- assign a_channel or mbc_lpchannum and freqr to channel map if not available
              if (a_channel != 0 && a_channel != 0xFFF && freqr != 0)
              {
                if (state->trunk_chan_map[a_channel] == 0)
                {
                  state->trunk_chan_map[a_channel] = freqr;
                  //add to rotation for CC Hunting on extended noframesync
                  state->trunk_lcn_freq[state->lcn_freq_count++%25] = freqr; //no not exceed 25 entries
                  if (state->lcn_freq_count > 25) state->lcn_freq_count = 25;
                }
              }

              //this one probably doesn't matter, I think that both values have the same channel number (a_channel and mbc_lpchannum)
              //and also since absolute channel grants will also have these values available to figure out frequency to tune to
              // if (a_channel == 0xFFF && freqr != 0)
              // {
              //   if (state->trunk_chan_map[mbc_lpchannum] == 0 && mbc_lpchannum != 0xFFFF && mbc_lpchannum != 0)
              //   {
              //     state->trunk_chan_map[mbc_lpchannum] = freqr;
              //     //add to rotation for CC Hunting on extended noframesync
              //     state->trunk_lcn_freq[state->lcn_freq_count++%25] = freqr; //no not exceed 25 entries
              //     if (state->lcn_freq_count > 25) state->lcn_freq_count = 25;
              //   }
              // }
              
            }
            else 
            {
              fprintf (stderr, "\n Unknown CDEFType: %X; CDEFParms: %015llX", mbc_cdeftype, mbc_cdefparms);
              fprintf (stderr, " MBC Op: %02X;", mbc_csbko);
              // fprintf (stderr, " CC: %d;", mbc_cc); //not on chan_freq
              fprintf (stderr, " RES1: %X;", mbc_res);
              fprintf (stderr, " RES2: %X;", mbc_res2);
            }
          }
        }

        //Vote Now, Adjacent Site, 
        if (a_type == 2 || a_type == 6)
        {
          uint8_t active_ava = bpbits2[0];
          uint8_t active_con = bpbits2[1];
          uint8_t c_chan_pri = (uint8_t)ConvertBitIntoBytes(&bpbits2[2], 3);
          uint8_t a_chan_pri = (uint8_t)ConvertBitIntoBytes(&bpbits2[5], 3);
          uint8_t a_reserved = (uint8_t)ConvertBitIntoBytes(&bpbits2[8], 4);

          fprintf (stderr, "\n");
          dmr_decode_syscode(opts, state, cs_pdu_bits, csbk_fid, 1);

          // fprintf (stderr, "\n");
          if (active_ava == 1) ; //fprintf (stderr, " Active Connection Information Available;");
          else fprintf (stderr, " Active Connection Information Not Available;");

          if (active_ava == 1)
          {
            // if (active_con == 1) fprintf (stderr, " Connection Active;");
            if (active_con == 1)
              fprintf (stderr, " Online;");
            else fprintf (stderr, " Offline;");
            // fprintf (stderr, " Confirmed Channel Priority: %d;", c_chan_pri);
            // fprintf (stderr, " Active Channel Priority: %d;", a_chan_pri);
            fprintf (stderr, " CC Pri: %d;", c_chan_pri);
            fprintf (stderr, " AC Pri: %d;", a_chan_pri);
            if (a_reserved) fprintf (stderr, " Res: %X;", a_reserved);
            if (a_channel != 0xFFF && a_channel != 0) fprintf (stderr, " LPCN: %d;", a_channel);
            if (a_channel == 0) fprintf (stderr, " LPCN: Null;");
            if (a_channel == 0xFFF)
            {
              if (mbc_cdeftype == 0) //if 0, then absolute channel parms
              {
                mbc_lpchannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[118], 12);
                mbc_abs_tx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[130], 10);
                mbc_abs_tx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[140], 13);
                mbc_abs_rx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[153], 10);
                mbc_abs_rx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[163], 13);
                freqr = (mbc_abs_rx_int * 1000000 ) + (mbc_abs_rx_step * 125);
                freqt = (mbc_abs_tx_int * 1000000 ) + (mbc_abs_tx_step * 125);

                fprintf (stderr, "\n APCN: %04d;", mbc_lpchannum); //absolute physical channel number
                fprintf (stderr, " RX Base: %d; RX Step: %d; RX Freq: %ld;", mbc_abs_rx_int * 1000000, mbc_abs_rx_step * 125, freqr);
                fprintf (stderr, "\n            "); //12 spaces
                fprintf (stderr, " TX Base: %d; TX Step: %d; TX Freq: %ld;", mbc_abs_tx_int * 1000000, mbc_abs_tx_step * 125, freqt);
                
              }
              else 
              {
                fprintf (stderr, "\n Unknown CDEFType: %X; CDEFParms: %015llX", mbc_cdeftype, mbc_cdefparms);
                fprintf (stderr, " MBC Op: %02X;", mbc_csbko);
                if (a_type == 2)
                  fprintf (stderr, " CC: %d;", mbc_cc);
                fprintf (stderr, " RES1: %X;", mbc_res);
                fprintf (stderr, " RES2: %X;", mbc_res2);
              }
            }
          }

        }

        //Gen_Site_Parms
        if (a_type == 7)
        {
          //parms1 are all reserved values

          //current (confirmed site information)
          uint8_t csi = (uint8_t)ConvertBitIntoBytes(&bpbits1[0], 8); //see clause 7.2.40
          //Network Information
          uint8_t nin = (uint8_t)ConvertBitIntoBytes(&bpbits2[16], 8); //see clause 7.2.41

          //the second bit of the CSI, the rest are reserved (wasteful)
          uint8_t hibernate_flag = bpbits1[1];

          //the first bit of the nin, the rest are reserved (yet again)
          uint8_t reg_tg_sub = bpbits2[0]; //if the MS has to send TG during Registration Process (Zzzzzz)

          fprintf (stderr, "\n");
          fprintf (stderr, " Hibernate Flag: %d; Reg Flag: %d; RES1: %d; RES2: %X; RES3: %X; BPARMS1: %X", hibernate_flag, reg_tg_sub, bpbits1[0], csi & 0x3F, nin & 0x7F, bparms1);
        }

        //MassReg
        if (a_type == 4)
        {
          uint8_t reg_window = (uint8_t)ConvertBitIntoBytes(&bpbits1[5], 4); //7.2.19.5.1 Table 7.77 convert raw to seconds
          uint8_t aloha_mask = (uint8_t)ConvertBitIntoBytes(&bpbits1[9], 5);
          //ADDRNull or MS Individual Address
          uint8_t reg_address = (uint8_t)ConvertBitIntoBytes(&bpbits2[16], 8); //see clause 7.2.41

          fprintf (stderr, "\n"); //raw value only on the reg window, see aforementioned table
          fprintf (stderr, " Reg Window: %X; Aloha Mask: %02X; Target: %d; ", reg_window, aloha_mask, reg_address);
          dmr_gateway_identifier(0,reg_address);
        }

        //debug
        if (opts->payload == 1)
        {
          fprintf (stderr, "\n ");
          fprintf (stderr, " SYS: %04X;", syscode);
          fprintf (stderr, " Reg: %d;", reg_req);
          fprintf (stderr, " Backoff: %X;", backoff); //7.2.5, Table 7.39
          fprintf (stderr, " BParms1: %04X;", bparms1);
          fprintf (stderr, " BParms2: %06X;", bparms2);
          if (mbc_cdefparms != 0) //if this isn't an MBC block, then these will all be zeroes
          {
            fprintf (stderr, "\n ");
            fprintf (stderr, " MBC Op: %02X;", mbc_csbko);
            if (a_type == 2)
              fprintf (stderr, " CC: %d;", mbc_cc);
            fprintf (stderr, " RES1: %X;", mbc_res);
            fprintf (stderr, " RES2: %X;", mbc_res2);
            fprintf (stderr, " CDEFTYPE: %X;", mbc_cdeftype);
            fprintf (stderr, " CDEFPARMS: %015llX;", mbc_cdefparms);
          }
        }

      }

      if (csbk_o == 28) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_AHOY - ");
        //C_AHOY is always a single block CSBK, no need to check or deal with continuation blocks
        uint16_t svc_opt = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 7);
        uint8_t svc_flag = cs_pdu_bits[23]; //service kind flag
        uint8_t als_flag = cs_pdu_bits[24]; //ambient listening service requested
        uint8_t ahoy_gi  = cs_pdu_bits[25]; //group or individual
        uint8_t ahoy_bf  = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[26], 2); //UDT blocks to follow (if required)
        uint8_t svc_kind = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[28], 4); //'Call' Type
        uint32_t ahoy_target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t ahoy_source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

        UNUSED(ahoy_bf);
        UNUSED(svc_flag);
        UNUSED(als_flag);

        if (ahoy_gi == 0) fprintf (stderr, "Private ");
        else fprintf (stderr, "Group ");

        //need to put SVC OPT decoding in here, maybe just copy and paste from FLC?

        fprintf (stderr, "FID: %02X SVC: %02X ", csbk_fid, svc_opt);
        if (svc_kind == 0 || svc_kind == 1) fprintf (stderr, "Voice Call ");
        else if (svc_kind == 2 || svc_kind == 3) fprintf (stderr, "Packet Data Call ");
        else if (svc_kind == 4 || svc_kind == 5) fprintf (stderr, "UDT Short Data Call ");
        else if (svc_kind == 6) fprintf (stderr, "UDT Short Data Polling Service ");
        else if (svc_kind == 7) fprintf (stderr, "Status Transport Service ");
        else if (svc_kind == 8) fprintf (stderr, "Call Diversion Service ");
        else if (svc_kind == 9) fprintf (stderr, "Call Answer Service ");
        else if (svc_kind == 10) fprintf (stderr, "Full Duplex Voice Call ");
        else if (svc_kind == 11) fprintf (stderr, "Full Duplex Packet Data Call ");
        else if (svc_kind == 12) fprintf (stderr, "Reserved ");
        else if (svc_kind == 13) fprintf (stderr, "Supplimentary Service (Stun/Revive/Kill/Auth): ");
        else if (svc_kind == 14) fprintf (stderr, "Registration/Authentication ");
        else if (svc_kind == 15) fprintf (stderr, "Cancel Call Service ");

        fprintf (stderr, "Target: %d; Source: %d; ", ahoy_target, ahoy_source);

        //check the source and/or target for special gateway identifiers
        dmr_gateway_identifier (ahoy_source, ahoy_target);

      }

      if (csbk_o == 42) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " P_MAINT -");

        //I can't recall ever seeing a p_maint use, always see a p_clear though
        uint16_t pm_res1 = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 12);
        uint8_t  pm_kind = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[28], 3);
        uint8_t  pm_res2 = cs_pdu_bits[31];
        uint32_t pm_target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24); //should be TSI (clear the call)
        uint32_t pm_source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

        if (pm_kind == 0) fprintf (stderr, "Disconnect; ");
        else fprintf (stderr, " Res Kind: %02X", pm_kind);

        if (pm_res1) fprintf (stderr, "Res A: %03X", pm_res1);
        if (pm_res2) fprintf (stderr, "Res B: 1");

        fprintf (stderr, "Target: %d; Source: %d; ", pm_target, pm_source);

        //check the source and/or target for special gateway identifiers
        dmr_gateway_identifier (pm_source, pm_target);
      }

      if (csbk_o == 30) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_ACKVIT (Ackvitation/Authorization) ");
      }

      if (csbk_fid != 0x10) //if Not Motorola
      {
        //These opcodes to exist on CapMax as well, but don't function the same
        if (csbk_o == 32 || csbk_o == 33 || csbk_o == 34 || csbk_o == 35) 
        {
          //initial line break
          fprintf (stderr, "\n"); //(Acknowledgement)
          if (csbk_o == 32) fprintf (stderr, " C_ACKD Outbound TSCC; ");
          if (csbk_o == 33) fprintf (stderr, " C_ACKU Inbound TSCC; ");
          if (csbk_o == 34) fprintf (stderr, " P_ACKD Outbound Payload; ");
          if (csbk_o == 35) fprintf (stderr, " P_ACKU Inbound Payload; ");

          uint8_t response_info = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 7);
          uint8_t reason_code = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[23], 8);
          uint8_t ack_res1 = cs_pdu_bits[31];

          uint32_t ack_target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
          uint32_t ack_source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

          //response_info and reason_code start to get really convoluted on decoding them
          //for each opcode, so I am just going to put the values out to the console, 
          //look at ETSI TS 102 361-4 V1.12.1 7.2.7 for more info

          fprintf (stderr, "Response: %02X; Reason: %02X; ", response_info, reason_code);
          if (ack_res1) fprintf (stderr, " Res: %d", ack_res1);
          fprintf (stderr, "Target: %d; Source: %d; ", ack_target, ack_source);

          //check the source and/or target for special gateway identifiers
          dmr_gateway_identifier (ack_source, ack_target);

        }
      }

      if (csbk_o == 31) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_RAND ");
      }

      //tier 2 csbks
      if (csbk_o == 4)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Unit to Unit Voice Service Request (UU_V_Req) - ");

        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        fprintf (stderr, "Target [%d] - Source [%d] ", target, source);
      }

      if (csbk_o == 5)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Unit to Unit Voice Service Answer Response (UU_Ans_Req) - ");

        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        fprintf (stderr, "Target [%d] - Source [%d] ", target, source);

      }

      if (csbk_o == 7)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Channel Timing CSBK (CT_CSBK) ");
      }

      if (csbk_o == 38)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Negative Acknowledgement Response (NACK_Rsp) - ");

        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        fprintf (stderr, "Target [%d] - Source [%d] ", target, source);

      }

      if (csbk_o == 56 && state->synctype == 33) //only run on MS Data sync pattern
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " BS Outbound Activation (BS_Dwn_Act) - ");
        //Inbound CSBK only from MS source to 'wake' the repeater up (best that I understand)
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        fprintf (stderr, "Target [%d] - Source [%d] ", target, source);
      }

      if (csbk_o == 61)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Preamble CSBK - ");
        uint8_t content = cs_pdu_bits[16];
        uint8_t gi = cs_pdu_bits[17];
        uint8_t res = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[18], 6);
        uint8_t blocks = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[24], 8);
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
        UNUSED2(res, blocks);

        uint8_t target_hash[24]; 
        uint8_t tg_hash = 0; 

        if (gi == 0) fprintf (stderr, "Individual ");
        else fprintf (stderr, "Group ");
        
        if (content == 0) fprintf (stderr, "CSBK - ");
        else fprintf (stderr, "Data - ");

        // check to see if this is XPT
        if (strcmp (state->dmr_branding_sub, "XPT ") == 0)
        {
          //get 16-bit truncated target and source ids
          target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 16); //40, or 32?
          source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[64], 16); //56, or 64?

          //check to see if this is indiv data (private) then we will need to report the hash value as well
          if (gi == 0)
          {
            //the crc8 hash is the value represented in the Hytera XPT Site Status CSBK when dealing with indiv data
            for (int i = 0; i < 16; i++) target_hash[i] = cs_pdu_bits[40+i];
            tg_hash = crc8 (target_hash, 16);
            fprintf (stderr, "Source: %d - Target: %d - Hash: %d ", source, target, tg_hash);
          }
          else fprintf (stderr, "Source: %d - Target: %d ", source, target);

        }
        // check to see if this is Cap+
        else if (strcmp (state->dmr_branding_sub, "Cap+ ") == 0)
        {
          //truncate tg on group? or just on private/individual data?
          if (gi == 0) target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 16);
          source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[64], 16);
          int rest = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[60], 4);
          fprintf (stderr, "Source: %d - Target: %d - Rest LSN: %d", source, target, rest);
        }
        else fprintf (stderr, "Source: %d - Target: %d ", source, target);

        //if trunking and data calls allowed, convert preamble burst to a DATA header burst to hold p_clear or con+
        if (opts->p25_trunk == 1 && opts->trunk_tune_data_calls == 1)
        {
          if (state->currentslot == 0) state->dmrburstL = 6;
          else state->dmrburstR = 6;
        }

      }
      //end tier 2 csbks

    }

    //Reworked Cap+ for Multi Block FL PDU and Private/Data Calls
    //experimental, but seems to be working well
    if (csbk_fid == 0x10)
    {

      //Cap+ Something
      if (csbk_o == 0x3A)
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);

        fprintf (stderr, " Capacity Plus CSBK 0x3A ");

        // 01:15:08 Sync: +DMR   slot1  [slot2] | Color Code=03 | CSBK
        // Capacity Plus Channel Status - FL: 3 TS: 1 RS: 0 - Rest Channel 1 - Single Block
        //   Ch1: Rest Ch2: Idle Ch3: Idle Ch4: Idle 
        //   Ch5: Idle Ch6: Idle Ch7: Idle Ch8: Idle 
        // DMR PDU Payload [BE][10][E1][00][00][00][00][00][00][00][4E][15]
        // 01:15:08 Sync: +DMR  [slot1]  slot2  | Color Code=03 | CSBK

        //FL, TS, and Rest Channel seem to be in same location as the Channel Status CSBK
        //other values are currently unknown
        // DMR PDU Payload [BA][10][C1][3B][61][11][51][00][00][00][3D][D6]
        // 01:15:08 Sync: +DMR   slot1  [slot2] | Color Code=03 | CSBK
        // DMR PDU Payload [BA][10][E1][3B][61][11][51][00][00][00][46][BE]

      }

      //Cap+ Adjacent Sites
      if (csbk_o == 0x3B)
      {

        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);

        fprintf (stderr, " Capacity Plus Adjacent Sites\n  ");

        uint8_t nl[6]; //adjacet site numerical value
        uint8_t nr[6]; //adjacent site current rest channel
        memset (nl, 0, sizeof (nl));
        memset (nr, 0, sizeof (nr));

        for (int i = 0; i < 6; i++)
        {
          nl[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[32+(i*8)], 4);
          nr[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[36+(i*8)], 4);
          if (nl[i]) fprintf (stderr, "Site: %d Rest: %d; ", nl[i], nr[i]);
        }

      }

      //Cap+ Channel Status -- Expanded for up to 16 LSNs and private voice/data calls and dual slot csbk_bit storage
      if (csbk_o == 0x3E)
      {

        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);

        uint8_t fl = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 2); 
        uint8_t ts = cs_pdu_bits[18];  //timeslot this PDU occurs in
        uint8_t res = cs_pdu_bits[19]; //unknown or unused bit value
        uint8_t rest_channel = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[20], 4); 
        uint8_t group_tally = 0; //set this to the number of active channels tallied
        uint8_t bank_one = 0;
        uint8_t bank_two = 0;
        uint8_t b2_start = 0;
        uint8_t block_num = state->cap_plus_block_num[ts];
        uint8_t pdflag = 0;
        uint8_t pdflag2 = 0;
        uint16_t private_target = 0;
        uint8_t pd_b2 = 0;
        uint8_t start = 8;
        uint8_t end = 8;
        uint8_t fl_bytes = 0;
        uint8_t ch[24]; //one bit per channel
        uint8_t pch[24]; //private or data call channel bits 
        uint16_t tg = 0;
        int i, j, k, x;
        //tg and channel info for trunking purposes
        uint16_t t_tg[24];
        char cap_active[20]; //local string to concantenate to active channel stuff
        uint8_t empty[24]; //used to evaluate whether or not all channels are idle or not with memcmp after loading

        //sanity check
        if (block_num > 6)
        {
          state->cap_plus_block_num[ts] = 6;
          block_num = 6;
        }

        //init some arrays
        memset (t_tg, 0, sizeof(t_tg));
        memset (ch, 0, sizeof(ch));
        memset (pch, 0, sizeof(pch));
        memset (empty, 0, sizeof(empty));

        //treating FL as a form of LCSS
        if (fl == 2 || fl == 3) //initial or single block (fl2 or fl3) 
        {
          //NOTE: this has been changed to store per slot
          memset (state->cap_plus_csbk_bits[ts], 0, sizeof(state->cap_plus_csbk_bits[ts]));
          for (i = 0; i < 10*8; i++) state->cap_plus_csbk_bits[ts][i] = cs_pdu_bits[i];
          state->cap_plus_block_num[ts] = 0;
        }
        else //appended block (fl 0) or final block (fl 1)
        {
          for (i = 0; i < 7*8; i++) state->cap_plus_csbk_bits[ts][i+80+(7*8*block_num)] = cs_pdu_bits[i+24];
          block_num++;
          state->cap_plus_block_num[ts]++;
        }
        
        //move assignment until later when evaluating for a p_clear condition
        if (rest_channel != state->dmr_rest_channel)
        {
          state->dmr_rest_channel = rest_channel; 
        }

        //assign to cc freq to follow during no sync
        if (state->trunk_chan_map[rest_channel] != 0)
        {
          // state->p25_cc_freq = state->trunk_chan_map[rest_channel];
          //set to always tuned
          opts->p25_is_tuned = 1;
        }

        fprintf (stderr, " Capacity Plus Channel Status - FL: %d TS: %d RS: %d - Rest LSN: %d", fl, ts, res, rest_channel);
        if (fl == 0) fprintf (stderr, " - Appended Block"); //have not yet observed a system use this fl value
        if (fl == 1) fprintf (stderr, " - Final Block"); 
        if (fl == 2) fprintf (stderr, " - Initial Block");
        if (fl == 3) fprintf (stderr, " - Single Block");

        //look at each group channel bit -- run this PRIOR to checking private or data calls
        //or else we will overwrite the ch bits we set below for private calls with zeroes
        //these also seem to indicate data calls -- can't find a distinction of which is which
        bank_one = (uint8_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][24], 8);
        for (int i = 0; i < 8; i++)
        {
          ch[i] = state->cap_plus_csbk_bits[ts][i+24];
          if (ch[i] == 1) group_tally++; //figure out where to start looking in the byte stream for the 0next bank of calls
        }

        //Expanded to cover larger systems up to 16 slots.
        bank_two = (uint8_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][32+(group_tally*8)], 8); //32
        b2_start = group_tally;
        if (bank_two)
        {
          for (int i = 0; i < 8; i++)
          {
            ch[i+8] = state->cap_plus_csbk_bits[ts][i+32+(b2_start*8)];
            if (ch[i+8] == 1) group_tally++; 
          }
        }

        //check flag for appended private and/or data calls
        pdflag = (uint8_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][40+(group_tally*8)], 8);

        //check for private activity on LSNs 1-8
        if (fl == 1 || fl == 3) 
        {
          if (pdflag) //== 0x80 //testing denny's 0x80 or 0x90 flag discovery
          {
            k = 0; //= 0
            fprintf (stderr, "\n");
            fprintf (stderr, " Bank One F%X Private or Data Call(s) - ", pdflag);
            for (int i = 0; i < 8; i++)
            {
              pch[i] = state->cap_plus_csbk_bits[ts][i+48+(group_tally*8)];
              if (pch[i] == 1)
              {
                fprintf (stderr, " LSN %02d:", i+1);
                private_target = (uint16_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][56+(k*16)+(group_tally*8)], 16);
                fprintf (stderr, " TGT %d;", private_target);
                k++;
                if (bank_one == 0) bank_one = 0xFF; //set all bits on so we can atleast parse all of them below in listing/display 
              } 
            }
            //save for starting point of the next private call bank
            pd_b2 = k;
          }
        }
        //end private/data call check on LSNs 1-8

        //check flag for appended private and/or data calls -- flag two still needs work or testing, double checking, etc, disable if issues arise
        pdflag2 = (uint8_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][56+(group_tally*8)+(pd_b2*16)], 8);  //48 -- had wrong value here (atleast in the one sample with the false positive)

        //check for private activity on LSNs 9-16
        if (fl == 1 || fl == 3) 
        {
          //then check to see if this byte has a value, should be 0x80, could be other?
          //this bytes location shifts depending on level of activity -- see banks above
          if (pdflag2) //== 0x80 //testing denny's 0x80 or 0x90 flag discovery
          {
            k = 0;
            fprintf (stderr, "\n");
            fprintf (stderr, " Bank Two F%02X Private or Data Call(s) - ", pdflag2);
            for (int i = 0; i < 8; i++)
            {
              pch[i+8] = state->cap_plus_csbk_bits[ts][i+64+(group_tally*8)+(pd_b2*16)]; //56
              if (pch[i+8] == 1)
              {
                fprintf (stderr, " LSN %02d:", i+1);
                private_target = (uint16_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][64+(k*16)+(group_tally*8)+(pd_b2*16)], 16); //56 -- had wrong value here (atleast in the one sample with the false positive)
                fprintf (stderr, " TGT %d;", private_target);
                k++;
                if (bank_two == 0) bank_two = 0xFF; //set all bits on so we can atleast parse all of them below in listing/display 
              } 
            }
          }
        }
        //end private/data call check on LSNs 9-16

        if (fl == 1 || fl == 3)
        {
          fprintf (stderr, "\n  ");

          //additive strings for active channels
          memset (state->active_channel, 0, sizeof(state->active_channel));
          sprintf (state->active_channel[0], "Cap+ ");
          state->last_active_time = time(NULL);
          
          k = 0;
          x = 0;

          //NOTE: New start and end positions will allow the status print
          //to contract and expand as needed to prevent this from
          //being a four liner CSBK when there is little or no activity

          //start position;
          start = 0; 
          if (bank_one & 0xF0) start = 0;
          else if (rest_channel < 5) start = 0;

          else if (bank_one & 0xF) start = 4;
          else if (rest_channel > 4 && rest_channel < 9) start = 4;

          else if (bank_two & 0xF0) start = 8;
          else if (rest_channel > 8 && rest_channel < 13) start = 8;

          else if (bank_two & 0xF) start = 12;
          else if (rest_channel > 12) start = 12;

          //end position;
          end = 16;
          if (bank_two & 0xF) end = 16;
          else if (rest_channel > 12) end = 16;

          else if (bank_two & 0xF0) end = 12;
          else if (rest_channel > 9 && rest_channel < 13) end = 12;

          else if (bank_one & 0xF) end = 8;
          else if (rest_channel > 4 && rest_channel < 9) end = 8;

          else if (bank_one & 0xF0) end = 4;
          else if (rest_channel < 5) end = 4;

          //start parsing info and listing active LSNs
          for (i = start; i < end; i++)
          {
            //skip an additional k value per bank
            if (i == 8) k++;

            if (start < 1 && i == 4)
              fprintf (stderr, "\n  ");

            if (start < 5 && i == 8)
              fprintf (stderr, "\n  ");

            if (start < 9 && i == 12)
              fprintf (stderr, "\n  ");

            fprintf (stderr, "LSN %02d: ", i+1);
            if (ch[i] == 1) //group voice channels
            {
              
              tg = (uint16_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][(k*8)+32], 8);
              if (tg != 0) fprintf (stderr, "%5d;  ", tg);
              else fprintf (stderr, "Group;  "); 
              //flag as available for tuning if group calls enabled 
              if (opts->trunk_tune_group_calls == 1) t_tg[i] = tg;
              if (tg != 0) k++;

              //add active channel to display string
              sprintf (cap_active, "LSN:%d TG:%d; ", i+1, tg);
              strcat (state->active_channel[i+1], cap_active);               
            }
            else if (pch[i] == 1) //private or data channels
            {
              tg = (uint16_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][(group_tally*8)+(x*16)+56], 16); //don't change this AGAIN!, this is correct!
              if (tg != 0) fprintf (stderr, "%5d;  ", tg);
              else fprintf (stderr, " P||D;  ");
              //flag as available for tuning if private calls enabled 
              if (opts->trunk_tune_private_calls == 1) t_tg[i] = tg; //ch[i] = 1;
              if (tg != 0) x++;

              //add active channel to display string
              
              //NOTE: Consider only adding this if user toggled or else 
              //lots of short data bursts blink in and out
              if (1 == 1) //opts->trunk_tune_private_calls
              {
                sprintf (cap_active, "LSN:%d PC:%d; ", i+1, tg);
                strcat (state->active_channel[i+1], cap_active);
              }
                
            }  
            else if (i+1 == rest_channel) fprintf (stderr, " Rest;  ");
            else fprintf (stderr, " Idle;  ");

          }


          state->dmr_mfid = 0x10;
          sprintf (state->dmr_branding, "%s", "Motorola");
          sprintf (state->dmr_branding_sub, "%s", "Cap+ ");

          //nullify any previous site_parm data
          sprintf(state->dmr_site_parms, "%s", "");

          fprintf (stderr, "%s", KNRM);

          //Skip tuning group calls if group calls are disabled -- moved to individual switches in the parsing phase
          // if (opts->trunk_tune_group_calls == 0) goto SKIPCAP;

          //Test allowing a group in the white list to preempt a call in progress and tune to a white listed call
          if (opts->trunk_use_allow_list == 1) state->last_vc_sync_time = 0;

          //Test allowing a tg hold to pre-empt a call in progress and tune to the hold TG
          if (state->tg_hold != 0) state->last_vc_sync_time = 0;

          //TODO: Consider a method to allow moving the frequency to the rest channel
          //when a TG hold is specified but nether slot carries the TG on Hold;
          //CODED: using slow link control for that

          //don't tune if vc on the current channel 
          if ( (time(NULL) - state->last_vc_sync_time > 2) ) 
          {
            for (j = start; j < end; j++) //go through the channels stored looking for active ones to tune to
            {
              char mode[8]; //allow, block, digital, enc, etc
              sprintf (mode, "%s", "");

              //if we are using allow/whitelist mode, then write 'B' to mode for block
              //comparison below will look for an 'A' to write to mode if it is allowed
              if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

              for (int i = 0; i < state->group_tally; i++)
              {
                if (state->group_array[i].groupNumber == t_tg[j])
                {
                  fprintf (stderr, " [%s]", state->group_array[i].groupName);
                  strcpy (mode, state->group_array[i].groupMode);
                  break;
                }
              }

              //TG hold on DMR Cap+ -- block non-matching target, allow matching target
              if (state->tg_hold != 0 && state->tg_hold != t_tg[j]) sprintf (mode, "%s", "B");
              if (state->tg_hold != 0 && state->tg_hold == t_tg[j]) sprintf (mode, "%s", "A");

              //without priority, this will tune the first one it finds (if group isn't blocked)
              if (t_tg[j] != 0 && state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "B") != 0) && (strcmp(mode, "DE") != 0)) 
              {
                //debug print for tuning verification
                // fprintf (stderr, "\n LSN/TG to tune to: %d - %d", j+1, t_tg[j]);
                
                if (state->trunk_chan_map[j+1] != 0) //if we have a valid frequency
                {
                  //RIGCTL
                  if (opts->use_rigctl == 1)
                  {

                    //may need the code below to TG hold (just in case SLC comes before a VLC or a VC6 EMB and immediately goes to the rest channel)
                    //disable or tweak code below if these are reversed somehow or causes issues
                    if (state->tg_hold != 0)
                    {
                      if ( (j&1) == 0 ) //slot 1 LSN
                      {
                        state->lasttg = t_tg[j];
                        // state->lastsrc = source;
                      }
                      else //slot 2 LSN
                      {
                        state->lasttgR = t_tg[j];
                        // state->lastsrcR = source;
                      }
                      //end TG set on tune
                    }
                    if (GetCurrentFreq(opts->rigctl_sockfd) != state->trunk_chan_map[j+1])
                      dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency
                    if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
                    SetFreq(opts->rigctl_sockfd, state->trunk_chan_map[j+1]); 
                    state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+1];
                    opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                    state->last_vc_sync_time = time(NULL);
                    j = 11; //break loop
                  }

                  //rtl
                  else if (opts->audio_in_type == 3)
                  {
                    #ifdef USE_RTLSDR

                    //may need the code below to TG hold (just in case SLC comes before a VLC or a VC6 EMB and immediately goes to the rest channel)
                    //disable or tweak code below if these are reversed somehow or causes issues
                    if (state->tg_hold != 0)
                    {
                      if ( (j&1) == 0 ) //slot 1 LSN
                      {
                        state->lasttg = t_tg[j];
                        // state->lastsrc = source;
                      }
                      else //slot 2 LSN
                      {
                        state->lasttgR = t_tg[j];
                        // state->lastsrcR = source;
                      }
                      //end TG set on tune
                    }
                    uint32_t temp = (uint32_t)state->trunk_chan_map[j+1];
                    if (opts->rtlsdr_center_freq != temp)
                    {
                      dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency
                      rtl_dev_tune (opts, state->trunk_chan_map[j+1]); //unlike rigctl, using this actually interrupts signal decodes (rtl_clean_queue)
                      //debug print for tuning verification
                      // fprintf (stderr, "\n RTL LSN/TG to tune to: %d - %d", j+1, t_tg[j]);
                    }
                    // else fprintf (stderr, "\n DONT RTL LSN/TG to tune to: %d - %d", j+1, t_tg[j]); //debug
                    state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+1];
                    opts->p25_is_tuned = 1;
                    state->last_vc_sync_time = time(NULL);
                    j = 11; //break loop
                    #endif
                  }

                }
              }

            }
          } //end tuning

          // SKIPCAP: ;
          //debug print
          if (fl == 1 && opts->payload == 1)
          {
            fprintf (stderr, "%s\n", KYEL); 
            fprintf (stderr, " CAP+ Multi Block PDU \n  ");
            fl_bytes = 0;
            for (i = 0; i < (10+(block_num*7)); i++) 
            {
              fl_bytes = (uint8_t)ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][i*8], 8);
              fprintf (stderr, "[%02X]", fl_bytes);
              if (i == 17 || i == 35) fprintf (stderr, "\n  ");
            }
            fprintf (stderr, "%s", KNRM);
          }
          memset (state->cap_plus_csbk_bits[ts], 0, sizeof(state->cap_plus_csbk_bits[ts]));
          state->cap_plus_block_num[ts] = 0;

          //check here to see if we can go to the current rest lsn, if not already on it
          int busy = memcmp (empty, t_tg, sizeof(empty));

          //testing (don't keep setting on quick data call LSN flip flops but same frequency for rest channel, other misc conditions)
          // if (!busy && rest_channel != state->dmr_rest_channel && opts->p25_trunk == 1 && state->p25_cc_freq != state->trunk_chan_map[rest_channel])
          if (!busy && opts->p25_trunk == 1 && state->p25_cc_freq != state->trunk_chan_map[rest_channel])
          {
            //assign now, ideally, this should always trigger a positive p_clear when needed
            // state->dmr_rest_channel = rest_channel;

            //update frequency
            if (state->trunk_chan_map[rest_channel] != 0)
              state->p25_cc_freq = state->trunk_chan_map[rest_channel];

            //Craft a fake CSBK pdu send it to run as a p_clear to go to rest channel if its available (no calls currently)
            uint8_t dummy[12]; uint8_t* dbits; memset (dummy, 0, sizeof(dummy)); dummy[0] = 46; dummy[1] = 253;
              dmr_cspdu (opts, state, dbits, dummy, 1, 0);
          }
          
        } //if (fl == 1 || fl == 3)
      } //opcode == 0x3E

    } //fid == 0x10

    //Connect+ Section
    if (csbk_fid == 0x06)
    {
      //Con+ Adjacent Site 
      if (csbk_o == 0x01)
      {
        uint8_t nb1 = cs_pdu[2] & 0x3F; 
        uint8_t nb2 = cs_pdu[3] & 0x3F; 
        uint8_t nb3 = cs_pdu[4] & 0x3F; 
        uint8_t nb4 = cs_pdu[5] & 0x3F; 
        uint8_t nb5 = cs_pdu[6] & 0x3F;

        //initial line break
        fprintf (stderr, "\n"); 
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Adjacent Sites:");
        if (nb1 != 0) fprintf (stderr, " %d;", nb1);
        if (nb2 != 0) fprintf (stderr, " %d;", nb2);
        if (nb3 != 0) fprintf (stderr, " %d;", nb3);
        if (nb4 != 0) fprintf (stderr, " %d;", nb4);
        if (nb5 != 0) fprintf (stderr, " %d;", nb5);
        if (nb1 == 0) fprintf (stderr, " None Listed;");
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");

      }

      //Con+ Voice Channel Grant
      if (csbk_o == 0x03)
      {

        /*
        //this was a private voice call, see if a bit is flipped somewhere to indicate as much, octet 9?
        Connect Plus Voice Channel Grant; Target: 118903; Source: 151015; LCN: 1; TS: 0;
        DMR PDU Payload [83][06][02][4D][E7][01][D0][77][10][03][C2][CA] <-- 03 private?

        //this was from a group voice call
        18:06:29 Sync: +DMR  [slot1]  slot2  | Color Code=01 | CSBK
        Connect Plus Voice Channel Grant; Target: 1216; Source: 113114; LCN: 2; TS: 1;
        DMR PDU Payload [83][06][01][B9][DA][00][04][C0][28][02][AA][FF] <--02 group?

        //the 02/03 holds up on a few samples from different systems, also seems
        //that encrypted calls are not bit flipped here
        */
        
        //initial line break
        fprintf (stderr, "\n");
        uint32_t srcAddr = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        uint32_t grpAddr = ( (cs_pdu[5] << 16) + (cs_pdu[6] << 8) + cs_pdu[7] ); 
        uint8_t  lcn     = ( (cs_pdu[8] & 0xF0 ) >> 4 ); 
        uint8_t  tslot   = ( (cs_pdu[8] & 0x08 ) >> 3 ) & 1;  
        uint8_t  opt     =    cs_pdu[9]; //call options? May only be a few of the LSB honestly
        fprintf (stderr, "%s", KYEL);
        if (opt == 2)
          fprintf (stderr, " Connect Plus Group Voice Channel Grant;");
        else if (opt == 3)
          fprintf (stderr, " Connect Plus Private Voice Channel Grant;");
        else fprintf (stderr, " Connect Plus Unknown %02X Channel Grant;", opt);
        fprintf (stderr, " Target: %d; Source: %d; LCN: %d; TS: %d;", grpAddr, srcAddr, lcn, tslot+1);
        // fprintf (stderr, " OPT: %02X;", opt); //debug
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");

        //add active channel string for display
        if (opt == 2)
          sprintf (state->active_channel[tslot], "Active Group Ch: %d TG: %d; ", lcn, grpAddr);
        else if (opt == 3)
          sprintf (state->active_channel[tslot], "Active Private Ch: %d TG: %d; ", lcn, grpAddr);
        else //generic channel of unknown type
          sprintf (state->active_channel[tslot], "Active OPT %02X Ch: %d TG: %d; ", opt, lcn, grpAddr);

        state->last_active_time = time(NULL);

        //Skip tuning group calls if group calls are disabled
        if (opts->trunk_tune_group_calls == 0 && opt == 2) goto SKIPCON;

        //Skip tuning private calls if private calls are disabled
        if (opts->trunk_tune_private_calls == 0 && opt == 3) goto SKIPCON;

        //skip tuning if opt is not 2 or 3 (assume group call option)
        if (opts->trunk_tune_group_calls == 0 && opt != 2 && opt != 3) goto SKIPCON;

        //NOTE: Only set CC Frequency from SLC since it will tell us
        //when we are on a control channel, and not a payload channel,
        //these CSBKs can come on payload channels as well and cause
        //FME to return to a dead air channel and start searching

        //shim in here for ncurses freq display when not trunking (playback, not live)
        if (opts->p25_trunk == 0 && state->trunk_chan_map[lcn] != 0)
        {
          //just set to both for now, could go on tslot later
          state->p25_vc_freq[0] = state->trunk_chan_map[lcn];
          state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
        }

        //if tg hold is specified and matches target, allow for a call pre-emption by nullifying the last vc sync time
        if (state->tg_hold != 0 && state->tg_hold == grpAddr)
          state->last_vc_sync_time = 0;

        char mode[8]; //allow, block, digital, enc, etc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == grpAddr)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on DMR Con+ -- block non-matching target, allow matching target
        if (state->tg_hold != 0 && state->tg_hold != grpAddr) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == grpAddr) sprintf (mode, "%s", "A");

        //TG Hop if the target here matches the target currently listening to (may be better to just rely on TG hold for this)
        // if (state->currentslot == 1 && state->lasttg  == grpAddr) state->last_vc_sync_time = 0; //opposite slot if TLC in current
        // if (state->currentslot == 0 && state->lasttgR == grpAddr) state->last_vc_sync_time = 0; //opposite slot if TLC in current

        time_t waitsec = 2;
        if (opts->trunk_tune_data_calls == 1)
          waitsec = 4; //increase time for a data call before hopping

        //don't tune if currently a vc on the control channel, but allow hopping form one VC to another VC if the former is in long TLC/Idle mode
        if ( (opts->trunk_tune_group_calls == 1) && (time(NULL) - state->last_vc_sync_time > waitsec) ) 
        {
          
          if (state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "B") != 0) && (strcmp(mode, "DE") != 0) ) 
          {
            if (state->trunk_chan_map[lcn] != 0) //if we have a valid frequency
            {
              //RIGCTL
              if (opts->use_rigctl == 1)
              {
                if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                SetFreq(opts->rigctl_sockfd, state->trunk_chan_map[lcn]); 
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
                opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
                state->is_con_plus = 1; //flag on
                state->last_vc_sync_time = time(NULL); //bugfix: set sync here so we don't immediately tune back to CC constantly.
                dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
              }

              //rtl
              else if (opts->audio_in_type == 3)
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, state->trunk_chan_map[lcn]);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
                opts->p25_is_tuned = 1;
                state->is_con_plus = 1; //flag on
                state->last_vc_sync_time = time(NULL); //bugfix: set sync here so we don't immediately tune back to CC constantly.
                dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
                #endif
              }

            }
          }
        }  

        SKIPCON: ; //do nothing

      }

      //Con+ Data Channel Grant
      if (csbk_o == 0x06)
      {
        
        //initial line break
        fprintf (stderr, "\n");
        uint32_t dtarget = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        uint8_t  lcn     = ( (cs_pdu[5] & 0xF0 ) >> 4 ); 
        uint8_t  tslot   = ( (cs_pdu[5] & 0x08 ) >> 3 ) & 1;  
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Data Channel Grant;");
        fprintf (stderr, " Target: %d; LCN: %d; TS: %d;", dtarget, lcn, tslot+1);
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");

        //Skip tuning data calls if data calls are disabled
        if (opts->trunk_tune_data_calls == 0) goto SKIPCOND;

        //add active channel string for display
        sprintf (state->active_channel[tslot], "Active Data Ch: %d TG: %d; ", lcn, dtarget);
        state->last_active_time = time(NULL);

        //NOTE: Only set CC Frequency from SLC since it will tell us
        //when we are on a control channel, and not a payload channel,
        //these CSBKs can come on payload channels as well and cause
        //FME to return to a dead air channel and start searching

        //shim in here for ncurses freq display when not trunking (playback, not live)
        if (opts->p25_trunk == 0 && state->trunk_chan_map[lcn] != 0)
        {
          //just set to both for now, could go on tslot later
          state->p25_vc_freq[0] = state->trunk_chan_map[lcn];
          state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
        }

        //if tg hold is specified and matches target, allow for a call pre-emption by nullifying the last vc sync time
        // if (state->tg_hold != 0 && state->tg_hold == dtarget)
        //   state->last_vc_sync_time = 0;

        char mode[8]; //allow, block, digital, enc, etc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == dtarget)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on DMR Con+ -- block non-matching target, allow matching target
        // if (state->tg_hold != 0 && state->tg_hold != dtarget) sprintf (mode, "%s", "B");
        // if (state->tg_hold != 0 && state->tg_hold == dtarget) sprintf (mode, "%s", "A");

        //don't tune if currently a vc on the control channel
        if ( (opts->trunk_tune_data_calls == 1) && (time(NULL) - state->last_vc_sync_time > 2) ) 
        {
          
          if (state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "B") != 0) && (strcmp(mode, "DE") != 0) ) 
          {
            if (state->trunk_chan_map[lcn] != 0) //if we have a valid frequency
            {
              //RIGCTL
              if (opts->use_rigctl == 1)
              {
                if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                SetFreq(opts->rigctl_sockfd, state->trunk_chan_map[lcn]); 
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
                opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
                state->is_con_plus = 1; //flag on
                state->last_vc_sync_time = time(NULL); //bugfix: set sync here so we don't immediately tune back to CC constantly.
                dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
              }

              //rtl
              else if (opts->audio_in_type == 3)
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, state->trunk_chan_map[lcn]);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
                opts->p25_is_tuned = 1;
                state->is_con_plus = 1; //flag on
                state->last_vc_sync_time = time(NULL); //bugfix: set sync here so we don't immediately tune back to CC constantly.
                dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
                #endif
              }

            }
          }
        }  

        SKIPCOND: ; //do nothing

      }

      //Con+ Slot Termination
      if (csbk_o == 0x0C)
      {

        //initial line break
        fprintf (stderr, "\n");
        uint32_t ttarget = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Slot Termination;");
        fprintf (stderr, " Target: %d;", ttarget);

        //Run p_clear to decide whether or not to return to the control channel
        uint8_t dummy[12]; uint8_t* dbits; memset (dummy, 0, sizeof(dummy)); dummy[0] = 46; dummy[1] = 12;
        dmr_cspdu (opts, state, dbits, dummy, 1, 0);

        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");

      }

      fprintf (stderr, "%s", KNRM);

    } //end Connect+

    //Hytera XPT -- Experimental, but values now seem very consistent and trunking is working on systems tested with
    if (csbk_fid == 0x68)
    {
      //XPT Site Status 
      if (csbk_o == 0x0A)
      {
        //initial line break
        fprintf (stderr, "\n"); 
        fprintf (stderr, "%s", KYEL);

        uint8_t xpt_ch[6]; //one tg/call per LSN
        uint16_t tg = 0;  //8-bit TG value or TGT hash
        int i, j;

        //tg and channel info for trunking purposes
        uint8_t t_tg[6];
        memset (t_tg, 0, sizeof(t_tg));

        uint8_t xpt_seq    = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[0], 2);  //this replaces the CSBK lb and pf
        uint8_t xpt_free   = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 4); //free repeater channel LCN
        uint8_t xpt_bank   = 0;
        uint8_t xpt_lcn    = 1;

        //determine starting position of LCN value
        if (xpt_seq == 1) xpt_lcn = 4;
        if (xpt_seq == 2) xpt_lcn = 7; 

        //determine xpt_bank value for LSN value/tuning
        if (xpt_seq) xpt_bank = xpt_seq*6; 

        //get 2-bit status values for each 6 LSNs
        for (i = 0; i < 6; i++) xpt_ch[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[20+(i*2)], 2);

        fprintf (stderr, " Hytera XPT Site Status - Free LCN: %d SN: %d", xpt_free, xpt_seq);

        //strings for active channels
        char xpt_active[20];
        if (xpt_seq == 0) sprintf (state->active_channel[0], "XPT "); //add initial if sequence 0
        else sprintf (state->active_channel[xpt_seq], "%s", ""); //blank current sequence to re-write to
        state->last_active_time = time(NULL);

        //Print List of LCN with LSN Activity 
        for (i = 0; i < 6; i++) 
        {
          //add LCN value for each LSN pair to help users differentiate between the two when seeing LCN on FLC
          if (i == 0 || i == 2 || i == 4)
          {
            fprintf (stderr, "\n LCN %d - ", xpt_lcn);
            xpt_lcn++;
          } 
          
          //LSN value here is logical slot, in flco, we get the logical channel (which is the repeater, does not include the slot value)
          fprintf (stderr, "LSN %02d: ", i+xpt_bank+1); //swapped out xpt_bank for all (xpt_seq*6) to simplify things
          tg = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[i*8+32], 8);
          fprintf (stderr, "ST-%X", xpt_ch[i]); //status bits value 0,1,2, or 3
          if (tg != 0)                fprintf (stderr, " %03d;  ", tg); 
          else
          {
            if (xpt_ch[i] == 3)         fprintf (stderr, " Null; "); //NULL on ST-3 indicates repeater is not active for the LSNs that would be covered by these bits
            else if (xpt_ch[i] == 2)    fprintf (stderr, " Priv; "); //This seems to be used on both private data and private voice calls, but occassionally has a 0 TGT value
            else if (xpt_ch[i] == 1)    fprintf (stderr, " Unk;  "); //the 01 value has not been observed as of yet
            else if (xpt_ch[i] == 0)    fprintf (stderr, " Idle; "); //Idle

            //if Priv/Unk occurs, add t_tg value to it for tuning purposes
            if (xpt_ch[i] == 2)
            {
              if (opts->trunk_tune_private_calls == 1) t_tg[i] = 1;
            }

            //could cause erroneous tuning since we don't yet know what the unk value really is (never observed)
            if (xpt_ch[i] == 1)
            {
              // if (opts->trunk_tune_data_calls == 1) t_tg[i] = 1; //disabled
            }

          }

          // if (i == 2) fprintf (stderr, "\n ");

          //add values to trunking tg/channel potentials
          if (tg != 0) t_tg[i+xpt_bank] = tg;

          //concantenate string to active channels for ncurses display
          if (tg != 0)
          {
            if (xpt_ch[i] == 3) sprintf (xpt_active, "LSN:%d TG:%d; ", i+xpt_bank+1, tg);
            if (xpt_ch[i] == 2) sprintf (xpt_active, "LSN:%d PC:%d; ", i+xpt_bank+1, tg);
            //last two are unknown status but have an associated target value
            if (xpt_ch[i] == 1) sprintf (xpt_active, "LSN:%d UK:%d; ", i+xpt_bank+1, tg);
            if (xpt_ch[i] == 0) sprintf (xpt_active, "LSN:%d UK:%d; ", i+xpt_bank+1, tg);
            strcat (state->active_channel[xpt_seq], xpt_active); //add string to active channel seq
          }

        }

        //add string for ncurses terminal display
        sprintf (state->dmr_site_parms, "Free LCN - %d ", xpt_free);

        //assign to cc freq to follow during no sync
        long int ccfreq = 0; 

        //if using rigctl we can set an unknown or updated cc frequency 
        if (opts->use_rigctl == 1 ) 
        {
          ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
          if (ccfreq != 0)
          {
            state->p25_cc_freq = ccfreq;
            opts->p25_is_tuned = 1;
          } 
        }

        //if using rtl input, we can ask for the current frequency tuned
        if (opts->audio_in_type == 3 )
        {
          ccfreq = (long int)opts->rtlsdr_center_freq;
          if (ccfreq != 0)
          {
            state->p25_cc_freq = ccfreq;
            opts->p25_is_tuned = 1;
          } 

        }

        //Skip tuning calls if group calls are disabled
        if (opts->trunk_tune_group_calls == 0) goto SKIPXPT;

        //Test allowing a group in the white list to preempt a call in progress and tune to a white listed call
        if (opts->trunk_use_allow_list == 1) state->last_vc_sync_time = 0;

        //Test allowing a tg hold to pre-empt a call in progress and tune to the hold TG
        if (state->tg_hold != 0) state->last_vc_sync_time = 0;

        //TODO: Consider a method to allow moving the frequency to the free repeater channel
        //when a TG hold is specified but nether slot carries the TG on Hold;
        //CODED: using Free repeater in SLC to change over if required

        //don't tune if vc on the current channel 
        if ( (time(NULL) - state->last_vc_sync_time) > 2 ) //parenthesis error fixed
        {
          for (j = 0; j < 6; j++) //go through the channels stored looking for active ones to tune to
          {
            char mode[8]; //allow, block, digital, enc, etc
            sprintf (mode, "%s", "");

            //if we are using allow/whitelist mode, then write 'B' to mode for block
            //comparison below will look for an 'A' to write to mode if it is allowed
            if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

            //this won't work properly on hashed TGT values
            //unless users load a TGT hash in a csv file
            //and hope it doesn't clash with other normal TG values
            for (int i = 0; i < state->group_tally; i++)
            {
              if (state->group_array[i].groupNumber == t_tg[j+xpt_bank])
              {
                fprintf (stderr, " [%s]", state->group_array[i].groupName);
                strcpy (mode, state->group_array[i].groupMode);
                break;
              }
            }

            //TG hold on DMR XPT -- block non-matching target, allow matching target
            if (state->tg_hold != 0 && state->tg_hold != t_tg[j+xpt_bank]) sprintf (mode, "%s", "B");
            if (state->tg_hold != 0 && state->tg_hold == t_tg[j+xpt_bank]) sprintf (mode, "%s", "A");

            //without priority, this will tune the first one it finds (if group isn't blocked)
            if (t_tg[j+xpt_bank] != 0 && state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "B") != 0) && (strcmp(mode, "DE") != 0)) 
            {
              //debug print for tuning verification
              fprintf (stderr, "\n LSN/TG to tune to: %d - %d", j+xpt_bank+1, t_tg[j+xpt_bank]);

              if (state->trunk_chan_map[j+xpt_bank+1] != 0) //if we have a valid frequency
              {
                //RIGCTL
                if (opts->use_rigctl == 1)
                {

                  //may need the code below to TG hold (just in case SLC comes before a VLC or a VC6 EMB and immediately goes to the free lcn channel)
                  //disable or tweak code below if these are reversed somehow or causes issues
                  if (state->tg_hold != 0)
                  {
                    if ( (j&1) == 0 ) //slot 1 LSN
                    {
                      state->lasttg = t_tg[j+xpt_bank];
                      // state->lastsrc = source;
                    }
                    else //slot 2 LSN
                    {
                      state->lasttgR = t_tg[j+xpt_bank];
                      // state->lastsrcR = source;
                    }
                    //end TG set on tune
                  }

                  //debug 
                  // fprintf (stderr, " - Freq: %ld", state->trunk_chan_map[j+xpt_bank+1]);
                  if (GetCurrentFreq(opts->rigctl_sockfd) != state->trunk_chan_map[j+xpt_bank+1])
                    dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency

                  if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
                  SetFreq(opts->rigctl_sockfd, state->trunk_chan_map[j+xpt_bank+1]); 
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+xpt_bank+1];
                  opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                  j = 11; //break loop
                }

                //rtl
                else if (opts->audio_in_type == 3)
                {
                  #ifdef USE_RTLSDR

                  //may need the code below to TG hold (just in case SLC comes before a VLC or a VC6 EMB and immediately goes to the free lcn channel)
                  //disable or tweak code below if these are reversed somehow or causes issues
                  if (state->tg_hold != 0)
                  {
                    if ( (j&1) == 0 ) //slot 1 LSN
                    {
                      state->lasttg = t_tg[j+xpt_bank];
                      // state->lastsrc = source;
                    }
                    else //slot 2 LSN
                    {
                      state->lasttgR = t_tg[j+xpt_bank];
                      // state->lastsrcR = source;
                    }
                    //end TG set on tune
                  }

                  uint32_t temp = (uint32_t)state->trunk_chan_map[j+xpt_bank+1];
                  if (opts->rtlsdr_center_freq != temp)
                  {
                    dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away from current frequency
                    rtl_dev_tune (opts, state->trunk_chan_map[j+xpt_bank+1]); //unlike rigctl, using this actually interrupts signal decodes (rtl_clean_queue)
                    //debug print for tuning verification
                    fprintf (stderr, " - Tune to Freq: %ld", state->trunk_chan_map[j+xpt_bank+1]);
                  }
                  else fprintf (stderr, " - Dont Tune Freq: %ld", state->trunk_chan_map[j+xpt_bank+1]);                 
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+xpt_bank+1];
                  opts->p25_is_tuned = 1;
                  j = 11; //break loop
                  #endif
                }

              }
            }

          }
        } //end tuning

        SKIPXPT: ;

        sprintf (state->dmr_branding_sub, "XPT ");

        //Notes: I had a few issues to fix in this CSBK, but it does appear that this is trunking on a few small systems now,
        //albeit a very quiet systems that makes it difficult to know for certain if every aspect is working correctly

        //Notes: I've set XPT to set a CC frequency to whichever frequency its tuned to currently and getting
        //this particular CSBK, if the status portion does work correctly, then it shouldn't matter which frequency it is on
        //as long as this CSBK comes in and we can tune to other repeater lcns if they have activity and the frequency mapping
        //is correct in the csv file, assuming the current frequency doesn't have voice activity

      } //end 0x0A

      //XPT Adjacent Site Information -- Have yet to find a consistent indication of 'current site' identification in any CSBK/FLC/SLC payload
      if (csbk_o == 0x0B)
      {

        //initial line break
        fprintf (stderr, "\n"); 
        fprintf (stderr, "%s", KYEL);

        int i;
        uint8_t xpt_site_id[4];
        uint8_t xpt_site_rp[4];
        uint8_t xpt_site_u1[4];
        uint8_t xpt_site_u2[4];
        UNUSED2(xpt_site_u1, xpt_site_u2);

        uint8_t xpt_sn = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[0], 2); 

        for (i = 0; i < 4; i++)
        {
          xpt_site_id[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16+(i*16)], 5);
          xpt_site_u1[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[21+(i*16)], 3);
          xpt_site_rp[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[24+(i*16)], 4);
          xpt_site_u2[i] = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[28+(i*16)], 4);
        }

        fprintf (stderr, " Hytera XPT CSBK 0x0B - SN: %d", xpt_sn);
        fprintf (stderr, "\n");
        fprintf (stderr, " XPT Adjacent ");
        for (i = 0; i < 4; i++)
        {
          if (xpt_site_id[i] != 0) 
          {
            fprintf (stderr, "Site:%d Free:%d; ", xpt_site_id[i], xpt_site_rp[i]);
            // fprintf (stderr, "RS1: %d RS2: %d - ", xpt_site_u1[i], xpt_site_u2[i]); //debug
          }
        }
        sprintf (state->dmr_branding_sub, "XPT ");

      } //end 0x0B

    } //end Hytera XPT section

    //misc discovered but not uncovered CSBKs
    if (csbk_o == 41 && csbk_fid == 0x10)
    {
      //initial line break
      fprintf (stderr, "\n"); 
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Moto Data Channel: %02X; ", csbk_o);
      for (int i = 2; i < 10; i++)
        fprintf (stderr, "%02X ", cs_pdu[i]);

      //SDRTrunk suggest this could be a data channel revert announcement
      //I'm not even sure what a revert data channel is
      //Moto Unknown Data Opcode: 29; 00 00 00 39 04 FC 00 00
    }

  }

  fprintf (stderr, "%s", KNRM);  

}

//translate special gateway identifier addresses
void dmr_gateway_identifier (uint32_t source, uint32_t target)
{
  int i;
  uint32_t id;
  for (i = 0; i < 2; i++)
  {
    if (i == 0) id = source;
    else id = target;
    if (id == 0xFFFEC0) fprintf (stderr, "PSTNI; ");
    if (id == 0xFFFEC1) fprintf (stderr, "PABXI; ");
    if (id == 0xFFFEC2) fprintf (stderr, "LINEI; ");
    if (id == 0xFFFEC3) fprintf (stderr, "IPI; ");
    if (id == 0xFFFEC4) fprintf (stderr, "SUPLI; ");
    if (id == 0xFFFEC5) fprintf (stderr, "SDMI; ");
    if (id == 0xFFFEC6) fprintf (stderr, "REGI; ");
    if (id == 0xFFFEC7) fprintf (stderr, "MSI; ");
    if (id == 0xFFFEC8) fprintf (stderr, "RESERVED; ");
    if (id == 0xFFFEC9) fprintf (stderr, "DIVERTI; ");
    if (id == 0xFFFECA) fprintf (stderr, "TSI; ");
    if (id == 0xFFFECB) fprintf (stderr, "DISPATI; ");
    if (id == 0xFFFECC) fprintf (stderr, "STUNI; ");
    if (id == 0xFFFECD) fprintf (stderr, "AUTHI; ");
    if (id == 0xFFFECE) fprintf (stderr, "GPI; ");
    if (id == 0xFFFECF) fprintf (stderr, "KILLI; ");
    if (id == 0xFFFED0) fprintf (stderr, "PSTNDI; ");
    if (id == 0xFFFED1) fprintf (stderr, "PABXDI; ");
    if (id == 0xFFFED2) fprintf (stderr, "LINEDI; ");
    if (id == 0xFFFED3) fprintf (stderr, "DISPATDI; ");
    if (id == 0xFFFED4) fprintf (stderr, "ALLMSI; ");
    if (id == 0xFFFED5) fprintf (stderr, "IPDI; ");
    if (id == 0xFFFED6) fprintf (stderr, "DGNAI; ");
    if (id == 0xFFFED7) fprintf (stderr, "TATTSI; ");
    if (id == 0xFFFFFD) fprintf (stderr, "ALLMSIDL; ");
    if (id == 0xFFFFFE) fprintf (stderr, "ALLMSIDZ; ");
    if (id == 0xFFFFFF) fprintf (stderr, "ALLMSID; ");

    //NOTE: Observed address values of 64250, or 0xFAFA have been observed
    //on some Moto Tier 2 and Cap+ Systems, and 0xFAFAFA has been observed
    //on some Moto Tier 3 (CapMax) Systems, unsure if these are unique to that
    //manufacturer, or not, usually associated with Data Headers and PDU Messages
  }
}

void dmr_decode_syscode(dsd_opts * opts, dsd_state * state, uint8_t * cs_pdu_bits, int csbk_fid, int type)
{
  //TODO: Probably return specific elements of just the aloha to its own area, and just do the syscode here
  int i;
  //copy and paste code into here, use type to determine whether or not to set current site info or not
  uint8_t reserved = cs_pdu_bits[16];
  uint8_t tsccas = cs_pdu_bits[17];
  uint8_t sync = cs_pdu_bits[18];
  uint8_t version = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[19], 3); //Document Version Control
  uint8_t offset = cs_pdu_bits[22]; //0-TSCC uses aligned timing; 1-TSCC uses offset timing
  uint8_t active = cs_pdu_bits[23]; //Active_Connection
  uint8_t mask = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[24], 5);
  uint8_t sf = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[29], 2); //service function
  uint8_t nrandwait = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[31], 4);
  uint8_t regreq = cs_pdu_bits[35];
  uint8_t backoff = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[36], 4);
  UNUSED5(reserved, tsccas, sync, offset, active);
  UNUSED3(sf, nrandwait, backoff);

  //bparms1
  uint8_t bpbits1[14]; for (i = 0; i < 14; i++) bpbits1[i] = cs_pdu_bits[21+i];

  //if not C_ALOHA_SYS_PARMS, overwrite syscode with bparms1 (too lazy method) 
  if (type != 0)
  {
    for (i = 0; i < 14; i++)
      cs_pdu_bits[40+i] = bpbits1[i];
  }

  //raw syscode
  uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 14);

  uint8_t model = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 2);
  uint16_t net = 0;
  uint16_t site = 0;

  //DMR Location Area - DMRLA
  uint16_t sub_mask = 0x1;
  //tiny n 1-3; small 1-5; large 1-8; huge 1-10
  uint16_t n = 1; //The minimum value of DMRLA is normally ≥ 1, 0 is reserved


  char model_str[8];
  char par_str[8]; //category A, B, AB, or reserved

  sprintf (model_str, "%s", " ");
  sprintf (par_str, "%s", "Res");

  if (model == 0) //Tiny
  {
    net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 9);
    site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[51], 3);
    sprintf (model_str, "%s", "Tiny");
    n = 3;
  }
  else if (model == 1) //Small
  {
    net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 7);
    site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[49], 5);
    sprintf (model_str, "%s", "Small");
    n = 5;
  }
  else if (model == 2) //Large
  {
    net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 4);
    site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[46], 8);
    sprintf (model_str, "%s", "Large");
    n = 8;
  }
  else if (model == 3) //Huge
  {
    net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 2);
    site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[44], 10);
    sprintf (model_str, "%s", "Huge");
    n = 10;
  }

  //honestly can't say that this is accurate, just a guess
  uint8_t is_capmax = 0; //capmax(mot) flag
  if (csbk_fid == 0x10)
  {
    n = 0;
    is_capmax = 1;
    opts->dmr_dmrla_is_set = 1;
    opts->dmr_dmrla_n = 0;
    sprintf (state->dmr_branding, "%s", "Motorola");
    // sprintf (state->dmr_branding_sub, "%s", "CapMax ");
  } 

  if (opts->dmr_dmrla_is_set == 1) n = opts->dmr_dmrla_n;

  if (n == 0) sub_mask = 0x0;
  if (n == 1) sub_mask = 0x1;
  if (n == 2) sub_mask = 0x3;
  if (n == 3) sub_mask = 0x7;
  if (n == 4) sub_mask = 0xF;
  if (n == 5) sub_mask = 0x1F;
  if (n == 6) sub_mask = 0x3F;
  if (n == 7) sub_mask = 0x7F;
  if (n == 8) sub_mask = 0xFF;
  if (n == 9) sub_mask = 0x1FF;
  if (n == 10) sub_mask = 0x3FF;

  uint8_t par = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[54], 2);
  if (par == 1) sprintf (par_str, "%s", "A");
  if (par == 2) sprintf (par_str, "%s", "B");
  if (par == 3) sprintf (par_str, "%s", "AB");

  uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24); 

  if (type == 0)
  {
    //considering not adding a +1 to these values (will need to consult the docs first)
    //TODO: Change SLC to mirror this output format
    if (n != 0) fprintf (stderr, " C_ALOHA_SYS_PARMS: %s; Net ID: %d; Site ID: %d.%d; Cat: %s;", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, par_str);
    else fprintf (stderr, " C_ALOHA_SYS_PARMS: %s; Net ID: %d; Site ID: %d;", model_str, net, site);
    fprintf (stderr, " SYS: %04X;", syscode); //#192
    if (is_capmax) fprintf (stderr, " Capacity Max");
    
    if (opts->payload == 1)
    {
      fprintf (stderr, "\n");
      if (reserved) fprintf (stderr, " Res: %04X;", reserved);
      if (tsccas) fprintf (stderr, " TSCCAS;");
      if (sync) fprintf (stderr, " Sync;");
      fprintf (stderr, " Ver: %d;", version);
      if (offset) fprintf (stderr, " Offset;");
      if (active) fprintf (stderr, " Active Connection;");
      fprintf (stderr, " SF: %d;", sf); //service function
      fprintf (stderr, " NR: %X;", nrandwait); //what is this for again?
      if (regreq) fprintf (stderr, " Reg Required;");
      fprintf (stderr, " Backoff: %X;", backoff);
      if (mask) fprintf (stderr, " Mask: %02X;", mask);
      if (target) fprintf (stderr, " MS: %d; ", target);
      dmr_gateway_identifier(0,target); //its either 0, or possibly ALLMSI, or a specifically targeted value?
    }

    //add string for ncurses terminal display
    // if (n != 0) sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d; SYS: %04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    // else sprintf (state->dmr_site_parms, "TIII - %s %d-%d; SYS: %04X; ", model_str, net, site, syscode);
    if (n != 0) sprintf (state->dmr_site_parms, "TIII %s:%d-%d.%d;%04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    else sprintf (state->dmr_site_parms, "TIII %s:%d-%d;%04X; ", model_str, net, site, syscode);
  }

  if (type == 1)
  {
    //NOTE: I just wrote bparms1 into the area where syscode is when it is an adj_site (or votenow site)
    if (n != 0) fprintf (stderr, " %s; Net ID: %d; Site ID: %d.%d;", model_str, net+1, (site>>n)+1, (site & sub_mask)+1); //par_string available here?
    else fprintf (stderr, " %s; Net ID: %d; Site ID: %d;", model_str, net, site);
    fprintf (stderr, " SYS: %04X;", syscode); //#192
    // if (is_capmax) fprintf (stderr, "Capacity Max ");
  }

}