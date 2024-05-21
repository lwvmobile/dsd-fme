/*-------------------------------------------------------------------------------
 * dmr_flco.c
 * DMR Full Link Control, Short Link Control, TACT/CACH and related funtions
 *
 * Portions of link control/voice burst/vlc/tlc from LouisErigHerve
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//combined flco handler (vlc, tlc, emb), minus the superfluous structs and strings
void dmr_flco (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors, uint8_t type)
{
  UNUSED(CRCCorrect);

  //force slot to 0 if using dmr mono handling
  if (opts->dmr_mono == 1) state->currentslot = 0;

  uint8_t pf = 0;
  uint8_t reserved = 0;
  uint8_t flco = 0;
  uint8_t fid = 0;
  uint8_t so = 0;
  uint32_t target = 0;
  uint32_t source = 0;
  uint8_t capsite = 0; 
  int restchannel = -1;
  int is_cap_plus = 0;
  int is_alias = 0;
  int is_gps = 0;
  UNUSED(capsite);

  //XPT 'Things'
  int is_xpt = 0;
  uint8_t xpt_hand = 0;  //handshake
  uint8_t xpt_free = 0;  //free repeater
  uint8_t xpt_int  = 0;  //xpt channel to interrupt (channel/repeater call should occur on?)
  uint8_t xpt_res_a = 0; //unknown values of other bits of the XPT LC
  uint8_t xpt_res_b = 0; //unknown values of other bits of the XPT LC
  uint8_t xpt_res_c = 0; //unknown values of other bits of the XPT LC
  uint8_t target_hash[24]; //for XPT (and others if desired, get the hash and compare against SLC or XPT Status CSBKs)
  uint8_t tg_hash = 0; //value of the hashed TG
  UNUSED3(xpt_res_a, xpt_res_b, xpt_res_c);

  uint8_t slot = state->currentslot;
  uint8_t unk = 0; //flag for unknown FLCO + FID combo

  pf = (uint8_t)(lc_bits[0]); //Protect Flag -- Hytera XPT uses this to signify which TS the PDU is on
  reserved = (uint8_t)(lc_bits[1]); //Reserved -- Hytera XPT G/I bit; 0 - Individual; 1 - Group;
  flco = (uint8_t)ConvertBitIntoBytes(&lc_bits[2], 6); //Full Link Control Opcode
  fid = (uint8_t)ConvertBitIntoBytes(&lc_bits[8], 8); //Feature set ID (FID)
  so = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 8); //Service Options
  target = (uint32_t)ConvertBitIntoBytes(&lc_bits[24], 24); //Target or Talk Group
  source = (uint32_t)ConvertBitIntoBytes(&lc_bits[48], 24);

  //read ahead a little to get this for the xpt flag
  if (IrrecoverableErrors == 0 && flco == 0x09 && fid == 0x68)
  {
    sprintf (state->dmr_branding, "%s", "  Hytera"); 
    sprintf (state->dmr_branding_sub, "XPT ");
  }

  //look at the dmr_branding_sub for the XPT string
  //branding sub is set at CSBK(68-3A and 3B), SLCO 8, and here on 0x09
  if (strcmp (state->dmr_branding_sub, "XPT ") == 0) is_xpt = 1;

  //hytera XPT -- disable the pf flag, is used for TS value in some Hytera XPT PDUs
  if (is_xpt) pf = 0;

  //check protect flag
  if (pf == 1)
  {
    if (type == 1) fprintf (stderr, "%s \n", KRED);
    if (type == 2) fprintf (stderr, "%s \n", KRED);
    if (type == 3) fprintf (stderr, "%s", KRED);
    fprintf (stderr, " SLOT %d", state->currentslot+1);
    fprintf (stderr, " Protected LC ");
    goto END_FLCO; 
  } 

  if (IrrecoverableErrors == 0)
  {
    
    if (slot == 0) state->dmr_flco = flco;
    else state->dmr_flcoR = flco;

    //FID 0x10 and FLCO 0x14, 0x15, 0x16 and 0x17 confirmed as Moto EMB Alias
    //Can probably assume FID 0x10 FLCO 0x18 is Moto EMB GPS -- to be tested
    if ( fid == 0x10 && (flco == 0x14 || flco == 0x15 || flco == 0x16 || flco == 0x17 || flco == 0x18) )
    {
      flco = flco - 0x10;
      fid = 0;
    }

    //Embedded Talker Alias Header Only (format and len storage)
    if ( (fid == 0 || fid == 0x68) && type == 3 && flco == 0x04) 
    {
      dmr_embedded_alias_header(opts, state, lc_bits);
    }

    //Embedded Talker Alias Header (continuation) and Blocks
    if ( (fid == 0 || fid == 0x68) && type == 3 && flco > 0x03 && flco < 0x08) 
    {
      is_alias = 1;
      dmr_embedded_alias_blocks(opts, state, lc_bits);
    }

    //Embedded GPS
    if ( (fid == 0 || fid == 0x68) && fid == 0 && type == 3 && flco == 0x08)
    {
      is_gps = 1;
      dmr_embedded_gps(opts, state, lc_bits);
    }

    //look for Cap+ on VLC header, then set source and/or rest channel appropriately
    if (type == 1 && fid == 0x10 && (flco == 0x04 || flco == 0x07) ) //0x07 appears to be a cap+ txi private call
    {
      is_cap_plus = 1;
      capsite = (uint8_t)ConvertBitIntoBytes(&lc_bits[48], 4); //don't believe so
      restchannel = (int)ConvertBitIntoBytes(&lc_bits[52], 4); //
      source = (uint32_t)ConvertBitIntoBytes(&lc_bits[56], 16);
    }

    //Unknown CapMax/Moto Things
    if (fid == 0x10 && (flco == 0x08 || flco == 0x28)) 
    {
      //NOTE: fid 0x10 and flco 0x08 (emb) produces a lot of 'zero' bytes
      //this has been observed to happen often on CapMax systems, so I believe it could be some CapMax 'thing'
      //Unknown Link Control - FLCO=0x08 FID=0x10 SVC=0xC1 or FLCO=0x08 FID=0x10 SVC=0xC0 <- probably no SVC bits in the lc
      //flco 0x28 has also been observed lately but the tg and src values don't match
      //another flco 0x10 does seem to match, so is probably capmax group call flco
      if (type == 1) fprintf (stderr, "%s \n", KCYN);
      if (type == 2) fprintf (stderr, "%s \n", KCYN);
      if (type == 3) fprintf (stderr, "%s", KCYN);
      fprintf (stderr, " SLOT %d", state->currentslot+1);
      fprintf (stderr, " Motorola");
      unk = 1;
      goto END_FLCO;
    }

    //7.1.1.1 Terminator Data Link Control PDU - ETSI TS 102 361-3 V1.2.1 (2013-07)
    if (type == 2 && flco == 0x30)
    {
      fprintf (stderr, "%s \n", KRED);
      fprintf (stderr, " SLOT %d", state->currentslot+1);
      fprintf (stderr, " Data Terminator (TD_LC) ");
      fprintf (stderr, "%s", KNRM);

      //reset data header format storage
      state->data_header_format[slot] = 7;
      //flag off data header validity 
      state->data_header_valid[slot] = 0; 
      //flag off conf data flag
      state->data_conf_data[slot] = 0;
      
      goto END_FLCO;
    }

    //Unknown Tait Things
    if (fid == 0x58)
    {
      //NOTE: fid 0x58 (tait) had a single flco 0x06 emb observed, but without the other blocks (4,5,7) for an alias
      //will need to observe this one, or just remove it from the list, going to isolate tait lc for now
      if (type == 1) fprintf (stderr, "%s \n", KCYN);
      if (type == 2) fprintf (stderr, "%s \n", KCYN);
      if (type == 3) fprintf (stderr, "%s", KCYN);
      fprintf (stderr, " SLOT %d", state->currentslot+1);
      fprintf (stderr, " Tait");
      unk = 1;
      goto END_FLCO;
    }

    //look for any Hytera XPT system, adjust TG to 16-bit allocation 
    //Groups use only 8 out of 16, but 16 always seems to be allocated
    //private calls use 16-bit target values hashed to 8-bit in the site status csbk
    //the TLC preceeds the VLC for a 'handshake' call setup in XPT

    //truncate if XPT is set
    if (is_xpt == 1)
    {
      target = (uint32_t)ConvertBitIntoBytes(&lc_bits[32], 16); //16-bit allocation
      source = (uint32_t)ConvertBitIntoBytes(&lc_bits[56], 16); //16-bit allocation

      //the crc8 hash is the value represented in the CSBK when dealing with private calls
      for (int i = 0; i < 16; i++) target_hash[i] = lc_bits[32+i];
      tg_hash = crc8 (target_hash, 16); 
    } 

    //XPT Call 'Grant/Alert' Setup Occurs in TLC with a flco 0x09 
    if (fid == 0x68 && flco == 0x09)
    {
      //The CSBK always uses an 8-bit TG/TGT; The White Papers (user manuals) say 8-bit TG and 16-bit SRC addressing
      //private calls and indiv data calls use a hash of their 8 bit tgt values in the CSBK
      xpt_int = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 4); //This is consistent with an LCN value, not an LSN value
      xpt_free = (uint8_t)ConvertBitIntoBytes(&lc_bits[24], 4); //24 and 4 on 0x09
      xpt_hand = (uint8_t)ConvertBitIntoBytes(&lc_bits[28], 4); //handshake kind: 0 - ordinary; 1-2 Interrupts; 3-15 reserved; 

      target = (uint32_t)ConvertBitIntoBytes(&lc_bits[32], 16); //16-bit allocation
      source = (uint32_t)ConvertBitIntoBytes(&lc_bits[56], 16); //16-bit allocation

      //the bits that are left behind
      xpt_res_a = (uint8_t)ConvertBitIntoBytes(&lc_bits[20], 4); //after the xpt_int channel, before the free repeater channel -- call being established = 7; call connected = 0; ??
      xpt_res_b = (uint8_t)ConvertBitIntoBytes(&lc_bits[48], 8); //where the first 8 bits of the SRC would be

      //the crc8 hash is the value represented in the CSBK when dealing with private calls
      for (int i = 0; i < 16; i++) target_hash[i] = lc_bits[32+i];
      tg_hash = crc8 (target_hash, 16); 

      fprintf (stderr, "%s \n", KGRN);
      fprintf (stderr, " SLOT %d ", state->currentslot+1);
      if (opts->payload == 1) fprintf(stderr, "FLCO=0x%02X FID=0x%02X ", flco, fid); 
      fprintf (stderr, "TGT=%u SRC=%u ", target, source);
      
      fprintf (stderr, "Hytera XPT ");

      //Group ID ranges from 1 to 240; emergency group call ID ranges from 250 to 254; all call ID is 255.
      if (reserved == 1)
      {
        fprintf (stderr, "Group "); //according to observation
        if (target > 248 && target < 255) fprintf (stderr, "Emergency ");
        if (target == 255) fprintf (stderr, "All ");
      } 
      else fprintf (stderr, "Private "); //according to observation
      fprintf (stderr, "Call Alert "); //Alert or Grant

      //reorganized all the 'extra' data to a second line and added extra verbosity
      if (opts->payload == 1) fprintf (stderr, "\n  ");
      if (opts->payload == 1) fprintf (stderr, "%s", KYEL);
      //only display the hashed tgt value if its a private call and not a group call
      if (reserved == 0 && opts->payload == 1) fprintf(stderr, "TGT Hash=%d; ", tg_hash);
      if (opts->payload == 1) fprintf(stderr, "HSK=%X; ", xpt_hand);
      //extra verbosity on handshake types found in the patent
      if (opts->payload == 1) 
      {
        fprintf (stderr, "Handshake - ");
        if (xpt_hand == 0) fprintf (stderr, "Ordinary; ");
        else if (xpt_hand == 1) fprintf (stderr, "Callback/Alarm Interrupt; ");
        else if (xpt_hand == 2) fprintf (stderr, "Release Channel Interrupt; ");
        else fprintf (stderr, "Reserved; ");
      }

      // if (opts->payload == 1)
      // {
      //   if (xpt_res_a == 0) fprintf (stderr, "Call Connected; ");
      //   if (xpt_res_a == 1) fprintf (stderr, "Data Call Request; ");
      //   if (xpt_res_a == 7) fprintf (stderr, "Voice Call Request; ");
      //   if (xpt_res_a == 2) fprintf (stderr, "Unknown Status; ");
      // }

      //logical repeater channel, not the logical slot value in the CSBK
      if (opts->payload == 1) fprintf(stderr, "Call on LCN %d; ", xpt_int); //LCN channel call or 'interrupt' will occur on
      // if (opts->payload == 1) fprintf(stderr, "RS A[%01X]B[%02X]C[%02X]; ", xpt_res_a, xpt_res_b, xpt_res_c); //leftover bits
      if (opts->payload == 1) fprintf (stderr, "Free LCN %d; ", xpt_free); //current free repeater LCN channel
      fprintf (stderr, "%s ", KNRM);

      //add string for ncurses terminal display
      sprintf (state->dmr_site_parms, "Free LCN - %d ", xpt_free);

      is_xpt = 1;
      goto END_FLCO;
    }

    //Hytera XPT 'Others' -- moved the patent opcodes here as well for now
    if ( fid == 0x68 && (flco == 0x13 || flco == 0x31 || flco == 0x2E || flco == 0x2F) )
    {
      if (type == 1) fprintf (stderr, "%s \n", KCYN);
      if (type == 2) fprintf (stderr, "%s \n", KCYN);
      if (type == 3) fprintf (stderr, "%s", KCYN);
      fprintf (stderr, " SLOT %d", state->currentslot+1);
      fprintf (stderr, " Hytera ");
      unk = 1;
      goto END_FLCO;
    }

    //unknown other manufacturer or OTA ENC, etc.
    //removed tait from the list, added hytera 0x08
    if (fid != 0 && fid != 0x68 && fid != 0x10 && fid != 0x08) 
    {
      if (type == 1) fprintf (stderr, "%s \n", KYEL);
      if (type == 2) fprintf (stderr, "%s \n", KYEL);
      if (type == 3) fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " SLOT %d", state->currentslot+1);
      fprintf (stderr, " Unknown LC ");
      unk = 1;
      goto END_FLCO;
    }

  }  

  //will want to continue to observe for different flco and fid combinations to find out their meaning
  if(IrrecoverableErrors == 0 && is_alias == 0 && is_gps == 0) 
  {
    //set overarching manufacturer in use when non-standard feature id set is up
    if (fid != 0) state->dmr_mfid = fid;

    if (type != 2) //VLC and EMB, set our target, source, so, and fid per channel
    {
      if (state->currentslot == 0)
      {
        state->dmr_fid = fid;
        state->dmr_so = so;
        state->lasttg = target;
        state->lastsrc = source;
      }
      if (state->currentslot == 1)
      {
        state->dmr_fidR = fid;
        state->dmr_soR = so;
        state->lasttgR = target;
        state->lastsrcR = source;
      }

      //update cc amd vc sync time for trunking purposes (particularly Con+)
      if (opts->p25_is_tuned == 1)
      {
        state->last_vc_sync_time = time(NULL);
        state->last_cc_sync_time = time(NULL);
      } 
      
    }

    if (type == 2) //TLC, zero out target, source, so, and fid per channel, and other odd and ends
    {
      //I wonder which of these we truly want to zero out, possibly none of them
      if (state->currentslot == 0)
      {
        state->dmr_fid = 0;
        state->dmr_so = 0;
        // state->lasttg = 0;
        // state->lastsrc = 0;
        state->payload_algid = 0;
        state->payload_mi = 0;
        state->payload_keyid = 0;
        //reset gain
        if (opts->floating_point == 1)
          state->aout_gain = opts->audio_gain;
      }
      if (state->currentslot == 1)
      {
        state->dmr_fidR = 0;
        state->dmr_soR = 0;
        // state->lasttgR = 0;
        // state->lastsrcR = 0;
        state->payload_algidR = 0;
        state->payload_miR = 0;
        state->payload_keyidR = 0;
        //reset gain
        if (opts->floating_point == 1)
          state->aout_gainR = opts->audio_gain; 
      }
      
    }
    
    //only assign this value here if not trunking
    // if (opts->p25_trunk == 0) //may be safe to always do this now with code changes, will want to test at some point (tg hold w/ dual voice / slco may optimally need this set)
    {
      if (restchannel != state->dmr_rest_channel && restchannel != -1)
      {
        state->dmr_rest_channel = restchannel;
        //assign to cc freq
        // if (state->trunk_chan_map[restchannel] != 0)
        // {
        //   state->p25_cc_freq = state->trunk_chan_map[restchannel];
        // } 
      }
    }

    if (type == 1) fprintf (stderr, "%s \n", KGRN);
    if (type == 2) fprintf (stderr, "%s \n", KRED);
    if (type == 3) fprintf (stderr, "%s", KGRN);
    
    fprintf (stderr, " SLOT %d ", state->currentslot+1);
    fprintf(stderr, "TGT=%u SRC=%u ", target, source);
    if (opts->payload == 1 && is_xpt == 1 && flco == 0x3) fprintf(stderr, "HASH=%d ", tg_hash);
    if (opts->payload == 1) fprintf(stderr, "FLCO=0x%02X FID=0x%02X SVC=0x%02X ", flco, fid, so);

    //0x04 and 0x05 on a TLC seem to indicate a Cap + Private Call Terminator (perhaps one for each MS)
    //0x07 on a VLC seems to indicate a Cap+ Private Call Header
    //0x23 on the Embedded Voice Burst Sync seems to indicate a Cap+ or Cap+ TXI Private Call in progress
    //0x20 on the Embedded Voice Burst Sync seems to indicate a Moto (non-specific) Group Call in progress
    //its possible that both EMB FID 0x10 FLCO 0x20 and 0x23 are just Moto but non-specific (observed 0x20 on Tier 2)

    if (fid == 0x68) sprintf (state->call_string[slot], " Hytera  "); 

    else if (flco == 0x4 || flco == 0x5 || flco == 0x7 || flco == 0x23) //Cap+ Things
    {
      // sprintf (state->call_string[slot], " Cap+");
      sprintf (state->call_string[slot], "%s","");
      fprintf (stderr, "Cap+ ");
      if (flco == 0x4)
      {
        // strcat (state->call_string[slot], " Grp");
        sprintf (state->call_string[slot], "   Group ");
        fprintf (stderr, "Group ");
      }
      else
      {
        // strcat (state->call_string[slot], " Pri");
        sprintf (state->call_string[slot], " Private ");
        fprintf (stderr, "Private ");
      } 
    }
    else if (flco == 0x3) //UU_V_Ch_Usr
    {
      sprintf (state->call_string[slot], " Private ");
      fprintf (stderr, "Private "); 
    }
    else //Grp_V_Ch_Usr -- still valid on hytera VLC
    {
      sprintf (state->call_string[slot], "   Group ");
      fprintf (stderr, "Group "); 
    } 

    if(so & 0x80)
    {
      strcat (state->call_string[slot], " Emergency  ");
      fprintf (stderr, "%s", KRED);
      fprintf(stderr, "Emergency ");
    }
    else strcat (state->call_string[slot], "            ");

    if(so & 0x40)
    {
      //REMUS! Uncomment Line Below if desired
      // strcat (state->call_string[slot], " Encrypted");
      fprintf (stderr, "%s", KRED);
      fprintf(stderr, "Encrypted ");

      //experimental TG LO/B if ENC trunked following disabled //DMR -- LO Trunked Enc Calls WIP; #121
      if (opts->p25_trunk == 1 && opts->trunk_tune_enc_calls == 0) //&& type != 2
      {
        int i, lo = 0;
        uint32_t t = 0; char gm[8]; char gn[50];

        //check to see if this group already exists, or has already been locked out, or is allowed
        for (i = 0; i <= state->group_tally; i++)
        {
          t = (uint32_t)state->group_array[i].groupNumber;
          if (target == t && t != 0)
          {
            lo = 1;
            //write current mode and name to temp strings
            sprintf (gm, "%s", state->group_array[i].groupMode);
            sprintf (gn, "%s", state->group_array[i].groupName);
            break;
          }
        }

        //if group doesn't exist, or isn't locked out, then do so now.
        if (lo == 0)
        { //changing from DE to B to fit the rest of the lockout logic ("Buzzer Fix")
          state->group_array[state->group_tally].groupNumber = target;
          sprintf (state->group_array[state->group_tally].groupMode, "%s", "B");
          sprintf (state->group_array[state->group_tally].groupName, "%s", "ENC LO");
          sprintf (gm, "%s", "B");
          sprintf (gn, "%s", "ENC LO");
          state->group_tally++;
        }

        //Craft a fake CSBK pdu send it to run as a p_clear to return to CC if available
        uint8_t dummy[12]; uint8_t* dbits; memset (dummy, 0, sizeof(dummy)); dummy[0] = 46; dummy[1] = 255;
        if ( (strcmp(gm, "B") == 0) && (strcmp(gn, "ENC LO") == 0)  ) //&& (opts->trunk_tune_data_calls == 0)
          dmr_cspdu (opts, state, dbits, dummy, 1, 0);

      }
      
    }
    //REMUS! Uncomment Line Below if desired
    // else strcat (state->call_string[slot], "          ");

    
    if (1 == 1) //fid == 0x10
    {
      /* Check the "Service Option" bits */ 
      if( (fid == 0x10) && (so & 0x20) ) //Motorola FID 0x10 Only
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], " TXI");
        fprintf(stderr, "TXI ");
      }
      if ( (fid == 0x10) && (so & 0x10) ) //Motorola FID 0x10 Only
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], " RPT");
        fprintf(stderr, "RPT "); //Short way of saying the next SF's VC6 will be pre-empted/repeat frames for the TXI backwards channel
      }
      if(so & 0x08)
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], "-BC   ");
        fprintf(stderr, "Broadcast ");
      } 
      if(so & 0x04)
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], "-OVCM ");
        fprintf(stderr, "OVCM ");
      } 
      if(so & 0x03)
      {
        if((so & 0x03) == 0x01)
        {
          //REMUS! Uncomment Line Below if desired
          // strcat (state->call_string[slot], "-P1");
          fprintf(stderr, "Priority 1 ");
        } 
        else if((so & 0x03) == 0x02)
        {
          //REMUS! Uncomment Line Below if desired
          // strcat (state->call_string[slot], "-P2");
          fprintf(stderr, "Priority 2 ");
        } 
        else if((so & 0x03) == 0x03)
        {
          //REMUS! Uncomment Line Below if desired
          // strcat (state->call_string[slot], "-P3");
          fprintf(stderr, "Priority 3 ");
        } 
        else /* We should never go here */
        {
          //REMUS! Uncomment Line Below if desired
          // strcat (state->call_string[slot], "  ");
          fprintf(stderr, "No Priority "); 
        } 
      }
    }
    
    //should rework this back into the upper portion
    if (fid == 0x68) fprintf (stderr, "Hytera ");
    if (is_xpt) fprintf (stderr, "XPT ");
    if (fid == 0x68 && flco == 0x00) fprintf (stderr, "Group ");
    if (fid == 0x68 && flco == 0x03) fprintf (stderr, "Private ");

    fprintf(stderr, "Call ");
    

    //check Cap+ rest channel info if available and good fec
    if (is_cap_plus == 1)
    {
      if (restchannel != -1)
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "Rest LSN: %d", restchannel);
      }
    }
  
    fprintf (stderr, "%s ", KNRM);

    //group labels
    for (int i = 0; i < state->group_tally; i++)
    {
      //Remus! Change target to source if you prefer
      if (state->group_array[i].groupNumber == target)
      {
        fprintf (stderr, "%s", KCYN);
        fprintf (stderr, "[%s] ", state->group_array[i].groupName);
        fprintf (stderr, "%s", KNRM);
      }
    }

    //BUGFIX: Include slot and algid so we don't accidentally print more than one loaded key
    //this can happen on Missing PI header and LE when the keyloader has loaded a TG/Hash key and an RC4 key simultandeously
    //subsequennt EMB would print two key values until call cleared out

    if (state->K != 0 && fid == 0x10 && so & 0x40 && slot == 0 && state->payload_algid == 0)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %lld ", state->K);
      fprintf (stderr, "%s ", KNRM);
    }

    if (state->K != 0 && fid == 0x10 && so & 0x40 && slot == 1 && state->payload_algidR == 0)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %lld ", state->K);
      fprintf (stderr, "%s ", KNRM);
    } 

    if (state->K1 != 0 && fid == 0x68 && so & 0x40 && slot == 0 && state->payload_algid == 0)
    {
      if (state->K2 != 0) fprintf (stderr, "\n ");
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %010llX ", state->K1);
      if (state->K2 != 0) fprintf (stderr, "%016llX ", state->K2);
      if (state->K4 != 0) fprintf (stderr, "%016llX %016llX", state->K3, state->K4);
      fprintf (stderr, "%s ", KNRM);
    }

    if (state->K1 != 0 && fid == 0x68 && so & 0x40 && slot == 1 && state->payload_algidR == 0)
    {
      if (state->K2 != 0) fprintf (stderr, "\n ");
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %010llX ", state->K1);
      if (state->K2 != 0) fprintf (stderr, "%016llX ", state->K2);
      if (state->K4 != 0) fprintf (stderr, "%016llX %016llX", state->K3, state->K4);
      fprintf (stderr, "%s ", KNRM);
    }

    if (slot == 0 && state->payload_algid == 0x21 && state->R != 0)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %010llX ", state->R);
      fprintf (stderr, "%s ", KNRM);
    }

    if (slot == 1 && state->payload_algidR == 0x21 && state->RR != 0)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %010llX ", state->RR);
      fprintf (stderr, "%s ", KNRM);
    }
    
  }

  END_FLCO:
  
  //blank the call string here if its a TLC
  if (type == 2) sprintf (state->call_string[slot], "%s", "                     "); //21 spaces

  if (unk == 1 || pf == 1)
  {
    fprintf(stderr, " FLCO=0x%02X FID=0x%02X ", flco, fid);
    fprintf (stderr, "%s", KNRM);
  }

  if(IrrecoverableErrors != 0)
  {
    if (type != 3) fprintf (stderr, "\n");
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, " SLOT %d", state->currentslot+1);
    fprintf (stderr, " FLCO FEC ERR ");
    fprintf (stderr, "%s", KNRM);
  }


}

//externalized dmr cach - tact and slco fragment handling
uint8_t dmr_cach (dsd_opts * opts, dsd_state * state, uint8_t cach_bits[25]) 
{
  int i, j;
  uint8_t err = 0;
  uint8_t tact_valid = 0; 
  UNUSED(tact_valid);

  bool h1, h2, h3, crc; 

  //dump payload - testing
  uint8_t slco_raw_bits[68]; //raw
  uint8_t slco_bits[68]; //de-interleaved

  //tact pdu
  uint8_t tact_bits[7];
  uint8_t at = 0; //access type, set to 1 during continuous transmission mode 
  uint8_t slot = 2; //tdma time slot
  uint8_t lcss = 0; //link control start stop (9.3.3) NOTE: There is no Single fragment LC defined for CACH signalling
  UNUSED2(at, slot);

  //cach_bits are already de-interleaved upon initial collection (still needs secodary slco de-interleave)
  for (i = 0; i < 7; i++)
  {
    tact_bits[i] = cach_bits[i];
  }

  //run hamming 7_4 on the tact_bits (redundant, since we do it earlier, but need the lcss)
  if ( Hamming_7_4_decode (tact_bits) )
  {
    at = tact_bits[0]; //any useful tricks with this? csbk on/off etc?
    slot = tact_bits[1]; //
    lcss = (tact_bits[2] << 1) | tact_bits[3];
    tact_valid = 1; //set to 1 for valid, else remains 0.
    //fprintf (stderr, "AT=%d LCSS=%d - ", at, lcss); //debug print
  }
  else //probably skip/memset/zeroes with else statement?
  {
    //do something?
    err = 1;
    //return (err);
  }

  //determine counter value based on lcss value
  if (lcss == 0) //run as single block/fragment NOTE: There is no Single fragment LC defined for CACH signalling (but is mentioned in the manual table)
  {
    //reset the full cach, even if we aren't going to use it, may be beneficial for next time
    state->dmr_cach_counter = 0; //first fragment, set to zero.
    memset (state->dmr_cach_fragment, 1, sizeof (state->dmr_cach_fragment));

    //seperate
    for (i = 0; i < 17; i++)
    {
      slco_raw_bits[i] = cach_bits[i+7];
    }
    
    //De-interleave
    int src = 0;
    for (i = 0; i < 17; i++)
    {
		  src = (i * 4) % 67;
		  slco_bits[i] = slco_raw_bits[src]; 
	  }
	  //slco_bits[i] = slco_raw_bits[i];

    //hamming check here
    h1 = Hamming17123 (slco_bits + 0);

    //run single - manual would suggest that though this exists, there is no support? or perhaps its manufacturer only thing?
    if (h1) ; // dmr_slco (opts, state, slco_bits); //random false positives and sets bad parms/mfid etc
    else
    {
      err = 1;
      //return (err);
    }
    return (err);
  }

  if (lcss == 1) //first block, reset counters and memset
  {
    //reset the full cach and counter
    state->dmr_cach_counter = 0; 
    memset (state->dmr_cach_fragment, 1, sizeof (state->dmr_cach_fragment));
  } 
  if (lcss == 3) state->dmr_cach_counter++; //continuation, so increment counter by one.
  if (lcss == 2) //final segment - assemble, de-interleave, hamming, crc, and execute
  {
    state->dmr_cach_counter = 3; 
  } 

  //sanity check
  if (state->dmr_cach_counter > 3) //marginal/shaky/bad signal or tuned away
  {
    //zero out complete fragment array
    lcss = 5; //toss away value
    state->dmr_cach_counter = 0;
    memset (state->dmr_cach_fragment, 1, sizeof (state->dmr_cach_fragment));
    err = 1; 
    return (err);
  }

  //add fragment to array
  for (i = 0; i < 17; i++)
  {
    state->dmr_cach_fragment[state->dmr_cach_counter][i] = cach_bits[i+7];
  }

  if (lcss == 2) //last block arrived, compile, hamming, crc and send off to dmr_slco
  {
    //assemble
    for (j = 0; j < 4; j++)
    {
      for (i = 0; i < 17; i++)
      {
        slco_raw_bits[i+(17*j)] = state->dmr_cach_fragment[j][i];
      }
    }

    //De-interleave method, hamming, and crc from Boatbod OP25

    //De-interleave
    int src = 0;
    for (i = 0; i < 67; i++)
    {
		  src = (i * 4) % 67;
		  slco_bits[i] = slco_raw_bits[src]; 
	  }
	  slco_bits[i] = slco_raw_bits[i];

    //hamming checks here
    h1 = Hamming17123 (slco_bits + 0);
    h2 = Hamming17123 (slco_bits + 17);
    h3 = Hamming17123 (slco_bits + 34);

    // remove hamming and leave 36 bits of Short LC
    for (i = 17; i < 29; i++) {
      slco_bits[i-5] = slco_bits[i];
    }
    for (i = 34; i < 46; i++) {
      slco_bits[i-10] = slco_bits[i];
    }

    //zero out the hangover bits
    for (i = 36; i < 68; i++)
    {
      slco_bits[i] = 0;
    }

    //run crc8
    crc = crc8_ok(slco_bits, 36);

    //only run SLCO on good everything
    if (h1 && h2 && h3 && crc) dmr_slco (opts, state, slco_bits);
    else
    {
      //this line break issue is wracking on my OCD for clean line breaks
      if (opts->payload == 1 && state->dmrburstL == 16 && state->currentslot == 0) ; //no line break if current slot is voice with payload enabled
      else if (opts->payload == 1 && state->dmrburstR == 16 && state->currentslot == 1) ; //no line break if current slot is voice with payload enabled
      else fprintf (stderr, "\n");
      fprintf (stderr, "%s", KRED);
      fprintf (stderr, " SLCO CRC ERR");
      fprintf (stderr, "%s", KNRM);
      if (opts->payload == 1 && state->dmrburstL == 16 && state->currentslot == 0) //if current slot is voice with payload enabled
        fprintf (stderr, "\n");
      else if (opts->payload == 1 && state->dmrburstR == 16 && state->currentslot == 1) //if current slot is voice with payload enabled
        fprintf (stderr, "\n");
    } 
    
  }
  return (err); //return err value based on success or failure, even if we aren't checking it
}

void dmr_slco (dsd_opts * opts, dsd_state * state, uint8_t slco_bits[])
{
  long int ccfreq = 0;

  int i;
  uint8_t slco_bytes[6]; //completed byte blocks for payload print
  for (i = 0; i < 5; i++) slco_bytes[i] = (uint8_t)ConvertBitIntoBytes(&slco_bits[i*8], 8);
  slco_bytes[5] = (uint8_t)ConvertBitIntoBytes(&slco_bits[32], 4);
  

  //just going to decode the Short LC with all potential parameters known
  uint8_t slco = (uint8_t)ConvertBitIntoBytes(&slco_bits[0], 4);
  uint8_t model = (uint8_t)ConvertBitIntoBytes(&slco_bits[4], 2);
  uint16_t netsite = (uint16_t)ConvertBitIntoBytes(&slco_bits[6], 12);
  uint8_t reg = slco_bits[18]; //registration required/not required or normalchanneltype/composite cch
  uint8_t csc = (uint16_t)ConvertBitIntoBytes(&slco_bits[19], 9); //common slot counter, 0-511
  UNUSED(netsite);

  uint16_t net = 0;
	uint16_t site = 0;
  char model_str[8];
  sprintf (model_str, "%s", "");
  //activity update stuff
  uint8_t ts1_act = (uint8_t)ConvertBitIntoBytes(&slco_bits[4], 4); //activity update ts1
  uint8_t ts2_act = (uint8_t)ConvertBitIntoBytes(&slco_bits[8], 4); //activity update ts2
  uint8_t ts1_hash = (uint8_t)ConvertBitIntoBytes(&slco_bits[12], 8); //ts1 address hash (crc8) //361-1 B.3.7
  uint8_t ts2_hash = (uint8_t)ConvertBitIntoBytes(&slco_bits[20], 8); //ts2 address hash (crc8) //361-1 B.3.7

  char ts1_str[25]; sprintf (ts1_str, "%s", "");
  char ts2_str[25]; sprintf (ts2_str, "%s", "");

  if      (ts1_act == 0b0000) sprintf (ts1_str, "%s", "Idle");
  else if (ts1_act == 0b0010) sprintf (ts1_str, "%s", "Group CSBK");
  else if (ts1_act == 0b0011) sprintf (ts1_str, "%s", "Ind CSBK");
  else if (ts1_act == 0b1000) sprintf (ts1_str, "%s", "Group Voice");
  else if (ts1_act == 0b1001) sprintf (ts1_str, "%s", "Ind Voice");
  else if (ts1_act == 0b1010) sprintf (ts1_str, "%s", "Ind Data");
  else if (ts1_act == 0b1011) sprintf (ts1_str, "%s", "Group Data");
  else if (ts1_act == 0b1100) sprintf (ts1_str, "%s", "Group Emergency");
  else if (ts1_act == 0b1101) sprintf (ts1_str, "%s", "Ind Emergency");
  else                        sprintf (ts1_str, "Res %X", ts1_act);

  if      (ts2_act == 0b0000) sprintf (ts2_str, "%s", "Idle");
  else if (ts2_act == 0b0010) sprintf (ts2_str, "%s", "Group CSBK");
  else if (ts2_act == 0b0011) sprintf (ts2_str, "%s", "Ind CSBK");
  else if (ts2_act == 0b1000) sprintf (ts2_str, "%s", "Group Voice");
  else if (ts2_act == 0b1001) sprintf (ts2_str, "%s", "Ind Voice");
  else if (ts2_act == 0b1010) sprintf (ts2_str, "%s", "Ind Data");
  else if (ts2_act == 0b1011) sprintf (ts2_str, "%s", "Group Data");
  else if (ts2_act == 0b1100) sprintf (ts2_str, "%s", "Group Emergency");
  else if (ts2_act == 0b1101) sprintf (ts2_str, "%s", "Ind Emergency");
  else                        sprintf (ts2_str, "Res %X", ts1_act);

  //DMR Location Area - DMRLA - should probably be state variables so we can use this in both slc and csbk
  uint16_t sub_mask = 0x1;
  //tiny n 1-3; small 1-5; large 1-8; huge 1-10
  uint16_t n = 1; //The minimum value of DMRLA is normally ≥ 1, 0 is reserved

  //TODO: Add logic to set n and sub_mask values for DMRLA
  //assigning n as largest possible value for model type

  //Sys_Parms
  if (slco == 0x2 || slco == 0x3) 
  {
    if (model == 0) //Tiny
    {
      net  = (uint16_t)ConvertBitIntoBytes(&slco_bits[6], 9);
      site = (uint16_t)ConvertBitIntoBytes(&slco_bits[15], 3);
      sprintf (model_str, "%s", "Tiny");
      n = 3;
    }
    else if (model == 1) //Small
    {
      net  = (uint16_t)ConvertBitIntoBytes(&slco_bits[6], 7);
      site = (uint16_t)ConvertBitIntoBytes(&slco_bits[13], 5);
      sprintf (model_str, "%s", "Small");
      n = 5;
    }
    else if (model == 2) //Large
    {
      net  = (uint16_t)ConvertBitIntoBytes(&slco_bits[6], 4);
      site = (uint16_t)ConvertBitIntoBytes(&slco_bits[10], 8);
      sprintf (model_str, "%s", "Large");
      n = 8;
    }
    else if (model == 3) //Huge
    {
      net  = (uint16_t)ConvertBitIntoBytes(&slco_bits[6], 2);
      site = (uint16_t)ConvertBitIntoBytes(&slco_bits[8], 10);
      sprintf (model_str, "%s", "Huge");
      n = 10;
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

  }

  //Con+
  uint8_t con_netid  = (uint8_t)ConvertBitIntoBytes(&slco_bits[8], 8);
  uint8_t con_siteid = (uint8_t)ConvertBitIntoBytes(&slco_bits[16], 8);

  //Cap+
  uint8_t capsite = (uint8_t)ConvertBitIntoBytes(&slco_bits[22], 3); //Seems more consistent
  uint8_t restchannel = (uint8_t)ConvertBitIntoBytes(&slco_bits[16], 4); 
  uint8_t cap_reserved = (uint8_t)ConvertBitIntoBytes(&slco_bits[20], 2); //significant value?

  //Hytera XPT
  uint8_t xpt_free = (uint8_t)ConvertBitIntoBytes(&slco_bits[12], 4); //free repeater
  //the next two per SDRTrunk, but only 0 values ever observed here
  uint8_t xpt_pri  = (uint8_t)ConvertBitIntoBytes(&slco_bits[16], 4); //priority repeater
  uint8_t xpt_hash = (uint8_t)ConvertBitIntoBytes(&slco_bits[20], 8); //priority TG hash

  //initial line break
  fprintf (stderr, "\n");
  fprintf (stderr, "%s", KYEL);

  if (slco == 0x2) //C_SYS_Parms
  {
    uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&slco_bits[4], 14);
    if (n != 0) fprintf (stderr, " SLC_C_SYS_PARMS: %s; Net ID: %d; Site ID: %d.%d; Reg Req: %d; CSC: %d;", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, reg, csc);
    else fprintf (stderr, " SLC_C_SYS_PARMS: %s; Net ID: %d; Site ID: %d; Reg Req: %d;", model_str, net, site, reg);
    fprintf (stderr, " SYS: %04X;", syscode); //#192

    //add string for ncurses terminal display
    // if (n != 0) sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d; SYS: %04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    // else sprintf (state->dmr_site_parms, "TIII - %s %d-%d; SYS: %04X; ", model_str, net, site, syscode);
    if (n != 0) sprintf (state->dmr_site_parms, "TIII %s:%d-%d.%d;%04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    else sprintf (state->dmr_site_parms, "TIII %s:%d-%d;%04X; ", model_str, net, site, syscode);

    //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency
    if (opts->use_rigctl == 1 && state->p25_cc_freq == 0) //if not set from channel map 0
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }

  }
  else if (slco == 0x3) //P_SYS_Parms
  {
    uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&slco_bits[4], 14);
    if (n != 0) fprintf (stderr, " SLC_P_SYS_PARMS: %s; Net ID: %d; Site ID: %d.%d; Comp CC: %d;", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, reg); 
    else fprintf (stderr, " SLC_P_SYS_PARMS: %s; Net ID: %d; Site ID: %d;", model_str, net, site);
    fprintf (stderr, " SYS: %04X;", syscode); //#192

    //add string for ncurses terminal display
    // if (n != 0) sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d; SYS: %04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    // else sprintf (state->dmr_site_parms, "TIII - %s %d-%d; SYS: %04X; ", model_str, net, site, syscode);
    if (n != 0) sprintf (state->dmr_site_parms, "TIII %s:%d-%d.%d;%04X; ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, syscode );
    else sprintf (state->dmr_site_parms, "TIII %s:%d-%d;%04X; ", model_str, net, site, syscode);
  }
  else if (slco == 0x0) //null
    fprintf (stderr, " SLCO NULL ");
  else if (slco == 0x1)
  {
    fprintf (stderr, " Activity Update"); //102 361-2 7.1.3.2
    fprintf (stderr, " TS1: %s; Hash: %d;", ts1_str, ts1_hash);
    fprintf (stderr, " TS2: %s; Hash: %d;", ts2_str, ts2_hash);
  } 
  else if (slco == 0x9)
  {
    fprintf (stderr, " SLCO Connect Plus Traffic Channel - Net ID: %d Site ID: %d", con_netid, con_siteid);
    sprintf (state->dmr_site_parms, "%d-%d ", con_netid, con_siteid);
  }
    
  else if (slco == 0xA)
  {
    fprintf (stderr, " SLCO Connect Plus Control Channel - Net ID: %d Site ID: %d", con_netid, con_siteid);
    sprintf (state->dmr_site_parms, "%d-%d ", con_netid, con_siteid);

    //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency
    if (opts->use_rigctl == 1 && opts->p25_is_tuned == 0)
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }

    //if using rtl input, we can ask for the current frequency tuned
    if (opts->audio_in_type == 3 && opts->p25_is_tuned == 0)
    {
      ccfreq = (long int)opts->rtlsdr_center_freq;
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
    
  }
   
  else if (slco == 0xF)
  {
    fprintf (stderr, " SLCO Capacity Plus Site: %d - Rest LSN: %d - RS: %02X", capsite, restchannel, cap_reserved);

    //extra handling for TG hold while trunking enabled
    if (state->tg_hold != 0 && opts->p25_trunk == 1) //logic seems to be fixed now for new rest lsn logic and other considerations
    {
      //debug
      // fprintf (stderr, " TG HOLD Both Slots Busy Check; ");

      //if both slots have some cobination of vlc, pi, voice, or tlc
      int busy = 0;
      if ( (state->dmrburstL == 16 || state->dmrburstL == 0 || state->dmrburstL == 1 || state->dmrburstL == 2) && 
           (state->dmrburstR == 16 || state->dmrburstR == 0 || state->dmrburstR == 1 || state->dmrburstR == 2)) busy = 1;
      if (busy)
      {

        //debug
        // fprintf (stderr, " Busy; ");

        //but nether is the TG on hold
        if ( (state->tg_hold != state->lasttg) && (state->tg_hold != state->lasttgR) )
        {
          //debug
          // fprintf (stderr, " Neither Slot is TG on Hold; ");

          //assign to cc freq if available -- move to right before needed for new logic on rest lsn
          if (state->trunk_chan_map[restchannel] != 0)
          {
            state->p25_cc_freq = state->trunk_chan_map[restchannel];
          }

          //tune to the current rest channel so we can observe its channel status csbks for the TG on hold
          if (state->p25_cc_freq != 0)
          {
            //RIGCTL
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
              SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
              opts->p25_is_tuned = 0;
              state->last_cc_sync_time = time(NULL);
              dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
            }

            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->p25_cc_freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
              opts->p25_is_tuned = 0;
              state->last_cc_sync_time = time(NULL);
              dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
              #endif
            }
          }
        }
      }
    }

  }
  else if (slco == 0x08)
  {
    //The Priority Repeater and Priority Hash values stem from SDRTrunk, but I've never seen these values not be zeroes
    fprintf (stderr, " SLCO Hytera XPT - Free LCN %d - PRI LCN %d - PRI HASH: %02X", xpt_free, xpt_pri, xpt_hash);
    //NOTE: on really busy systems, this free repeater assignment can lag due to the 4 TS requirment to get SLC
    // fprintf (stderr, " SLCO Hytera XPT - Free LCN %d ", xpt_free); 
    sprintf (state->dmr_branding_sub, "XPT ");

    //add string for ncurses terminal display
    sprintf (state->dmr_site_parms, "Free LCN - %d ", xpt_free);

    //extra handling for TG hold while trunking enabled
    if (state->tg_hold != 0 && opts->p25_trunk == 1)
    {
      //if both slots are voice,
      if (state->dmrburstL == 16 && state->dmrburstR == 16)
      {
        //but nether is the TG on hold
        if ( (state->tg_hold != state->lasttg) && (state->tg_hold != state->lasttgR) )
        {
          //convert xpt_free from lcn to lsn -- up to 8 voice repeaters
          if      (xpt_free == 2) xpt_free = 3;
          else if (xpt_free == 3) xpt_free = 5;
          else if (xpt_free == 4) xpt_free = 7;
          else if (xpt_free == 5) xpt_free = 9;
          else if (xpt_free == 6) xpt_free = 11;
          else if (xpt_free == 7) xpt_free = 13;
          else if (xpt_free == 8) xpt_free = 15;

          //check to see if the XPT free channel converted to lsn is available in the map
          if (state->trunk_chan_map[xpt_free] != 0)
          {
            state->p25_cc_freq = state->trunk_chan_map[xpt_free];
          }

          //tune to the current rest channel so we can observe its channel status csbks for the TG on hold
          if (state->p25_cc_freq != 0)
          {
            //RIGCTL
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
              SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
              opts->p25_is_tuned = 0;
              state->last_cc_sync_time = time(NULL);
              dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
            }

            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->p25_cc_freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
              opts->p25_is_tuned = 0;
              state->last_cc_sync_time = time(NULL);
              dmr_reset_blocks (opts, state); //reset all block gathering since we are tuning away
              #endif
            }
          }
        }
      }
    }
  }
    
  else fprintf (stderr, " SLCO Unknown - %d ", slco);

  if (opts->payload == 1 && slco != 0) //if not SLCO NULL
  {
    fprintf (stderr, "\n SLCO Completed Block ");
    for (i = 0; i < 5; i++)
    {
      fprintf (stderr, "[%02X]", slco_bytes[i]);
    }
    fprintf (stderr, "\n"); //if its a voice frame, we need the line break 
  }

  fprintf (stderr, "%s", KNRM);
  
}

//externalize embedded alias to keep the flco function relatively clean
void dmr_embedded_alias_header (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{
  UNUSED(opts);
  
  uint8_t slot = state->currentslot;
  uint8_t format = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 2); 
  uint8_t len;

  //this len seems to pertain to number of blocks? not bit len.
  //len = (uint8_t)ConvertBitIntoBytes(&lc_bits[18], 5); 

  if (format == 0) len = 7;
  else if (format == 1 || format == 2) len = 8;
  else len = 16;

  state->dmr_alias_format[slot] = format;
  state->dmr_alias_len[slot] = len;

  //fprintf (stderr, "F: %d L: %d - ", format, len);
  
}

void dmr_embedded_alias_blocks (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{
  UNUSED(opts);

  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, " Embedded Alias: ");
  uint8_t slot = state->currentslot;
  uint8_t block = (uint8_t)ConvertBitIntoBytes(&lc_bits[2], 6); //FLCO equals block number
  uint8_t format = state->dmr_alias_format[slot]; //0=7-bit; 1=ISO8; 2=UTF-8; 3=UTF16BE
  uint8_t len =  state->dmr_alias_len[slot];
  uint8_t start; //starting position depends on context of block and format
  UNUSED(format);

  //Cap Max Variation
  uint8_t fid = (uint8_t)ConvertBitIntoBytes(&lc_bits[8], 8);
  if (fid == 0x10) block = block - 0x10; //CapMax adds 0x10 to its FLCO (block num) in its Embedded Aliasing

  //there is some issue with the next three lines of code that prevent proper assignments, not sure what.
  //if (block > 4) start = 16;
  //else if (block == 4 && format > 0) start = 23; //8-bit and 16-bit chars
  //else start = 24;

  //forcing start to 16 make it work on 8-bit alias, len seems okay when set off of format
  start = 16; 
  len = 8;

  // fprintf (stderr, "block: %d start: %d len: %d ", block, start, len);

  //all may not be used depending on format, len, start.
  uint16_t A0, A1, A2, A3, A4, A5, A6; 

  //sanity check
  if (block > 7) block = 4; //prevent oob array (although we should never get here)

  if (len > 6) //if not greater than zero, then the header hasn't arrived yet
  {
    A0 = 0; A1 = 0; A2 = 0; A3 = 0; A4 = 0; A5 = 0; A6 = 0; //NULL ASCII Characters
    A0 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*0], len);
    A1 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*1], len);
    A2 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*2], len);
    A3 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*3], len);
    A4 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*4], len);
    A5 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*5], len);
    A6 = (uint16_t)ConvertBitIntoBytes(&lc_bits[start+len*6], len);

    //just going to assign the usual ascii set here to prevent 'naughty' characters, sans diacriticals
    if (A0 > 0x19 && A0 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][0], "%c", A0);
    if (A1 > 0x19 && A1 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][1], "%c", A1);
    if (A2 > 0x19 && A2 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][2], "%c", A2);
    if (A3 > 0x19 && A3 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][3], "%c", A3);
    if (A4 > 0x19 && A4 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][4], "%c", A4);
    if (A5 > 0x19 && A5 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][5], "%c", A5);
    if (A6 > 0x19 && A6 < 0x7F) sprintf (state->dmr_alias_block_segment[slot][block-4][6], "%c", A6);

    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 7; j++)
      {
        fprintf (stderr, "%s", state->dmr_alias_block_segment[slot][i][j]); 
      }
    }
    

  }
  else fprintf (stderr, "Missing Header Block Format and Len Data");
  fprintf (stderr, "%s", KNRM);
  
}
