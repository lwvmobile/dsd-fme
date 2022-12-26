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

  //force slot to 0 if using dmr mono handling
  if (opts->dmr_mono == 1) state->currentslot = 0;

  uint8_t pf = 0;
  uint8_t reserved = 0;
  uint8_t flco = 0;
  uint8_t fid = 0;
  uint8_t so = 0;
  uint32_t target = 0;
  uint32_t source = 0;
  int restchannel = -1;
  int is_cap_plus = 0;
  int is_alias = 0;
  int is_gps = 0;
  uint8_t slot = state->currentslot;

  
  pf = (uint8_t)(lc_bits[0]); //Protect Flag
  reserved = (uint8_t)(lc_bits[1]); //Reserved 
  flco = (uint8_t)ConvertBitIntoBytes(&lc_bits[2], 6); //Full Link Control Opcode
  fid = (uint8_t)ConvertBitIntoBytes(&lc_bits[8], 8); //Feature set ID (FID)
  so = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 8); //Service Options
  target = (uint32_t)ConvertBitIntoBytes(&lc_bits[24], 24); //Target or Talk Group
  source = (uint32_t)ConvertBitIntoBytes(&lc_bits[48], 24);

  //look for Cap+ on VLC header, then set source and/or rest channel appropriately
  if (type == 1 && fid == 0x10 && flco == 0x04)
  {
    is_cap_plus = 1;
    restchannel = (int)ConvertBitIntoBytes(&lc_bits[48], 8);
    source = (uint32_t)ConvertBitIntoBytes(&lc_bits[56], 16);
    sprintf (state->dmr_branding, "%s", "Motorola");
    sprintf (state->dmr_branding_sub, "%s", "Cap+ ");
  }

  
  if (IrrecoverableErrors == 0)
  {
    //Embedded Talker Alias Header Only (format and len storage)
    if (type == 3 && flco == 0x04) 
    {
      dmr_embedded_alias_header(opts, state, lc_bits);
    }

    //Embedded Talker Alias Header (continuation) and Blocks
    if (type == 3 && flco > 0x3 && flco < 0x08) 
    {
      is_alias = 1;
      dmr_embedded_alias_blocks(opts, state, lc_bits);
    }

    //Embedded GPS
    if (type == 3 && flco == 0x08)
    {
      is_gps = 1;
      if (fid == 0) dmr_embedded_gps(opts, state, lc_bits); //issues with fid 0x10 and flco 0x08
    }
  }  

  //will want to continue to observe for different flco and fid combinations to find out their meaning
  //Standard Addressing/Cap+ Addressing (trying to avoid embedded alias and gps, etc)
  if(IrrecoverableErrors == 0 && is_alias == 0 && is_gps == 0) 
  {
    //set overarching manufacturer in use when non-standard feature id set is up
    //may not want to set moto 0x10 here either, lots of radios use that set as well
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
      }
      if (state->currentslot == 1)
      {
        state->dmr_fidR = 0;
        state->dmr_soR = 0;
        // state->lasttgR = 0;
        // state->lastsrcR = 0;
        state->payload_algidR = 0;
        state->payload_miR = 0;
      }
    }
    

    if (restchannel != state->dmr_rest_channel && restchannel != -1)
    {
      state->dmr_rest_channel = restchannel;
      //assign to cc freq
      if (state->trunk_chan_map[restchannel] != 0)
      {
        state->p25_cc_freq = state->trunk_chan_map[restchannel];
      } 
    }

    if (type == 1) fprintf (stderr, "%s \n", KGRN);
    if (type == 2) fprintf (stderr, "%s \n", KRED);
    if (type == 3) fprintf (stderr, "%s", KGRN);
    
    fprintf (stderr, " SLOT %d ", state->currentslot+1);
    fprintf(stderr, "TGT=%u SRC=%u ", target, source);
    if (opts->payload == 1) fprintf(stderr, "FLCO=0x%02X FID=0x%02X SVC=0x%02X ", flco, fid, so);

    if (flco == 3) //UU_V_Ch_Usr 
    {
      sprintf (state->call_string[slot], " Private");
      fprintf (stderr, "Private "); 
    } 
    else //Grp_V_Ch_Usr
    {
      sprintf (state->call_string[slot], "   Group");
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
    }
    //REMUS! Uncomment Line Below if desired
    // else strcat (state->call_string[slot], "          ");

    /* Check the "Service Option" bits */ 
    if(so & 0x30)
    {
      /* Experimentally determined with DSD+,
      * is equal to 0x2, this is a TXI call */
      if((so & 0x30) == 0x20)
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], " TXI");
        fprintf(stderr, "TXI ");
      } 
      else
      {
        //REMUS! Uncomment Line Below if desired
        // strcat (state->call_string[slot], " RES");
        fprintf(stderr, "Reserved=%d ", (so & 0x30) >> 4);
      } 
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
    
    fprintf(stderr, "Call ");
    
    fprintf (stderr, "%s", KRED);
    if (CRCCorrect == 1) ; //CRCCorrect 1 is good, else is bad CRC; no print on good
    else if(IrrecoverableErrors == 0) fprintf(stderr, "(FEC/RAS)");
    else fprintf(stderr, "(FEC/CRC ERR)");

    //check Cap+ rest channel info if available and good fec
    if (is_cap_plus == 1)
    {
      if (restchannel != -1)
      {
        fprintf (stderr, "%s ", KYEL);
        fprintf (stderr, "Cap+ R-Ch %d", restchannel);
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

    if (state->K != 0 && fid == 0x10 && so & 0x40)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "Key %lld ", state->K);
      fprintf (stderr, "%s ", KNRM);
    } 

    if (state->K1 != 0 && fid == 0x68 && so & 0x40)
    {
      fprintf (stderr, "\n");
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, " Key %010llX ", state->K1);
      if (state->K2 != 0) fprintf (stderr, "%016llX ", state->K2);
      if (state->K4 != 0) fprintf (stderr, "%016llX %016llX", state->K3, state->K4);
      fprintf (stderr, "%s ", KNRM);
    } 
    
  }

}

//externalized dmr cach - tact and slco fragment handling
uint8_t dmr_cach (dsd_opts * opts, dsd_state * state, uint8_t cach_bits[25]) 
{
  int i, j;
  uint8_t err = 0;
  uint8_t tact_valid = 0; 
  uint8_t cach_valid = 0;

  bool h1, h2, h3, crc; 

  //dump payload - testing
  uint8_t slco_raw_bits[68]; //raw
  uint8_t slco_bits[68]; //de-interleaved

  //tact pdu
  uint8_t tact_bits[7];
  uint8_t at = 0; //access type, set to 1 during continuous transmission mode 
  uint8_t slot = 2; //tdma time slot
  uint8_t lcss = 0; //link control start stop (9.3.3) NOTE: There is no Single fragment LC defined for CACH signalling

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
      fprintf (stderr, "\n");
      fprintf (stderr, "%s", KRED);
      fprintf (stderr, " SLCO CRC ERR");
      fprintf (stderr, "%s", KNRM);
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
  uint16_t net = 0;
	uint16_t site = 0;
  char model_str[8];
  sprintf (model_str, "%s", "");
  //activity update stuff
  uint8_t ts1_act = (uint8_t)ConvertBitIntoBytes(&slco_bits[4], 4); //activity update ts1
  uint8_t ts2_act = (uint8_t)ConvertBitIntoBytes(&slco_bits[8], 4); //activity update ts2
  uint8_t ts1_hash = (uint8_t)ConvertBitIntoBytes(&slco_bits[12], 8); //ts1 address hash (crc8) //361-1 B.3.7
  uint8_t ts2_hash = (uint8_t)ConvertBitIntoBytes(&slco_bits[20], 8); //ts2 address hash (crc8) //361-1 B.3.7

  //DMR Location Area - DMRLA - should probably be state variables so we can use this in both slc and csbk
  uint16_t sys_area = 0;
  uint16_t sub_area = 0;
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
  uint8_t restchannel = (uint8_t)ConvertBitIntoBytes(&slco_bits[12], 8);

  //initial line break
  fprintf (stderr, "\n");
  fprintf (stderr, "%s", KYEL);

  if (slco == 0x2) //C_SYS_Parms
  {
    if (n != 0) fprintf (stderr, " C_SYS_PARMS - %s - Net ID: %d Site ID: %d.%d - Reg Req: %d - CSC: %d ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, reg, csc);
    else fprintf (stderr, " C_SYS_PARMS - %s - Net ID: %d Site ID: %d - Reg Req: %d - CSC: %d ", model_str, net, site, reg, csc);

    //add string for ncurses terminal display
    if (n != 0) sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1 );
    else sprintf (state->dmr_site_parms, "TIII - %s %d-%d ", model_str, net, site);

    //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency

    if (opts->use_rigctl == 1 && state->p25_cc_freq == 0) //if not set from channel map 0
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }

    //nullify any previous branding sub (bugfix for bad assignments or system type switching)
    //sprintf(state->dmr_branding_sub, "%s", "");

    //debug print
    uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&slco_bits[4], 14);
    //fprintf (stderr, "\n  SYSCODE: %014b", syscode);
    //fprintf (stderr, "\n  SYSCODE: %02b.%b.%b", model, net, site); //won't show leading zeroes, but may not be required

  }
  else if (slco == 0x3) //P_SYS_Parms
  {
    fprintf (stderr, " P_SYS_PARMS - %s - Net ID: %d Site ID: %d.%d - Comp CC: %d ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, reg); 

    //add string for ncurses terminal display
    if (n != 0) sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1 );
    else sprintf (state->dmr_site_parms, "TIII - %s %d-%d ", model_str, net, site);

    //nullify any previous branding sub (bugfix for bad assignments or system type switching)
    //sprintf(state->dmr_branding_sub, "%s", "");

    //debug print
    uint16_t syscode = (uint16_t)ConvertBitIntoBytes(&slco_bits[4], 14);
    //fprintf (stderr, "\n  SYSCODE: %014b", syscode);
    //fprintf (stderr, "\n  SYSCODE: %02b.%b.%b", model, net, site); //won't show leading zeroes, but may not be required

  }
  else if (slco == 0x0) //null
    fprintf (stderr, " SLCO NULL ");
  else if (slco == 0x1)
    fprintf (stderr, " SLCO Activity Update TS1: %X Hash: %02X TS2: %X Hash: %02X", ts1_act, ts1_hash, ts2_act, ts2_hash); //102 361-2 7.1.3.2 
  else if (slco == 0x9)
  {
    state->dmr_mfid = 0x10;
    sprintf (state->dmr_branding, "%s", "Motorola");
    sprintf (state->dmr_branding_sub, "%s", "Con+ ");
    fprintf (stderr, " SLCO Connect Plus Voice Channel - Net ID: %d Site ID: %d", con_netid, con_siteid);
    sprintf (state->dmr_site_parms, "%d-%d ", con_netid, con_siteid);
  }
    
  else if (slco == 0xA)
  {
    state->dmr_mfid = 0x10;
    sprintf (state->dmr_branding, "%s", "Motorola");
    sprintf (state->dmr_branding_sub, "%s", "Con+ ");
    fprintf (stderr, " SLCO Connect Plus Control Channel - Net ID: %d Site ID: %d", con_netid, con_siteid);
    sprintf (state->dmr_site_parms, "%d-%d ", con_netid, con_siteid);

    //if using rigctl we can set an unknown cc frequency by polling rigctl for the current frequency
    if (opts->use_rigctl == 1 && state->p25_cc_freq == 0) //if not set from channel map 0
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
  }
   
  else if (slco == 0xF)
  {
    state->dmr_mfid = 0x10;
    sprintf (state->dmr_branding, "%s", "Motorola");
    sprintf (state->dmr_branding_sub, "%s", "Cap+ ");
    fprintf (stderr, " SLCO Capacity Plus Rest Channel %d", restchannel);
    state->dmr_rest_channel = restchannel;
    //assign to cc freq if available
    if (state->trunk_chan_map[restchannel] != 0)
    {
      state->p25_cc_freq = state->trunk_chan_map[restchannel];
    }

    //nullify any previous TIII data (bugfix for bad assignments or system type switching)
    sprintf(state->dmr_site_parms, "%s", "");
  }
    
  else fprintf (stderr, " SLCO Unknown - %d ", slco);

  if (opts->payload == 1 && slco != 0) //if not SLCO NULL
  {
    fprintf (stderr, "\n SLCO Completed Block ");
    for (i = 0; i < 5; i++)
    {
      fprintf (stderr, "[%02X]", slco_bytes[i]);
    }
  }

  fprintf (stderr, "%s", KNRM);
  
}

//externalize embedded alias to keep the flco function relatively clean
void dmr_embedded_alias_header (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{
  
  uint8_t slot = state->currentslot;
  uint8_t pf; 
  uint8_t res;
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
  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, " Embedded Alias: ");
  uint8_t slot = state->currentslot;
  uint8_t block = (uint8_t)ConvertBitIntoBytes(&lc_bits[2], 6); //FLCO equals block number
  uint8_t format = state->dmr_alias_format[slot]; //0=7-bit; 1=ISO8; 2=UTF-8; 3=UTF16BE
  uint8_t len =  state->dmr_alias_len[slot];
  uint8_t start; //starting position depends on context of block and format

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

//externalize embedded GPS - Needs samples to test/fix function
void dmr_embedded_gps (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{
  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, " Embedded GPS:");
  uint8_t slot = state->currentslot;
  uint8_t pf = lc_bits[0];
  uint8_t res_a = lc_bits[1];
  uint8_t res_b = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 4);
  uint8_t pos_err = (uint8_t)ConvertBitIntoBytes(&lc_bits[20], 3);
  
  uint32_t lon_sign = lc_bits[23]; 
  uint32_t lon = (uint32_t)ConvertBitIntoBytes(&lc_bits[24], 24); 
  uint32_t lat_sign = lc_bits[48]; 
  uint32_t lat = (uint32_t)ConvertBitIntoBytes(&lc_bits[49], 23); 

  double lat_unit = (double)180/ pow (2.0, 24); //180 divided by 2^24
  double lon_unit = (double)360/ pow (2.0, 25); //360 divided by 2^25

  char latstr[3];
  char lonstr[3];
  sprintf (latstr, "%s", "N");
  sprintf (lonstr, "%s", "E");

  //run calculations and print (still cannot verify as accurate)
  //7.2.16 and 7.2.17 (two's compliment)

  double latitude = 0;  
  double longitude = 0; 

  if (pf) fprintf (stderr, " Protected");
  else
  {
    if (lat_sign)
    {
      lat = 0x800001 - lat;
      sprintf (latstr, "%s", "S");
    } 
    latitude = ((double)lat * lat_unit);

    if (lon_sign)
    {
      lon = 0x1000001 - lon;
      sprintf (lonstr, "%s", "W");
    } 
    longitude = ((double)lon * lon_unit);

    //sanity check
    if (abs (latitude) < 90 && abs(longitude) < 180)
    {
      fprintf (stderr, " Lat: %.5lf %s Lon: %.5lf %s", latitude, latstr, longitude, lonstr);

      //7.2.15 Position Error
      uint16_t position_error = 2 * pow(10, pos_err); //2 * 10^pos_err 
      if (pos_err == 0x7 ) fprintf (stderr, " Position Error: Unknown or Invalid");
      else fprintf (stderr, " Position Error: Less than %dm", position_error);

      //save to array for ncurses
      if (pos_err != 0x7)
      {
        sprintf (state->dmr_embedded_gps[slot], "E-GPS (%lf %s %lf %s) Err: %dm", latitude, latstr, longitude, lonstr, position_error);
      }

    }

  }

  fprintf (stderr, "%s", KNRM);
}
