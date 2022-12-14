/*-------------------------------------------------------------------------------
 * dmr_csbk.c
 * DMR Control Signal Data PDU (CSBK, MBC, UDT) Handler and Related Functions
 *
 * Portions of Connect+ code reworked from Boatbod OP25
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * Portions of Capacity+ code reworked from Eric Cottrell
 * Source: https://github.com/LinuxSheeple-E/dsd/blob/Feature/DMRECC/dmr_csbk.c
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//function for handling Control Signalling PDUs (CSBK, MBC, UDT) messages
void dmr_cspdu (dsd_opts * opts, dsd_state * state, uint8_t cs_pdu_bits[], uint8_t cs_pdu[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors)
{
  
  int  csbk_lb   = 0;
  int  csbk_pf   = 0;
  int  csbk_o    = 0;
  int  csbk_fid  = 0;
  uint64_t csbk_data = 0;
  int csbk      = 0;

  long int ccfreq = 0;

  if(IrrecoverableErrors == 0 && CRCCorrect == 1) 
  {
    
    csbk_lb  = ( (cs_pdu[0] & 0x80) >> 7 );
    csbk_pf  = ( (cs_pdu[0] & 0x40) >> 6 );
    csbk_o   =    cs_pdu[0] & 0x3F; 
    csbk_fid =    cs_pdu[1]; //feature set id
    csbk_pf  = ( (cs_pdu[1] & 0x80) >> 7); 
    csbk     = ((cs_pdu[0] & 0x3F) << 8) | cs_pdu[1]; //opcode and fid combo set

    //set overarching manufacturer in use when non-standard feature id set is up
    if (csbk_fid != 0) state->dmr_mfid = csbk_fid; 

    //TIII standard with fid of 0 - (opcodes in decimal)
    if (0 == 0) //standard feature set for TIII //csbk_fid == 0
    {

      
      fprintf (stderr, "%s", KYEL); 
      
      //7.1.1.1.1 Channel Grant CSBK/MBC PDU
      if (csbk_o > 47 && csbk_o < 53 )
      {

        //users will need to import the cc frequency as channel 0 for now
        if (state->trunk_chan_map[0] != 0) state->p25_cc_freq = state->trunk_chan_map[0];

        //initial line break
        fprintf (stderr, "\n");

        long int freq = 0;

        //all of these messages share the same format (thankfully)
        if (csbk_o == 48) fprintf (stderr, " Private Voice Channel Grant (PV_GRANT)");
        if (csbk_o == 49) fprintf (stderr, " Talkgroup Voice Channel Grant (TV_GRANT)");
        if (csbk_o == 50) fprintf (stderr, " Private Broadcast Voice Channel Grant (BTV_GRANT)");
        if (csbk_o == 51) fprintf (stderr, " Private Data Channel Grant: Single Item (PD_GRANT)");
        if (csbk_o == 52) fprintf (stderr, " Talkgroup Data Channel Grant: Single Item (TD_GRANT)");

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
        uint8_t st1 = cs_pdu_bits[29]; //late_entry, hi_rate
        uint8_t st2 = cs_pdu_bits[30]; //emergency
        uint8_t st3 = cs_pdu_bits[31]; //offset, call direction
        //target and source are always the same bits
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

        fprintf (stderr, "\n");
        //added my best guess as to how dsdplus arrives at a dmr channel value (seems pretty consistent) as C+
        fprintf (stderr, "  Ch [%03X] Cd [%04d] C+ [%04d] - TS [%d] - Target [%08d] - Source [%08d]", lpchannum, lpchannum, pluschannum, lcn, target, source);

        if (st2) fprintf (stderr, " Emergency");

        if (lpchannum == 0xFFF) //This is from an MBC, signalling an absolute and not a logical
        {
          
          //7.1.1.1.2 Channel Grant Absolute Parameters CG_AP appended MBC PDU
          uint8_t mbc_lb = cs_pdu_bits[96]; //
          uint8_t mbc_pf = cs_pdu_bits[97];
          uint8_t mbc_csbko = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[98], 6);
          uint8_t mbc_res = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[104], 4);
          uint8_t mbc_cc = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[108], 4);
          uint8_t mbc_cdeftype = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[112], 4); //see 7.2.19.7 = 0 for channel parms, 1 through FFFF reserved
          uint8_t mbc_res2 = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[116], 2);
          uint64_t mbc_cdefparms = (uint64_t)ConvertBitIntoBytes(&cs_pdu_bits[118], 58); //see 7.2.19.7.1

          //this is how you read the 58 parm bits according to the appendix 7.2.19.7.1
          if (mbc_cdeftype == 0) //if 0, then absolute channel parms
          {
            uint16_t mbc_lpchannum = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[118], 12);
            uint16_t mbc_abs_tx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[130], 10);
            uint16_t mbc_abs_tx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[140], 13);
            uint16_t mbc_abs_rx_int = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[153], 10);
            uint16_t mbc_abs_rx_step = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[163], 13);

            //tx_int (Mhz) + (tx_step * 125) = tx_freq
            //rx_int (Mhz) + (rx_step * 125) = rx_freq
            fprintf (stderr, "\n");
            fprintf (stderr, "  ABS-CHAN [%03X][%04d] - RX INT [%d][%X] - RX STEP [%d][%X]", mbc_lpchannum, mbc_lpchannum, mbc_abs_rx_int, mbc_abs_rx_int, mbc_abs_rx_step, mbc_abs_rx_step );
            //DOULBE CHECK APPENDIX C - C.1.1.2 Fixed Channel Plan for accurate information (found it after figuring out calc, so might be worth double checking)
            //The Frequency we want to tune is the RX Frequency
            freq = (mbc_abs_rx_int * 1000000 ) + (mbc_abs_rx_step * 125); 
            //tx_freq = (mbc_abs_tx_int * 1000000 ) + (mbc_abs_tx_step * 125); 

          }
        }

        //print frequency from absolute
        if (freq != 0 && lpchannum == 0xFFF) fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);

        //run external channel map function on logical
        if (lpchannum != 0 && lpchannum != 0xFFF)
        {
          freq = state->trunk_chan_map[lpchannum];
          (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
        }  

        //if not a data channel grant (only tuning to voice channel grants)
        if (csbk_o < 51) //48, 49, 50 are voice grants, 51 and 52 are data grants
        {
          //shim in chan map 0 as the cc frequency, user will need to specify it in the channel map file
          if (state->p25_cc_freq != 0 && state->trunk_chan_map[0] != 0) state->p25_cc_freq = state->trunk_chan_map[0];

          //shim in here for ncurses freq display when not trunking (playback, not live)
          if (opts->p25_trunk == 0 && freq != 0)
          {
            //just set to both for now, could go on tslot later
            state->p25_vc_freq[0] = freq;
            state->p25_vc_freq[1] = freq;
          }

          //if neither current slot has vlc, pi, or voice in it currently
          //may need to fully write this out if this statement doesn't work the way I want it to
          if (state->dmrburstL != (16 || 0 || 1) && state->dmrburstR != (16 || 0 || 1) )
          {
            char mode[8]; //allow, block, digital, enc, etc

            for (int i = 0; i < state->group_tally; i++)
            {
              if (state->group_array[i].groupNumber == target)
              {
                fprintf (stderr, " [%s]", state->group_array[i].groupName);
                strcpy (mode, state->group_array[i].groupMode);
              }
            } 

            if (state->p25_cc_freq != 0 && opts->p25_trunk == 1  && (strcmp(mode, "DE") != 0)) 
            {
              if (freq != 0) //if we have a valid frequency
              {
                //RIGCTL
                if (opts->use_rigctl == 1)
                {
                  if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
                  SetFreq(opts->rigctl_sockfd, freq);
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
                  opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                }
              }

              //rtl_udp
              else if (opts->audio_in_type == 3)
              {
                rtl_udp_tune (opts, state, freq);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
                opts->p25_is_tuned = 1;
              }
            }
          }

        }
        
      }

      //Move
      if (csbk_o == 57)
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Move (C_MOVE) (TODO)");
      } 

      //Aloha 
      if (csbk_o == 25)
      {
        //initial line break
        fprintf (stderr, "\n");
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

        uint8_t model = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 2);
        uint16_t net = 0;
        uint16_t site = 0;
        char model_str[8];
        sprintf (model_str, "%s", " ");
        if (model == 0) //Tiny
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 9);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[51], 3);
          sprintf (model_str, "%s", "Tiny");
        }
        else if (model == 1) //Small
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 7);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[49], 5);
          sprintf (model_str, "%s", "Small");
        }
        else if (model == 2) //Large
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 4);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[46], 8);
          sprintf (model_str, "%s", "Large");
        }
        else if (model == 3) //Huge
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 2);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[44], 10);
          sprintf (model_str, "%s", "Huge");
        }

        uint8_t par = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[54], 2); 
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24); 
        
        fprintf (stderr, " C_ALOHA_SYS_PARMS - %s - Net ID: %d Site ID: %d Par: %d \n  Reg Req: %d V: %d MS: %d", model_str, net, site, par, regreq, version, target);

        //add string for ncurses terminal display - no par since slc doesn't carrry that value
        sprintf (state->dmr_site_parms, "TIII - %s N%d-S%d ", model_str, net, site);

        //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency
        if (opts->use_rigctl == 1 && state->p25_cc_freq == 0) //if not set from channel map 0
        {
          ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
          if (ccfreq != 0) state->p25_cc_freq = ccfreq;
        }

        uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 16);

        //debug print
        //fprintf (stderr, "\n  SYSCODE: %016b", syscode);
        //fprintf (stderr, " Sys ID Code: [%04X]", sysidcode);
      } 

      //P_CLEAR
      if (csbk_o == 46)
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Clear (P_CLEAR) (TODO)");
      } 

      //(P_PROTECT) - Could be a useful way of determining who is on the current timeslot p_kind 2
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

        if (p_kind == 0) fprintf (stderr, " Disable Target PTT (DIS_PTT)");
        if (p_kind == 1) fprintf (stderr, " Enable Target PTT (EN_PTT)");
        if (p_kind == 2) fprintf (stderr, " Private Call (ILLEGALLY_PARKED)");
        if (p_kind == 3) fprintf (stderr, " Enable Target MS PTT (EN_PTT_ONE_MS)");

        fprintf (stderr, "\n");
        fprintf (stderr, "  Target [%08d] - Source [%08d]", target, source);

        //Illegally Parked is another way to say a private call is outbound on this channel
        //so, we can use this as a form of 'link control'
        if (p_kind == 2)
        {
          //don't set this if operating out of dmr mono or using call alert beep
          if (opts->dmr_mono == 0 && opts->call_alert == 0)
          {
            if (state->currentslot == 0)
            {
              state->lasttg = target;
              state->lastsrc = source;
            }
            else 
            {
              state->lasttgR = target;
              state->lastsrcR = source;
            }
          }
          
        }

      }

      if (csbk_o == 40) //0x28 //check to see if these are fid 0 only, or also in other manufacturer fids
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " Announcements (C_BCAST)"); //site it, vote for etc etc
        uint8_t a_type = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 5);
        if (a_type == 0) fprintf (stderr, " Announce/Withdraw TSCC (Ann_WD_TSCC)");
        if (a_type == 1) fprintf (stderr, " Specify Call Timer Parameters (CallTimer_Parms)");
        if (a_type == 2) fprintf (stderr, " Vote Now Advice (Vote_Now)");
        if (a_type == 3) fprintf (stderr, " Broadcast Local Time (Local_Time)");
        if (a_type == 4) fprintf (stderr, " Mass Registration (MassReg)");
        if (a_type == 5) fprintf (stderr, " Announce Logical Channel/Frequency Relationship (Chan_Freq)");
        if (a_type == 6) fprintf (stderr, " Adjacent Site Information (Adjacent_Site)");
        if (a_type == 7) fprintf (stderr, " General Site Parameters (Gen_Site_Params)");
        if (a_type > 7 && a_type < 0x1E) fprintf (stderr, " Reserved %02X", a_type);
        if (a_type == 0x1E || a_type == 0x1F) fprintf (stderr, " Manufacturer Specific %02X", a_type);

        //sysidcode, probably would have been easier to make this its own external function
        uint8_t model = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 2);
        uint16_t net = 0;
        uint16_t site = 0;
        char model_str[8];
        sprintf (model_str, "%s", " ");

        if (model == 0) //Tiny
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 9);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[51], 3);
          sprintf (model_str, "%s", "Tiny");
        }
        else if (model == 1) //Small
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 7);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[49], 5);
          sprintf (model_str, "%s", "Small");
        }
        else if (model == 2) //Large
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 4);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[46], 8);
          sprintf (model_str, "%s", "Large");
        }
        else if (model == 3) //Huge
        {
          net  = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[42], 2);
          site = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[44], 10);
          sprintf (model_str, "%s", "Huge");
        }

        uint8_t par = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[54], 2); 
        uint32_t parms2 = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[56], 24); 
        fprintf (stderr, "\n");
        fprintf (stderr, "  C_BCAST_SYS_PARMS - %s - Net ID: %d Site ID: %d Par: %d ", model_str, net, site, par);
        uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&cs_pdu_bits[40], 16);
        //fprintf (stderr, "\n  SYSCODE: %016b", syscode);

      }

      if (csbk == 28) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_AHOY (TODO)");
      }

      if (csbk == 42) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " P_MAINT (TODO)");
      }

      if (csbk == 30) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_ACKVIT (TODO)");
      }

      if (csbk == 31) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, " C_RAND (TODO)");
      }

    }

    //Capacity+ Section
    if (csbk_fid == 0x10)
    {
      //not quite sure how these tuning rules will go over
      //if they don't work so well, may just fall back to
      //a 'follow rest channel on no sync' only approach
      if (csbk_o == 0x3E)
      {

        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        uint8_t fl = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[16], 2);
        uint8_t slot = cs_pdu_bits[18];
        uint8_t rest_channel = cs_pdu[2] & 0xF; //0xF, or 0x1F?
        uint8_t ch[8]; //one bit per channel
        uint8_t tg = 0;
        uint32_t tghex = 0; //combined all tgs for debug
        int i, j, k;

        //tg and channel info for trunking purposes
        uint8_t t_tg[9];
        memset (t_tg, 0, sizeof(t_tg));

        k = 0;
        if (rest_channel != state->dmr_rest_channel)
        {
          state->dmr_rest_channel = rest_channel; 
        }
        for (int i = 0; i < 8; i++)
        {
          ch[i] = cs_pdu_bits[i+24];
        }

        //assign to cc freq to follow during no sync
        if (state->trunk_chan_map[rest_channel] != 0)
        {
          state->p25_cc_freq = state->trunk_chan_map[rest_channel];
          //set to always tuned
          opts->p25_is_tuned = 1;
        }
        
        fprintf (stderr, " Capacity Plus Channel Status - Rest Channel %d", rest_channel);

        fprintf (stderr, "\n  ");
        for (i = 0; i < 8; i++)
        {
          fprintf (stderr, "Ch%d: ", i+1);
          if (ch[i] != 0)
          {
            tg = (uint8_t)ConvertBitIntoBytes(&cs_pdu_bits[k*8+32], 8); 
            fprintf (stderr, " %03d ", tg);
            //add values to trunking tg/channel potentials
            t_tg[i] = tg;
            k++;
            
          }
          else if (i+1 == rest_channel) fprintf (stderr, "Rest ");
          else fprintf (stderr, "Idle ");

          if (i == 3) fprintf (stderr, "\n  "); 
        }
        tghex = (uint32_t)ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
        //fprintf (stderr, "\n   TG Hex = 0x%06X", tghex);
        state->dmr_mfid = 0x10;
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf (state->dmr_branding_sub, "%s", "Cap+ ");
        fprintf (stderr, "%s", KNRM);

        //tuning logic with active tg's in active channels
        //if neither current slot has vlc, pi, or voice in it currently
        //may need to fully write this out if this statement doesn't work the way I want it to
        if (state->dmrburstL != (16 || 0 || 1) && state->dmrburstR != (16 || 0 || 1) ) 
        {
          for (j = 0; j < 8; j++) //go through the channels stored looking for active ones to tune to
          {
            char mode[8]; //allow, block, digital, enc, etc
            for (int i = 0; i < state->group_tally; i++)
            {
              if (state->group_array[i].groupNumber == t_tg[j])
              {
                fprintf (stderr, " [%s]", state->group_array[i].groupName);
                strcpy (mode, state->group_array[i].groupMode);
              }
            }

            //no more 0 reporting, that was some bad code that caused that issue
            //without priority, this will tune the first one it finds (if group isn't blocked)
            if (t_tg[j] != 0 && state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0)) 
            {
              if (state->trunk_chan_map[j+1] != 0) //if we have a valid frequency
              {
                //RIGCTL
                if (opts->use_rigctl == 1)
                {
                  if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
                  SetFreq(opts->rigctl_sockfd, state->trunk_chan_map[j+1]); 
                  state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+1];
                  opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                  j = 11; //break loop
                }
              }

              //rtl_udp
              else if (opts->audio_in_type == 3)
              {
                rtl_udp_tune (opts, state, state->trunk_chan_map[j+1]);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[j+1];
                opts->p25_is_tuned = 1;
                j = 11; //break loop
              }
            }

          }
        }
        
        
      }
    }

    //Connect+ Section
    if (csbk_fid == 0x06)
    {

      //users need to set channel 0 to their current cc frequency for now
      if (state->trunk_chan_map[0] != 0) state->p25_cc_freq = state->trunk_chan_map[0];

      if (csbk_o == 0x01)
      {
        //initial line break
        fprintf (stderr, "\n");
        uint8_t nb1 = cs_pdu[2] & 0x3F; 
        uint8_t nb2 = cs_pdu[3] & 0x3F; 
        uint8_t nb3 = cs_pdu[4] & 0x3F; 
        uint8_t nb4 = cs_pdu[5] & 0x3F; 
        uint8_t nb5 = cs_pdu[6] & 0x3F; 
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Neighbors\n");
        fprintf (stderr, "  NB1(%02d), NB2(%02d), NB3(%02d), NB4(%02d), NB5(%02d)", nb1, nb2, nb3, nb4, nb5);
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      if (csbk_o == 0x03)
      {
        
        //initial line break
        fprintf (stderr, "\n");
        uint32_t srcAddr = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        uint32_t grpAddr = ( (cs_pdu[5] << 16) + (cs_pdu[6] << 8) + cs_pdu[7] ); 
        uint8_t  lcn     = ( (cs_pdu[8] & 0xF0 ) >> 4 ); 
        uint8_t  tslot   = ( (cs_pdu[8] & 0x08 ) >> 3 );  
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Voice Channel Grant\n");
        fprintf (stderr, "  srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");

        //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency
        if (opts->use_rigctl == 1 && state->p25_cc_freq == 0) //if not set from channel map 0
        {
          ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
          if (ccfreq != 0) state->p25_cc_freq = ccfreq;
        }

        //shim in here for ncurses freq display when not trunking (playback, not live)
        if (opts->p25_trunk == 0 && state->trunk_chan_map[lcn] != 0)
        {
          //just set to both for now, could go on tslot later
          state->p25_vc_freq[0] = state->trunk_chan_map[lcn];
          state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
        }

        char mode[8]; //allow, block, digital, enc, etc

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == grpAddr)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
          }
        }

        //voice calls can occur on the con+ control channel, so we need the same check for voice as we do for cap+
        //if neither current slot has vlc, pi, or voice in it currently
        //may need to fully write this out if this statement doesn't work the way I want it to
        if (state->dmrburstL != (16 || 0 || 1) && state->dmrburstR != (16 || 0 || 1) )
        {
          //need channel map frequencies and stuff, also way to figure out control channel frequency? (channel 0 from channel map?)
          if (state->p25_cc_freq != 0 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0)) 
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
              }
            }

            //rtl_udp
            else if (opts->audio_in_type == 3)
            {
              rtl_udp_tune (opts, state, state->trunk_chan_map[lcn]);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
              opts->p25_is_tuned = 1;
              state->is_con_plus = 1; //flag on
            }
          }
        }  

      }

      if (csbk_o == 0x06) 
      {
        //initial line break
        fprintf (stderr, "\n");
        uint32_t srcAddr = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        uint32_t grpAddr = ( (cs_pdu[5] << 16) + (cs_pdu[6] << 8) + cs_pdu[7] ); 
        uint8_t  lcn     = ( (cs_pdu[8] & 0xF0 ) >> 4 ) ; 
        uint8_t  tslot   = ( (cs_pdu[8] & 0x08 ) >> 3 ); 
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Data Channel Grant\n"); 
        fprintf (stderr, "  srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      if (csbk_o == 0x0C) 
      {
        //initial line break
        fprintf (stderr, "\n");
        uint32_t srcAddr = ( (cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4] ); 
        uint32_t grpAddr = ( (cs_pdu[5] << 16) + (cs_pdu[6] << 8) + cs_pdu[7] ); 
        uint8_t  lcn     = ( (cs_pdu[8] & 0xF0 ) >> 4 ); 
        uint8_t  tslot   = ( (cs_pdu[8] & 0x08 ) >> 3 ); 
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Terminate Channel Grant\n"); 
        fprintf (stderr, "  srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      if (csbk_o == 0x11) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Registration Request");
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      if (csbk_o == 0x12) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Registration Response");
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      if (csbk_o == 0x18) 
      {
        //initial line break
        fprintf (stderr, "\n");
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Connect Plus Talkgroup Affiliation");
        state->dmr_mfid = 0x06; 
        sprintf (state->dmr_branding, "%s", "Motorola");
        sprintf(state->dmr_branding_sub, "Con+ ");
      }

      fprintf (stderr, "%s", KNRM);

    } 

  }
  END:
  fprintf (stderr, "%s", KNRM);  

}