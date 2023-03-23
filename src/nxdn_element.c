/*
 ============================================================================
 Name        : nxdn_element.c (formerly nxdn_lib)
 Author      :
 Version     : 1.0
 Date        : 2018 December 26
 Copyright   : No copyright
 Description : NXDN decoding source lib - modified from nxdn_lib
 Origin      : Originally found at - https://github.com/LouisErigHerve/dsd
 ============================================================================
 */

#include "dsd.h"

void NXDN_SACCH_Full_decode(dsd_opts * opts, dsd_state * state)
{
  uint8_t SACCH[72];
  uint32_t i;
  uint8_t CrcCorrect = 1;

  /* Consider all SACCH CRC parts as correct */
  CrcCorrect = 1;

  /* Reconstitute the full 72 bits SACCH */
  for(i = 0; i < 4; i++)
  {
    memcpy(&SACCH[i * 18], state->nxdn_sacch_frame_segment[i], 18);

    /* Check CRC */ 
    if (state->nxdn_sacch_frame_segcrc[i] != 0) CrcCorrect = 0;
  }

  /* Decodes the element content */
  // currently only going to run this if all four CRCs are good
  if (CrcCorrect == 1) NXDN_Elements_Content_decode(opts, state, CrcCorrect, SACCH);
  else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, SACCH);

} /* End NXDN_SACCH_Full_decode() */


void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state,
                                  uint8_t CrcCorrect, uint8_t * ElementsContent)
{
  uint32_t i;
  uint8_t MessageType;
  uint64_t CurrentIV = 0;
  unsigned long long int FullMessage = 0;
  /* Get the "Message Type" field */
  MessageType  = (ElementsContent[2] & 1) << 5;
  MessageType |= (ElementsContent[3] & 1) << 4;
  MessageType |= (ElementsContent[4] & 1) << 3;
  MessageType |= (ElementsContent[5] & 1) << 2;
  MessageType |= (ElementsContent[6] & 1) << 1;
  MessageType |= (ElementsContent[7] & 1) << 0;

  //only run message type if good CRC? 
  if (CrcCorrect == 1) nxdn_message_type (opts, state, MessageType);

  /* Save the "F1" and "F2" flags */
  state->NxdnElementsContent.F1 = ElementsContent[0];
  state->NxdnElementsContent.F2 = ElementsContent[1];

  /* Save the "Message Type" field */
  state->NxdnElementsContent.MessageType = MessageType;

  /* Decode the right "Message Type" */
  switch(MessageType)
  {
    //VCALL_ASSGN
    case 0x04:
      //continue flow to DUP, both use same message format

    //VCALL_ASSGN_DUP
    case 0x05:
      NXDN_decode_VCALL_ASSGN(opts, state, ElementsContent);
      break;

    //Alias 0x3F
    case 0x3F:
      state->NxdnElementsContent.VCallCrcIsGood = CrcCorrect;
      NXDN_decode_Alias(opts, state, ElementsContent);
      break;

    //SRV_INFO
    case 0x19:
      NXDN_decode_srv_info(opts, state, ElementsContent);
      break;

    //CCH_INFO
    case 0x1A:
      NXDN_decode_cch_info(opts, state, ElementsContent);
      break;

    //SITE_INFO
    case 0x18:
      NXDN_decode_site_info(opts, state, ElementsContent);
      break;

    //DISC
    case 0x11:
      //NXDN_decode_VCALL(opts, state, ElementsContent);

      //tune back to CC here - save about 1-2 seconds
      if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && opts->p25_is_tuned == 1)
      {
        //rigctl
        if (opts->use_rigctl == 1)
        {
          //extra safeguards due to sync issues with NXDN
          memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
          memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
          // state->lastsynctype = -1; 
          // state->last_cc_sync_time = time(NULL);
          opts->p25_is_tuned = 0;
          if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
          SetFreq(opts->rigctl_sockfd, state->p25_cc_freq); 
          
        }
        //rtl_udp
        else if (opts->audio_in_type == 3)
        {
          //extra safeguards due to sync issues with NXDN
          memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
          memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
          // state->lastsynctype = -1; 
          // state->last_cc_sync_time = time(NULL);
          opts->p25_is_tuned = 0;
          rtl_udp_tune (opts, state,  state->p25_cc_freq); 
        }
      }
      
      break;

    //Idle
    case 0x10:
    {
      break;
    }

    /* VCALL */
    case 0x01:
    {

      /* Set the CRC state */
      state->NxdnElementsContent.VCallCrcIsGood = CrcCorrect;

      /* Decode the "VCALL" message */
      NXDN_decode_VCALL(opts, state, ElementsContent);

      /* Check the "Cipher Type" and the "Key ID" validity */
      if(CrcCorrect)
      {
        state->NxdnElementsContent.CipherParameterValidity = 1;
      }
      else state->NxdnElementsContent.CipherParameterValidity = 0;
      break;
    } /* End case NXDN_VCALL: */

    /* VCALL_IV */
    case 0x03:
    {

      /* Set the CRC state */
      state->NxdnElementsContent.VCallIvCrcIsGood = CrcCorrect;

      /* Decode the "VCALL_IV" message */
      NXDN_decode_VCALL_IV(opts, state, ElementsContent);

      if(CrcCorrect)
      {
        /* CRC is correct, copy the next theorical IV to use directly from the
         * received VCALL_IV */
        memcpy(state->NxdnElementsContent.NextIVComputed, state->NxdnElementsContent.IV, 8);
      }
      else
      {
        /* CRC is incorrect, compute the next IV to use */
        CurrentIV = 0;

        /* Convert the 8 bytes buffer into a 64 bits integer */
        for(i = 0; i < 8; i++)
        {
          CurrentIV |= state->NxdnElementsContent.NextIVComputed[i];
          CurrentIV = CurrentIV << 8;
        }

      }
      break;
    } /* End case NXDN_VCALL_IV: */

    /* Unknown Message Type */
    default:
    {
      break;
    }
  } /* End switch(MessageType) */

} /* End NXDN_Elements_Content_decode() */

//externalize multiple sub-element handlers
void nxdn_location_id_handler (dsd_state * state, uint32_t location_id)
{
  //6.5.2 Location ID
  uint8_t category_bit = location_id >> 22; 
  uint32_t sys_code = 0;
  uint16_t site_code = 0;

  char category[14]; //G, R, or L 

  if (category_bit == 0)
  {
    sys_code  = ( (location_id & 0x3FFFFF) >> 12); //10 bits
    site_code = location_id & 0x3FF; //12 bits    
    sprintf (category, "%s", "Global");
  } 
  else if (category_bit == 2)
  {
    sys_code  = ( (location_id & 0x3FFFFF) >> 8); //14 bits
    site_code = location_id & 0xFF; //8 bits 
    sprintf (category, "%s", "Regional");
  } 
  else if (category_bit == 1)
  {
    sys_code  = ( (location_id & 0x3FFFFF) >> 5); //17 bits
    site_code = location_id & 0x1F; //5 bits 
    sprintf (category, "%s", "Local");
  }
  else
  {
    //err, or we shouldn't ever get here
    sprintf (category, "%s", "Reserved/Err");
  }

  //not entirely convinced that site code and ran are the exact same thing due to bit len differences
  //site code is meant to be a color code (but this could also be like the P25 p1 nac and p2 cc being the same)
  state->nxdn_last_ran = site_code; //this one may drop on nocarrier
  if (site_code != 0) state->nxdn_location_site_code = site_code; //this one won't
  if (sys_code != 0) state->nxdn_location_sys_code = sys_code;
  sprintf (state->nxdn_location_category, "%s", category); 

  fprintf (stderr, "\n Location Information - Cat: %s - Sys Code: %d - Site Code %d ", category, sys_code, site_code);

}

void nxdn_srv_info_handler (dsd_state * state, uint16_t svc_info)
{
  //handle the service information elements
  //Part 1-A Common Air Interface Ver.2.0
  //6.5.33. Service Information
  fprintf (stderr, "\n Services:");
  //check each SIF 1-bit element
  if (svc_info & 0x8000) fprintf (stderr, " Multi-Site;");
  if (svc_info & 0x4000) fprintf (stderr, " Multi-System;");
  if (svc_info & 0x2000) fprintf (stderr, " Location Registration;");
  if (svc_info & 0x1000) fprintf (stderr, " Group Registration;");

  if (svc_info & 0x800) fprintf (stderr, " Authentication;");
  if (svc_info & 0x400) fprintf (stderr, " Composite Control Channel;");
  if (svc_info & 0x200) fprintf (stderr, " Voice Call;");
  if (svc_info & 0x100) fprintf (stderr, " Data Call;");

  if (svc_info & 0x80) fprintf (stderr, " Short Data Call;");
  if (svc_info & 0x40) fprintf (stderr, " Status Call & Remote Control;");
  if (svc_info & 0x20) fprintf (stderr, " PSTN Network Connection;");
  if (svc_info & 0x10) fprintf (stderr, " IP Network Connection;");

  //last 4-bits are spares

}

void nxdn_rst_info_handler (dsd_state * state, uint32_t rst_info)
{
  //handle the restriction information elements
  //Part 1-A Common Air Interface Ver.2.0
  //6.5.34. Restriction Information
  fprintf (stderr, "\n RST -");

  //Mobile station operation information (Octet 0, Bits 7 to 4)
  fprintf (stderr, " MS:");
  if (rst_info & 0x800000)      fprintf (stderr, " Access Restriction;");
  else if (rst_info & 0x400000) fprintf (stderr, " Maintenance Restriction;");
  // else                          fprintf (stderr, " No Restriction;");

  //Access cycle interval (Octet 0, Bits 3 to 0)
  fprintf (stderr, " ACI:");
  uint8_t frames = (rst_info >> 16) & 0xF; 
  if (frames) fprintf (stderr, " %d Frame Restriction;", frames * 20);
  // else        fprintf (stderr, " No Restriction;");

  //Restriction group specification (Octet 1, Bits 7 to 4)
  fprintf (stderr, " RGS:");
  uint8_t uid = (rst_info >> 12) & 0x7; //MSB is a spare, so only evaluate 3-bits
  fprintf (stderr, " Lower 3 bits of Unit ID = %d %d %d", uid & 1, (uid >> 1) & 1, (uid >> 2) & 1);

  //Restriction Information (Octet 1, Bits 3 to 0)
  fprintf (stderr, " RI:");
  if (rst_info & 0x800)      fprintf (stderr, " Location Restriction;");
  else if (rst_info & 0x400) fprintf (stderr, " Call Restriction;");
  else if (rst_info & 0x200) fprintf (stderr, " Short Data Restriction;");
  // else                        fprintf (stderr, " No Restriction;");

  //Restriction group ratio specification (Octet 2, Bits 7 to 6)
  fprintf (stderr, " RT:");
  uint8_t ratio = (rst_info >> 22) & 0x3; 
  if      (ratio == 1) fprintf (stderr, " 50 Restriction;");
  else if (ratio == 2) fprintf (stderr, " 75 Restriction;");
  else if (ratio == 3) fprintf (stderr, " 87.5 Restriction;");
  // else                 fprintf (stderr, " No Restriction;");

  //Delay time extension specification (Octet 2, Bits 5 to 4)
  fprintf (stderr, " DT:");
  uint8_t dt = (rst_info >> 20) & 0x3; 
  if      (dt == 0) fprintf (stderr, " Timer T2 max x 1;");
  else if (dt == 1) fprintf (stderr, " Timer T2 max x 2;");
  else if (dt == 2) fprintf (stderr, " Timer T2 max x 3;");
  else              fprintf (stderr, " Timer T2 max x 4;");

  //ISO Temporary Isolation Site -- This is valid only if the SIF 1 of Service Information is set to 1.
  if (rst_info & 0x0001)      fprintf (stderr, " - Site Isolation;");

  //what a pain...
}

void nxdn_ca_info_handler (dsd_state * state, uint32_t ca_info)
{
  //handle the channel access info for channel or dfa
  //Part 1-A Common Air Interface Ver.2.0
  //6.5.36. Channel Access Information
  //this element only seems to appear in the SITE_INFO message
  uint32_t RCN = ca_info >> 23; //Radio Channel Notation
  uint32_t step = (ca_info >> 21) & 0x3; //Stepping
  uint32_t base = (ca_info >> 19) & 0x7; //Base Frequency
  uint32_t spare = ca_info & 0x3FF;

  //set state variable here to tell us to use DFA or Channel Versions
  if (RCN == 1)
  {
    state->nxdn_rcn = RCN; 
    state->nxdn_step = step;
    state->nxdn_base_freq = base;
  } 

}

//end sub-element handlers

void NXDN_decode_VCALL_ASSGN(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  //just using 'short form' M only data, not the optional data
  uint8_t  CCOption = 0;
  uint8_t  CallType = 0;
  uint8_t  VoiceCallOption = 0;
  uint16_t SourceUnitID = 0;
  uint16_t DestinationID = 0;
  uint8_t  CallTimer = 0;
  uint16_t Channel = 0;
  uint8_t  LocationIDOption = 0;

  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};

  //DFA specific variables
  uint8_t  bw = 0;
  uint16_t OFN = 0;
  uint16_t IFN = 0;

  /* Decode "CC Option" */
  CCOption = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);
  state->NxdnElementsContent.CCOption = CCOption;

  /* Decode "Call Type" */
  CallType = (uint8_t)ConvertBitIntoBytes(&Message[16], 3);
  state->NxdnElementsContent.CallType = CallType;

  /* Decode "Voice Call Option" */
  VoiceCallOption = (uint8_t)ConvertBitIntoBytes(&Message[19], 5);
  state->NxdnElementsContent.VoiceCallOption = VoiceCallOption;

  /* Decode "Source Unit ID" */
  SourceUnitID = (uint16_t)ConvertBitIntoBytes(&Message[24], 16);
  state->NxdnElementsContent.SourceUnitID = SourceUnitID;

  /* Decode "Destination ID" */
  DestinationID = (uint16_t)ConvertBitIntoBytes(&Message[40], 16);
  state->NxdnElementsContent.DestinationID = DestinationID;

  /* Decode "Call Timer" */ //unsure of format of call timer, not required info for trunking
  CallTimer = (uint8_t)ConvertBitIntoBytes(&Message[56], 6); 

  /* Decode "Channel" */
  Channel = (uint16_t)ConvertBitIntoBytes(&Message[62], 10);

  /* Decode DFA-only variables*/
  if (state->nxdn_rcn == 1)
  {
    bw  = (uint8_t)ConvertBitIntoBytes(&Message[62], 2); 
    OFN = (uint16_t)ConvertBitIntoBytes(&Message[64], 16);
    IFN = (uint16_t)ConvertBitIntoBytes(&Message[80], 16);
  }
  
  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  /* Print the "Call Type" */
  fprintf (stderr, "%s", KGRN);
  fprintf(stderr, "\n %s - ", NXDN_Call_Type_To_Str(CallType)); 

  /* Print the "Voice Call Option" */
  NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  fprintf(stderr, "%s %s - ", DuplexMode, TransmissionMode);

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);

  /* Print Channel */
  if (state->nxdn_rcn == 0)
    fprintf(stderr, "- Channel [%03X][%04d] ", Channel & 0x3FF, Channel & 0x3FF);
  if (state->nxdn_rcn == 1) 
    fprintf(stderr, "- DFA Channel [%04X][%05d] ", OFN, OFN);

  //run process to figure out frequency value from the channel import or from DFA
  long int freq = 0;
  if (state->nxdn_rcn == 0) 
    freq = nxdn_channel_to_frequency(opts, state, Channel);
  if (state->nxdn_rcn == 1)
    freq = nxdn_channel_to_frequency(opts, state, OFN);

  //check the rkey array for a scrambler key value
  //TGT ID and Key ID could clash though if csv or system has both with different keys
  if (state->rkey_array[DestinationID] != 0) state->R = state->rkey_array[DestinationID];
  if (state->M == 1) state->nxdn_cipher_type = 0x1;

  //check for control channel frequency in the channel map if not available
  if (opts->p25_trunk == 1 && state->p25_cc_freq == 0)
  {
    long int ccfreq = 0;
    
    //if not available, then poll rigctl if its available
    if (opts->use_rigctl == 1)
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
    //if using rtl input, we can ask for the current frequency tuned
    else if (opts->audio_in_type == 3)
    {
      ccfreq = (long int)opts->rtlsdr_center_freq;
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
  }

  //run group/source analysis and tune if available/desired
  //group list mode so we can look and see if we need to block tuning any groups, etc
	char mode[8]; //allow, block, digital, enc, etc

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

  for (int i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == DestinationID) //source, or destination?
    {
      fprintf (stderr, " [%s]", state->group_array[i].groupName);
      strcpy (mode, state->group_array[i].groupMode);
    }
    //might not be ideal if both source and group/target are both in the array
    else if (state->group_array[i].groupNumber == SourceUnitID) //source, or destination?
    {
      fprintf (stderr, " [%s]", state->group_array[i].groupName);
      strcpy (mode, state->group_array[i].groupMode);
    }
  }

  //check to see if the source/target candidate is blocked first
  if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
  {
    if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency already
    {
      //rigctl
      if (opts->use_rigctl == 1)
      {
        //extra safeguards due to sync issues with NXDN
        memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		    state->lastsynctype = -1; 
        state->last_cc_sync_time = time(NULL);
        state->last_vc_sync_time = time(NULL); //useful?
        //
        
        if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
        SetFreq(opts->rigctl_sockfd, freq);
        state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
        opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop

        //set rid and tg when we actually tune to it
        state->nxdn_last_rid = SourceUnitID & 0xFFFF;   
        state->nxdn_last_tg = (DestinationID & 0xFFFF);
        sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));
      }
      //rtl_udp
      else if (opts->audio_in_type == 3)
      {
        //extra safeguards due to sync issues with NXDN
        memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		    state->lastsynctype = -1; 
        state->last_cc_sync_time = time(NULL);
        state->last_vc_sync_time = time(NULL);
        //

        rtl_udp_tune (opts, state, freq); 
        state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
        opts->p25_is_tuned = 1;

        //set rid and tg when we actually tune to it
        state->nxdn_last_rid = SourceUnitID & 0xFFFF;   
        state->nxdn_last_tg = (DestinationID & 0xFFFF);
        sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));
      }
      
    }    
  }

  fprintf (stderr, "%s", KNRM);

} /* End NXDN_decode_VCALL_ASSGN() */

void NXDN_decode_Alias(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint8_t Alias1 = 0x0; //value of an ascii 'NULL' character
  uint8_t Alias2 = 0x0;
  uint8_t Alias3 = 0x0;
  uint8_t Alias4 = 0x0;
  uint8_t blocknumber = 0; 
  uint8_t CrcCorrect = 0;
  
  //alias can also hit on a facch1 so that would be with a non_sf_sacch attached
  if (state->nxdn_sacch_non_superframe == FALSE)
  {
    CrcCorrect = state->NxdnElementsContent.VCallCrcIsGood;
  }
  else CrcCorrect = 1; //FACCH1 with bad CRC won't make it this far anyways, so set as 1
  

  //FACCH Payload [3F][68][82][04][2 <- block number4] "[69][6F][6E][20]" <- 4 alias octets [00][7F][1C]
  blocknumber = (uint8_t)ConvertBitIntoBytes(&Message[32], 4) & 0x7; // & 0x7, might just be three bits, unsure
  Alias1 = (uint8_t)ConvertBitIntoBytes(&Message[40], 8);
  Alias2 = (uint8_t)ConvertBitIntoBytes(&Message[48], 8);
  Alias3 = (uint8_t)ConvertBitIntoBytes(&Message[56], 8);
  Alias4 = (uint8_t)ConvertBitIntoBytes(&Message[64], 8);
  //sanity check to prevent OOB array assignment
  if (blocknumber > 0 && blocknumber < 5)
  {
    //assign to index -1, since block number conveyed here is 1,2,3,4, and index values are 0,1,2,3
    //only assign if within valid range of ascii characters (not including diacritical extended alphabet)
    //else assign "null" ascii character

    //since we are zeroing out the blocks on tx_rel and other conditions, better to just set nothing to bad Alias bytes
    //tends to zero out otherwise already good blocks set in a previous repitition.
    if (Alias1 > 0x19 && Alias1 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][0], "%c", Alias1);
    else ;// sprintf (state->nxdn_alias_block_segment[blocknumber-1][0], "%c", 0);

    if (Alias2 > 0x19 && Alias2 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][1], "%c", Alias2);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][1], "%c", 0);

    if (Alias3 > 0x19 && Alias3 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][2], "%c", Alias3);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][2], "%c", 0);

    if (Alias4 > 0x19 && Alias4 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][3], "%c", Alias4);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][3], "%c", 0);
  }

  //crc errs in one repitition may occlude an otherwise good alias, so test and change if needed
  //completed alias should still appear in ncurses terminal regardless, so this may be okay
  if (CrcCorrect) 
  { 
    fprintf (stderr, " "); //spacer
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        fprintf (stderr, "%s", state->nxdn_alias_block_segment[i][j]); 
      }
    }
    fprintf (stderr, " ");
  }
  else fprintf (stderr, " CRC ERR "); //redundant print? or okay?
}

void NXDN_decode_cch_info(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  //6.4.3.3. Control Channel Information (CCH_INFO) for more information
  uint32_t location_id = 0;
  uint8_t channel1sts = 0;
  uint16_t channel1 = 0;
  uint8_t channel2sts = 0;
  uint16_t channel2 = 0;
  long int freq1 = 0;
  long int freq2 = 0;
  
  //DFA
  uint8_t  bw1 = 0;
  uint16_t OFN1 = 0;
  uint16_t IFN1 = 0;
  uint8_t  bw2 = 0;
  uint16_t OFN2 = 0;
  uint16_t IFN2 = 0;

  location_id = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
  channel1sts = (uint8_t)ConvertBitIntoBytes(&Message[32], 6);
  channel1 =    (uint16_t)ConvertBitIntoBytes(&Message[38], 10);
  channel2sts = (uint8_t)ConvertBitIntoBytes(&Message[48], 6);
  channel2 =    (uint16_t)ConvertBitIntoBytes(&Message[54], 10);

  fprintf (stderr, "%s", KYEL);
  nxdn_location_id_handler (state, location_id);

  fprintf (stderr, "\n Control Channel Information \n");

  //non-DFA version
  if (state->nxdn_rcn == 0)
  {
    fprintf (stderr, "  Location ID [%06X] CC1 [%03X][%04d] CC2 [%03X][%04d] Status: ", location_id, channel1, channel1, channel2, channel2);
    //check the sts bits to determine if current, new, add, or delete
    if (channel1sts & 0x20) fprintf (stderr, "Current ");
    if (channel1sts & 0x10) fprintf (stderr, "New ");
    if (channel1sts & 0x08) fprintf (stderr, "Candidate Added ");
    if (channel1sts & 0x04) fprintf (stderr, "Candidate Deleted ");
    freq1 = nxdn_channel_to_frequency (opts, state, channel1);
    freq2 = nxdn_channel_to_frequency (opts, state, channel2);
  }
  
  //DFA version
  if (state->nxdn_rcn == 1)
  {
    bw1  = (uint8_t)ConvertBitIntoBytes(&Message[38], 2);
    OFN1 = (uint16_t)ConvertBitIntoBytes(&Message[40], 16);
    IFN1 = (uint16_t)ConvertBitIntoBytes(&Message[56], 16);

    //facch1 will not have the below items
    // bw2  = (uint8_t)ConvertBitIntoBytes(&Message[78], 2);
    // OFN2 = (uint16_t)ConvertBitIntoBytes(&Message[80], 16);
    // IFN2 = (uint16_t)ConvertBitIntoBytes(&Message[96], 16);

    if (channel1sts & 0x10) fprintf (stderr, "New ");
    if (channel1sts & 0x02) fprintf (stderr, "Current1 ");
    if (channel1sts & 0x01) fprintf (stderr, "Current2 ");
    freq1 = nxdn_channel_to_frequency (opts, state, OFN1);

    //facch1 does not carry this value, so let's not look at it
    // freq2 = nxdn_channel_to_frequency (opts, state, OFN2);
  }
  
  fprintf (stderr, "%s", KNRM);
}

void NXDN_decode_srv_info(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint32_t location_id = 0;
  uint16_t svc_info = 0; //service information
  uint32_t rst_info = 0; //restriction information

  location_id = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
  svc_info    = (uint16_t)ConvertBitIntoBytes(&Message[32], 16);
  rst_info    = (uint32_t)ConvertBitIntoBytes(&Message[48], 24);

  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, "\n Service Information - ");
  fprintf (stderr, "Location ID [%06X] SVC [%04X] RST [%06X] ", location_id, svc_info, rst_info);
  nxdn_location_id_handler (state, location_id);

  //run the srv info
  nxdn_srv_info_handler (state, svc_info);

  //run the rst info, if not zero
  if (rst_info) nxdn_rst_info_handler (state, rst_info);

  fprintf (stderr, "%s", KNRM);

  //poll for current frequency, will always be the control channel
  //this PDU is constantly pumped out on the CC CAC Message
  if (opts->p25_trunk == 1)
  {
    long int ccfreq = 0;
    //if using rigctl, we can poll for the current frequency
    if (opts->use_rigctl == 1)
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
    //if using rtl input, we can ask for the current frequency tuned
    else if (opts->audio_in_type == 3)
    {
      ccfreq = (long int)opts->rtlsdr_center_freq;
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
  }

}

void NXDN_decode_site_info(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint32_t location_id = 0;
  uint16_t cs_info = 0;  //channel structure information
  uint16_t svc_info = 0; //service information
  uint32_t rst_info = 0; //restriction information
  uint32_t ca_info = 0; //channel access information
  uint8_t version_num = 0;
  uint8_t adj_alloc = 0; //number of adjacent sites (on same system?)

  uint16_t channel1 = 0;
  uint16_t channel2 = 0;
  long int freq1 = 0;
  long int freq2 = 0;

  location_id = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
  cs_info     = (uint16_t)ConvertBitIntoBytes(&Message[32], 16);
  svc_info    = (uint16_t)ConvertBitIntoBytes(&Message[48], 16);
  rst_info    = (uint32_t)ConvertBitIntoBytes(&Message[64], 24);
  ca_info     = (uint32_t)ConvertBitIntoBytes(&Message[88], 24);
  version_num = (uint8_t)ConvertBitIntoBytes(&Message[112], 8);
  adj_alloc   = (uint8_t)ConvertBitIntoBytes(&Message[120], 4);
  channel1    = (uint16_t)ConvertBitIntoBytes(&Message[124], 10);
  channel2    = (uint16_t)ConvertBitIntoBytes(&Message[134], 10);

  //check the channel access information first
  nxdn_ca_info_handler (state, ca_info);

  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, "\n Location ID [%06X] CSC [%04X] SVC [%04X] RST [%06X] \n          CA [%06X] V[%X] ADJ [%01X] ", 
                                location_id, cs_info, svc_info, rst_info, ca_info, version_num, adj_alloc);
  nxdn_location_id_handler(state, location_id);

  //run the srv info
  nxdn_srv_info_handler (state, svc_info);

  //run the rst info, if not zero
  if (rst_info) nxdn_rst_info_handler (state, rst_info);

  //only get frequencies if using channel version of message and not dfa
  if (state->nxdn_rcn == 0)
  {
    if (channel1 != 0)
    {
      fprintf (stderr, "\n Control Channel 1 [%03X][%04d] ", channel1, channel1 );
      freq1 = nxdn_channel_to_frequency (opts, state, channel1);
    }
    if (channel2 != 0)
    {
      fprintf (stderr, "\n Control Channel 2 [%03X][%04d] ", channel2, channel2 );
      freq2 = nxdn_channel_to_frequency (opts, state, channel2);
    }
  }
  else
  {
    ; //DFA version does not carry an OFN/IFN value, so no freqs
  }

  fprintf (stderr, "%s", KNRM);


}

void NXDN_decode_VCALL(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  uint8_t  CCOption = 0;
  uint8_t  CallType = 0;
  uint8_t  VoiceCallOption = 0;
  uint16_t SourceUnitID = 0;
  uint16_t DestinationID = 0;
  uint8_t  CipherType = 0;
  uint8_t  KeyID = 0;
  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};
  unsigned long long int FullMessage = 0;

  /* Decode "CC Option" */
  CCOption = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);
  state->NxdnElementsContent.CCOption = CCOption;

  /* Decode "Call Type" */
  CallType = (uint8_t)ConvertBitIntoBytes(&Message[16], 3);
  state->NxdnElementsContent.CallType = CallType;

  /* Decode "Voice Call Option" */
  VoiceCallOption = (uint8_t)ConvertBitIntoBytes(&Message[19], 5);
  state->NxdnElementsContent.VoiceCallOption = VoiceCallOption;

  /* Decode "Source Unit ID" */
  SourceUnitID = (uint16_t)ConvertBitIntoBytes(&Message[24], 16);
  state->NxdnElementsContent.SourceUnitID = SourceUnitID;

  /* Decode "Destination ID" */
  DestinationID = (uint16_t)ConvertBitIntoBytes(&Message[40], 16);
  state->NxdnElementsContent.DestinationID = DestinationID;

  /* Decode the "Cipher Type" */
  CipherType = (uint8_t)ConvertBitIntoBytes(&Message[56], 2);
  state->NxdnElementsContent.CipherType = CipherType;

  /* Decode the "Key ID" */
  KeyID = (uint8_t)ConvertBitIntoBytes(&Message[58], 6);
  state->NxdnElementsContent.KeyID = KeyID;

  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  if((CipherType == 2) || (CipherType == 3))
  {
    state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 1;
    state->NxdnElementsContent.PartOfNextEncryptedFrame    = 2;
  }
  else
  {
    state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 1;
    state->NxdnElementsContent.PartOfNextEncryptedFrame    = 1;
  }

  /* Print the "Call Type" */
  fprintf (stderr, "%s", KGRN);
  fprintf(stderr, "\n %s - ", NXDN_Call_Type_To_Str(CallType)); 
  sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType)); 

  /* Print the "Voice Call Option" */
  NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  fprintf(stderr, "%s %s - ", DuplexMode, TransmissionMode);

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);
  fprintf (stderr, "%s", KNRM);

  //check the rkey array for a scrambler key value
  //check by keyid first, then by tgt id
  //TGT ID and Key ID could clash though if csv or system has both with different keys
  if (state->rkey_array[KeyID] != 0) state->R = state->rkey_array[KeyID];
  else if (state->rkey_array[DestinationID] != 0) state->R = state->rkey_array[DestinationID];

  //consider removing code below, if we get this far (under good crc),
  //then we will always have a correct cipher type and won't need to
  //force it, its only needed on the vcall_assgn where no cipher is set
  //dsd_mbe can also force the scrambler without triggering the settings here

  // if (state->M == 1)
  // {
  //   state->nxdn_cipher_type = 0x1;
  //   CipherType = 0x1;
  // }

  //debug
  // if (CipherType > 0x1) state->R = 0; //don't use on manual key entry

  /* Print the "Cipher Type" */
  if(CipherType != 0)
  {
    fprintf (stderr, "\n  %s", KYEL);
    fprintf(stderr, "%s - ", NXDN_Cipher_Type_To_Str(CipherType));
  }

  /* Print the Key ID */
  if(CipherType != 0)
  {
    fprintf(stderr, "Key ID %u - ", KeyID & 0xFF);
    fprintf (stderr, "%s", KNRM);
  }


  if (state->nxdn_cipher_type == 0x01 && state->R > 0) //scrambler key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %05lld", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  //only grab if CRC is okay
  if(state->NxdnElementsContent.VCallCrcIsGood)
  {
    if ( (SourceUnitID & 0xFFFF) > 0 ) //
    {
      state->nxdn_last_rid = SourceUnitID & 0xFFFF;   
      state->nxdn_last_tg = (DestinationID & 0xFFFF);
      state->nxdn_key = (KeyID & 0xFF);
      state->nxdn_cipher_type = CipherType; 
    }
  }
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf(stderr, "(CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

  //set enc bit here so we can tell playSynthesizedVoice whether or not to play enc traffic
  if (state->nxdn_cipher_type != 0)
  {
    state->dmr_encL = 1;
  }
  if (state->nxdn_cipher_type == 0 || state->R != 0)
  {
    state->dmr_encL = 0;
  }
} /* End NXDN_decode_VCALL() */

void NXDN_decode_VCALL_IV(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint32_t i;
  state->payload_miN = 0; //zero out
  
  /* Extract the IV from the VCALL_IV message */
  for(i = 0; i < 8; i++)
  {
    state->NxdnElementsContent.IV[i] = (uint8_t)ConvertBitIntoBytes(&Message[(i + 1) * 8], 8);

    state->payload_miN = state->payload_miN << 8 | state->NxdnElementsContent.IV[i];
  }

  state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 2;
  state->NxdnElementsContent.PartOfNextEncryptedFrame    = 1;

} /* End NXDN_decode_VCALL_IV() */


char * NXDN_Call_Type_To_Str(uint8_t CallType)
{
  char * Ptr = NULL;

  switch(CallType)
  {
    case 0:  Ptr = "Broadcast Call";    break;
    case 1:  Ptr = "Group Call";        break;
    case 2:  Ptr = "Unspecified Call";  break;
    case 3:  Ptr = "Reserved";          break;
    case 4:  Ptr = "Individual Call";   break;
    case 5:  Ptr = "Reserved";          break;
    case 6:  Ptr = "Interconnect Call"; break;
    case 7:  Ptr = "Speed Dial Call";   break;
    default: Ptr = "Unknown Call Type"; break;
  }

  return Ptr;
} /* End NXDN_Call_Type_To_Str() */

void NXDN_Voice_Call_Option_To_Str(uint8_t VoiceCallOption, uint8_t * Duplex, uint8_t * TransmissionMode)
{
  char * Ptr = NULL;

  Duplex[0] = 0;
  TransmissionMode[0] = 0;

  if(VoiceCallOption & 0x10) strcpy((char *)Duplex, "Duplex");
  else strcpy((char *)Duplex, "Half Duplex");

  switch(VoiceCallOption & 0x17)
  {
    case 0: Ptr = "4800bps/EHR"; break;
    case 2: Ptr = "9600bps/EHR"; break;
    case 3: Ptr = "9600bps/EFR"; break;
    default: Ptr = "Reserved Voice Call Option";  break;
  }

  strcpy((char *)TransmissionMode, Ptr);
} /* End NXDN_Voice_Call_Option_To_Str() */

char * NXDN_Cipher_Type_To_Str(uint8_t CipherType)
{
  char * Ptr = NULL;

  switch(CipherType)
  {
    case 0:  Ptr = "";          break;  /* Non-ciphered mode / clear call */
    case 1:  Ptr = "Scrambler"; break;
    case 2:  Ptr = "DES";       break;
    case 3:  Ptr = "AES";       break;
    default: Ptr = "Unknown Cipher Type"; break;
  }

  return Ptr;
} /* End NXDN_Cipher_Type_To_Str() */