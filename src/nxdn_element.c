/*-------------------------------------------------------------------------------
* nxdn_element.c
* NXDN Message Decoding
*
* Reworked portions from NXDN decoding source lib - modified from nxdn_lib
* Originally found at - https://github.com/LouisErigHerve/dsd
*
* LWVMOBILE
* 2026-01 DSD-FME Florida Man Edition
*-----------------------------------------------------------------------------*/

#include "dsd.h"

//below items are in cmake as -DASCII=ON (default), or -DBIG5=ON , -DSJIS=ON , or -DUTF16=ON
// #define ASCII_DECODE //if using older ASCII only Alias Decoder, and not new one.
// #define SJIS_DECODE  //if using ASCII/SJIS decoder, and not BIG5 decoder
// #define BIG5_DECODE  //if using ASCII/BIG5 decoder, and not SJIS decoder
// #define UTF16_DECODE //if using straight UTF16 with Alias16 decoder (untested/not observed)

void NXDN_SACCH_Full_decode(dsd_opts * opts, dsd_state * state)
{
  //below sizes are matching largest message carrier (facch2) when
  //sending to content element decoder, we don't want to go address a memory range out of bounds
  //so this is a safeguard to prevent doing so. This is same as viterbi bytes and bits sizes.
  uint8_t SACCH[26*8]; //72
  uint8_t sacch_bytes[26]; //9

  uint32_t i;
  uint8_t CrcCorrect = 1;

  memset (SACCH, 0, sizeof (SACCH));
  memset (sacch_bytes, 0, sizeof (sacch_bytes));

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
  // else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, SACCH);

  //reset the sacch field -- Github Issue #118
  memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
  memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n");
    fprintf (stderr, " Full SACCH Payload ");
    for (i = 0; i < 9; i++)
    {
      sacch_bytes[i] = (uint8_t)ConvertBitIntoBytes(&SACCH[i*8], 8);
      fprintf (stderr, "[%02X]", sacch_bytes[i]);
    }
  }


} /* End NXDN_SACCH_Full_decode() */

void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state, uint8_t CrcCorrect, uint8_t * ElementsContent)
{

  //Including Flags on bit 0 and bit 1 for more precise usage below
  uint8_t MessageType = (uint8_t)convert_bits_into_output(ElementsContent+0, 8);

  nxdn_message_type (opts, state, MessageType);

  /* Set the CRC state */
  state->NxdnElementsContent.VCallCrcIsGood = CrcCorrect;

  /* Decode the right "Message Type" */
  switch(MessageType)
  {

    /*
    //Note: CAC Message with same Message Type -- This is a private call request and rejection (TODO: Seperate handling depending on CAC, FACCH< Sacch, etc)
    20:56:15 Sync: NXDN96  RCCH  Data   RAN 01  CAC VCALL (VCALL_REQ)
      Private Call - Half Duplex 9600bps/EHR (02) - Src=211 - Dst/TG=1603
    20:56:15 Sync: NXDN96  RCCH  Data   RAN 01  CAC DISC (VCALL_REJECTION)
      Private Call -        Disconnect       - Src=1603 - Dst/TG=211

    */
    //Debug: Disable DUP messages if they cause random issues with Type-C trunking (i.e. changing SRC ang TGT IDs, hopping in the middle of calls, etc)

    //observed new messages in #318, should also be noted that F1 and F2 are both set on these messages

    //VCALL and TX_REL custom to certain radios, but they have different elements in them
    case 0xE1:
    case 0xE8:
      NXDN_decode_VCALL_ARIB(opts, state, ElementsContent);
      break;

    //Shift-JIS Talker Alias
    case 0xE7:
      NXDN_decode_ALIAS_ARIB(opts, state, ElementsContent);
      break;

    //end observations from #318

    //VCALL_ASSGN_DUP
    case 0x05:

    //DCALL_ASSGN_DUP
    case 0x0D:

    //VCALL_ASSGN
    case 0x04:

    //DCALL_ASSGN
    case 0x0E:
      NXDN_decode_VCALL_ASSGN(opts, state, ElementsContent);
      break;

    //PROP_FORM 0x3F
    case 0x3F:
      NXDN_decode_Prop(opts, state, ElementsContent);
      break;

    //SRV_INFO
    case 0x19:
      state->nxdn_last_rid = 0;
      state->nxdn_last_tg = 0;
      NXDN_decode_srv_info(opts, state, ElementsContent);
      break;

    //CCH_INFO
    case 0x1A:
      NXDN_decode_cch_info(opts, state, ElementsContent);
      break;

    //DST_ID_INFO
    case 0x17:
      nxdn_decode_dst_info(opts, state, ElementsContent);
      break;

    //SITE_INFO
    case 0x18:
      NXDN_decode_site_info(opts, state, ElementsContent);
      break;

    //ADJ_SITE_INFO
    case 0x1B:
      NXDN_decode_adj_site(opts, state, ElementsContent);
      break;

    //VCALL, TX_REL_EXT and TX_REL
    case 0x07: //TX_REL_EXT
    case 0x08: //TX_REL
      sprintf (state->call_string[0], "%s", "");
      sprintf (state->nxdn_call_type, "%s", "");
    case 0x01: //VCALL
      NXDN_decode_VCALL(opts, state, ElementsContent);
      break;

    //DISC
    case 0x11:
      NXDN_decode_VCALL(opts, state, ElementsContent);
      memset (state->generic_talker_alias[0], 0, sizeof(state->generic_talker_alias[0]));
      sprintf (state->generic_talker_alias[0], "%s", "");
      sprintf (state->call_string[0], "%s", "");
      sprintf (state->nxdn_call_type, "%s", "");

      //tune back to CC here - save about 1-2 seconds
      if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && opts->p25_is_tuned == 1)
      {
        //rigctl
        if (opts->use_rigctl == 1)
        {
          //extra safeguards due to sync issues with NXDN
          memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
          memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
          memset(state->active_channel, 0, sizeof(state->active_channel));
          opts->p25_is_tuned = 0;
          if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
          SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);

          state->nxdn_last_rid = 0;
          state->nxdn_last_tg = 0;
          if (state->forced_alg_id == 0)
            state->nxdn_cipher_type = 0;
          sprintf (state->nxdn_call_type, "%s", "");

        }
        //rtl
        else if (opts->audio_in_type == 3)
        {
          #ifdef USE_RTLSDR
          //extra safeguards due to sync issues with NXDN
          memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
          memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
          memset(state->active_channel, 0, sizeof(state->active_channel));
          opts->p25_is_tuned = 0;
          rtl_dev_tune (opts, state->p25_cc_freq);

          state->nxdn_last_rid = 0;
          state->nxdn_last_tg = 0;
          if (state->forced_alg_id == 0)
            state->nxdn_cipher_type = 0;
          sprintf (state->nxdn_call_type, "%s", "");
          #endif
        }

        state->last_cc_sync_time = time(NULL); //allow tuners a second to catch up, particularly rtl input
      }

      // #endif
      break;

    //Idle
    case 0x10:
      break;

    //VCALL_IV
    case 0x03:
      NXDN_decode_VCALL_IV(opts, state, ElementsContent);
      break;

    /* Unknown Message Type */
    default:
    {
      break;
    }
  } /* End switch(MessageType) */

} /* End NXDN_Elements_Content_decode() */

//externalize multiple sub-element handlers
void nxdn_location_id_handler (dsd_state * state, uint32_t location_id, uint8_t type)
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

  //type 0 is for current site, type 1 is for adjacent sites
  if (type == 0)
  {
    state->nxdn_last_ran = site_code % 64; //Table 6.3-4 RAN for Trunked Radio Systems
    if (site_code != 0) state->nxdn_location_site_code = site_code;
    if (sys_code != 0) state->nxdn_location_sys_code = sys_code;
    sprintf (state->nxdn_location_category, "%s", category);
  }

  if (type == 0) fprintf (stderr, "\n Location Information - Cat: %s - Sys Code: %d - Site Code %d ", category, sys_code, site_code);
  else fprintf (stderr, "\n Adjacent Information - Cat: %s - Sys Code: %d - Site Code %d ", category, sys_code, site_code);

}

void nxdn_srv_info_handler (dsd_state * state, uint16_t svc_info)
{
  UNUSED(state);
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
  UNUSED(state);

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
  uint32_t base = (ca_info >> 18) & 0x7; //Base Frequency
  uint32_t spare = ca_info & 0x3FF;
  UNUSED(spare);

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
  UNUSED(CallTimer);

  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};

  uint8_t MessageType;
  /* Get the "Message Type" field */
  MessageType  = (Message[2] & 1) << 5;
  MessageType |= (Message[3] & 1) << 4;
  MessageType |= (Message[4] & 1) << 3;
  MessageType |= (Message[5] & 1) << 2;
  MessageType |= (Message[6] & 1) << 1;
  MessageType |= (Message[7] & 1) << 0;

  if (MessageType == 0x4) fprintf (stderr, "%s", KGRN);       //VCALL_ASSGN
  else if (MessageType == 0x05) fprintf (stderr, "%s", KGRN); //VCALL_ASSGN_DUP
  else if (MessageType == 0x0E) fprintf (stderr, "%s", KCYN); //DCALL_ASSGN
  else if (MessageType == 0x0D) fprintf (stderr, "%s", KCYN); //DCALL_ASSGN_DUP

  //DFA specific variables
  uint8_t  bw = 0;
  uint16_t OFN = 0;
  uint16_t IFN = 0;
  UNUSED2(bw, IFN);

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

  //Part 1-E Common Air Interface Ver.1.3 - 6.4.1.23. Voice Call Assignment (VCALL_ASSGN)
  //While I've only seen Busy Repeater Message on the SCCH message (different format)
  //The manual suggests this message exists on Type-D systems with this configuration

  //On Type-D systems, need to truncate to an 11-bit value
  //other 5-bits are repeater or prefix value
  // uint8_t idas = 0;
  // uint8_t rep1 = 0;
  // uint8_t rep2 = 0;
  // if (strcmp (state->nxdn_location_category, "Type-D") == 0) idas = 1;
  // if (idas)
  // {
  //   rep1 = (SourceUnitID >> 11) & 0x1F;
  //   rep2 = (DestinationID >> 11) & 0x1F;
  //   SourceUnitID = SourceUnitID & 0x7FF;
  //   DestinationID = DestinationID & 0x7FF;
  //   //assign source prefix/rep1 as the tune to channel?
  //   //In normal VCALL, both prefix/rep values are the same on the Type-D samples I have
  //   Channel = rep1;
  // }

  if (MessageType == 0x4 || MessageType == 0x5)
    fprintf (stderr, "%s", KGRN);     //VCALL_ASSGN
  else fprintf (stderr, "%s", KCYN); //DCALL_ASSGN

  fprintf (stderr, "\n ");

  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  /* Print the "Call Type" */
  fprintf(stderr, "%s - ", NXDN_Call_Type_To_Str(CallType));

  /* Print the "Voice Call Option" */
  if (MessageType == 0x4 || MessageType == 0x5) NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  if (MessageType == 0x4 || MessageType == 0x5) fprintf(stderr, "%s %s (%02X) - ", DuplexMode, TransmissionMode, VoiceCallOption);
  else fprintf (stderr, "   Data Call Assignment (%02X) - ", VoiceCallOption); //DCALL_ASSGN or DCALL_ASSGN_DUP

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);

  //Channel here appears to be the prefix or home channel of the caller, not necessarily the channel the call is occuring on
  // if (idas) fprintf (stderr, "- Prefix Ch: %d ", rep1);

  /* Print Channel */
  if (state->nxdn_rcn == 0)
    fprintf(stderr, "- Channel [%03X][%04d] ", Channel & 0x3FF, Channel & 0x3FF);
  if (state->nxdn_rcn == 1)
    fprintf(stderr, "- DFA Channel [%04X][%05d] ", OFN, OFN);

  //test VCALL_ASSGN_DUP, if no voice sync activity (by trunk_hangtime), then convert to assgn and allow tuning
  //VCALL_ASSGN_DUP has been seen in the middle of calls, but also on the tail end instead of a TX_REL or DISC
  if (MessageType == 0x5 && opts->p25_is_tuned == 1 && opts->p25_trunk == 1)
  {
    if ( (time(NULL) - state->last_vc_sync_time) > opts->trunk_hangtime )
    {
      MessageType = 0x04; //convert to VCALL
      opts->p25_is_tuned = 0; //open tuning back up to tune
    }
  }

  //TG Hold during VCALL_ASSGN_DUP, allow tuning to TG hold channel assignment
  if (MessageType == 0x5 && opts->p25_is_tuned == 1 && opts->p25_trunk == 1)
  {
    if ( state->tg_hold != 0 && state->tg_hold == DestinationID )
    {
      MessageType = 0x04; //convert to VCALL
      // opts->p25_is_tuned = 0; //open tuning back up to tune
    }
  }

  //use DUP to display any other rolling active channels while on a call (from the dup message)
  int dup = 0;
  if (MessageType == 0x5) dup = 1;

  //assign active call to string (might place inside of tune decision to get multiple ones?)
  if (state->nxdn_rcn == 0)
    sprintf (state->active_channel[dup], "Active Ch: %d TG: %d SRC: %d; ", Channel, DestinationID, SourceUnitID);
  if (state->nxdn_rcn == 1)
    sprintf (state->active_channel[dup], "Active Ch: %d TG: %d SRC: %d; ", OFN, DestinationID, SourceUnitID);

  state->last_active_time = time(NULL);

  //Add support for tuning data and group/private calls on trunking systems
  uint8_t tune = 0;

  //DCALL_ASSGN and DCALL_ASSGN_DUP
  if (MessageType == 0x0D || MessageType == 0x0E )
  {
    if (opts->trunk_tune_data_calls == 1) tune = 1;
  }
  else if (MessageType == 0x04) //VCALL_ASSGN and converted VCALL_ASSGN_DUP
  {
    if (CallType == 4) //individual/private call
    {
      if (opts->trunk_tune_private_calls) tune = 1;
    }
    else if (opts->trunk_tune_group_calls) tune = 1;
  }

  if (tune == 0) goto END_ASSGN;

  //run process to figure out frequency value from the channel import or from DFA
  long int freq = 0;
  if (state->nxdn_rcn == 0)
    freq = nxdn_channel_to_frequency(opts, state, Channel);
  if (state->nxdn_rcn == 1)
    freq = nxdn_channel_to_frequency(opts, state, OFN);

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
  sprintf (mode, "%s", "");

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

  for (int i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == DestinationID && DestinationID != 0) //destination, if it isn't 0
    {
      fprintf (stderr, " [%s]", state->group_array[i].groupName);
      strcpy (mode, state->group_array[i].groupMode);
      break;
    }
    //might not be ideal if both source and group/target are both in the array
    else if (state->group_array[i].groupNumber == SourceUnitID && DestinationID == 0) //source, if destination is 0
    {
      fprintf (stderr, " [%s]", state->group_array[i].groupName);
      strcpy (mode, state->group_array[i].groupMode);
      break;
    }
  }

  //check purely by SourceUnitID as last resort -- this is a bugfix to block individual radios on selected systems
  if ((strcmp(mode, "") == 0))
  {
    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == SourceUnitID)
      {
        fprintf (stderr, " [%s]", state->group_array[i].groupName);
        strcpy (mode, state->group_array[i].groupMode);
        break;
      }
    }
  }

  //TG hold on NXDN -- block non-matching target, allow matching DestinationID
  if (state->tg_hold != 0 && state->tg_hold != DestinationID) sprintf (mode, "%s", "B");
  if (state->tg_hold != 0 && state->tg_hold == DestinationID)
  {
    sprintf (mode, "%s", "A");
    opts->p25_is_tuned = 0; //unlock tuner at this stage and not above check
  }

  //check to see if the source/target candidate is blocked first
  if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
  {
    if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
    {
      //rigctl
      if (opts->use_rigctl == 1)
      {
        //extra safeguards due to sync issues with NXDN
        memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		    state->lastsynctype = -1;
        state->last_cc_sync_time = time(NULL);
        state->last_vc_sync_time = time(NULL);
        //

        if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
        SetFreq(opts->rigctl_sockfd, freq);
        state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
        opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop

        //set rid and tg when we actually tune to it
        //only assign rid if not spare and not reserved (happens on private calls, unsure of its significance)
        if ( (VoiceCallOption & 0xF) < 4) //ideally, only want 0, 2, or 3
          state->nxdn_last_rid = SourceUnitID;
        state->nxdn_last_tg = DestinationID;
        sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));

        if (CallType == 3)
        {
          state->gi[0] = 1; //Private Call
          //unassign these, sometimes, when trunking, these may be reversed by the time listened,
          //and this will plant an extra private call in the event_history
          state->nxdn_last_rid = 0;
          state->nxdn_last_tg = 0;
        }
        else state->gi[0] = 0; //Group Call

        //Call String for Per Call WAV File
        sprintf (state->call_string[0], "%s", NXDN_Call_Type_To_Str(CallType));
        if (CCOption & 0x80) strcat (state->call_string[0], " Emergency");

        //check the rkey array for a scrambler key value
        //TGT ID and Key ID could clash though if csv or system has both with different keys
        if (state->rkey_array[DestinationID] != 0)
        {
          state->R = state->rkey_array[DestinationID];
          fprintf (stderr, " %s", KYEL);
          fprintf (stderr, " Key Loaded: %lld", state->rkey_array[DestinationID]);
          state->payload_miN = state->R; //should be okay to load here, will test
        }
        if (state->forced_alg_id == 1) state->nxdn_cipher_type = 0x1;
      }
      //rtl
      else if (opts->audio_in_type == 3)
      {
        #ifdef USE_RTLSDR
        //extra safeguards due to sync issues with NXDN
        memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		    state->lastsynctype = -1;
        state->last_cc_sync_time = time(NULL);
        state->last_vc_sync_time = time(NULL);
        //

        rtl_dev_tune (opts, freq);
        state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
        opts->p25_is_tuned = 1;

        //set rid and tg when we actually tune to it
        //only assign rid if not spare and not reserved (happens on private calls, unsure of its significance)
        if ( (VoiceCallOption & 0xF) < 4) //ideally, only want 0, 2, or 3
          state->nxdn_last_rid = SourceUnitID;
        state->nxdn_last_tg = DestinationID;
        sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));

        if (CallType == 3)
        {
          state->gi[0] = 1; //Private Call
          //unassign these, sometimes, when trunking, these may be reversed by the time listened,
          //and this will plant an extra private call in the event_history
          state->nxdn_last_rid = 0;
          state->nxdn_last_tg = 0;
        }
        else state->gi[0] = 0; //Group Call

        //Call String for Per Call WAV File
        sprintf (state->call_string[0], "%s", NXDN_Call_Type_To_Str(CallType));
        if (CCOption & 0x80) strcat (state->call_string[0], " Emergency");

        //check the rkey array for a scrambler key value
        //TGT ID and Key ID could clash though if csv or system has both with different keys
        if (state->rkey_array[DestinationID] != 0)
        {
          state->R = state->rkey_array[DestinationID];
          fprintf (stderr, " %s", KYEL);
          fprintf (stderr, " Key Loaded: %lld", state->rkey_array[DestinationID]);
          state->payload_miN = state->R; //should be okay to load here, will test
        }
        if (state->forced_alg_id == 1) state->nxdn_cipher_type = 0x1;
        #endif
      }

    }
  }

  END_ASSGN: ; //do nothing
  fprintf (stderr, "%s", KNRM);

} /* End NXDN_decode_VCALL_ASSGN() */

//Multi Encoding Alias Decoder (ASCII, and SJIS or BIG5 or UTF16BE)
void NXDN_decode_Alias16(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  uint8_t mfid = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);
  uint16_t unk = (uint16_t)ConvertBitIntoBytes(&Message[16], 16);

  uint8_t seg_num = (uint8_t)ConvertBitIntoBytes(&Message[32], 4);
  uint8_t seg_len = (uint8_t)ConvertBitIntoBytes(&Message[36], 4);

  uint8_t seg_bytes[12]; memset(seg_bytes, 0, sizeof(seg_bytes));

  int seg_byte_num = 4;
  for (int i = 0; i < seg_byte_num; i++)
    seg_bytes[i] = (uint8_t)ConvertBitIntoBytes(&Message[(i*8)+40], 8);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n Multi Segment Alias %d/%d; ", seg_num, seg_len);
    for (int i = 0; i < seg_byte_num; i++)
      fprintf (stderr, "%02X", seg_bytes[i]);
  }
  else fprintf (stderr, " (%d/%d) ", seg_num, seg_len);

  //copy to PDU superframe
  memcpy(state->dmr_pdu_sf[0]+(seg_num-1)*seg_byte_num, seg_bytes, seg_byte_num*sizeof(uint8_t));

  int len = seg_byte_num * seg_len;

  //TODO: Appears to have embedded 2 byte crc, populated up to 12 bits
  //not a match (may need bitwise, or may need entire messages put together and not just alias)
  // uint16_t crc_chk = crc12f(state->dmr_pdu_sf[0], 12);

  //extract CRC
  uint16_t crc_ext = (state->dmr_pdu_sf[0][len-2] << 8) | (state->dmr_pdu_sf[0][len-1] << 0);

  //remove CRC bytes
  if (len > 2)
    len -= 2;

  if (seg_num == seg_len)
  {
    if (opts->payload == 1)
    {
      fprintf (stderr, "\n Completed Alias Encoded: ");
      for (int i = 0; i < (seg_len*seg_byte_num); i++)
        fprintf (stderr, "%02X", state->dmr_pdu_sf[0][i]);
    }

    //Alias String
    char alias[500]; memset(alias, 0, sizeof(alias));
    int ptr = 0;        //pointer to position in alias buffer
    int k = 0;          //pointer to position in completed message
    uint16_t c16b = 0;  //16-bit character encoded in BIG5 or SJIS
    uint8_t  utf8 = 0;  //UTF-8 Character
    uint32_t uni = 0;   //Unicode Character returned from BIG5, or SJIS decoder

    //SHIFT-JIS or BIG5 to UNICODE Conversion
    setlocale(LC_ALL, ""); //needed when encoded alias contains CJK

    if (opts->payload == 1)
      fprintf (stderr, "\n MFID: %02X; Unk: %04X; Encoded Len: %d; CRC: %03X; Alias: ", mfid, unk, len, crc_ext);
    
    for (int i = 0; i < len; i++)
    {
      
      uni  = 0;
      utf8 = state->dmr_pdu_sf[0][k];
      c16b = (state->dmr_pdu_sf[0][k+0] << 8) | state->dmr_pdu_sf[0][k+1];

      if (utf8 >= 0x20 && utf8 < 0x80)
      {
        uni = sjis_char_to_unicode(utf8);
        fprintf (stderr, "%lc", uni);
        k++;
      }
      else if (c16b != 0)
      {
        #ifdef BIG5_DECODE //BIG5 Tradional Chinese
        uni = big5_char_to_unicode(c16b);
        #elif SJIS_DECODE //SJIS for Japanese
        uni = sjis_char_to_unicode(c16b);
        #elif UTF16_DECODE //try as UTF16BE (untested through Unicode to UTF8 decoder)
        uni = c16b; //UTF16BE
        // uni = (state->dmr_pdu_sf[0][k+1] << 8) | state->dmr_pdu_sf[0][k+0]; //UTF16LE (for test only)
        #endif
        fprintf (stderr, "%lc", uni);
        k+=2;
      }
      else if (utf8 == 0 && c16b == 0)
        break;

      //Encode Unicode to UTF-8 if not a pass-through ASCII control character or 0
      if (uni >= 0x0020)
        ptr += utf8_encode(uni, alias+ptr);

    }

    //terminate string failsafe
    alias[ptr++] = 0x00;
    alias[ptr++] = 0x00;

    sprintf (state->generic_talker_alias[0], "%s", alias);
    sprintf (state->event_history_s[0].Event_History_Items[0].alias, "%s; ", alias);

    //reset PDU superframe afterwards
    memset(state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
  }

} /* End NXDN_decode_Alias16 */

void NXDN_decode_Prop(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  fprintf (stderr, "%s", KYEL);

  //Octet 1 is MFID value
  uint8_t mfid = (uint8_t)convert_bits_into_output(Message+8, 8);

  //check this message to see if it is an alias, or other unknown PROP_FORM
  uint32_t alias_check = (uint16_t)convert_bits_into_output(Message+16, 16);

  if (mfid == 0x68 && alias_check == 0x8204)
  {
    fprintf(stderr, " ALIAS");
    #ifdef BIG5_DECODE
    NXDN_decode_Alias16(opts, state, Message); //ASCII and 16-bit SJIS or BIG5 (depends on cmake define)
    #elif SJIS_DECODE
    NXDN_decode_Alias16(opts, state, Message); //ASCII and 16-bit SJIS or BIG5 (depends on cmake define)
    #elif UTF16_DECODE
    NXDN_decode_Alias16(opts, state, Message); //ASCII and 16-bit UTF16BE (untested) (depends on cmake define)
    #elif ASCII_DECODE
    NXDN_decode_Alias(opts, state, Message);   //ASCII only version
    #else
    NXDN_decode_Alias(opts, state, Message);   //ASCII only version (fallback if none selected by cmake)
    #endif
  }
  else
  {
    fprintf(stderr, " PROP_FORM");
    fprintf (stderr, "\n");
    fprintf (stderr, " MFID: %02X; Message: ", mfid);
    for (int i = 2; i < 9; i++)
      fprintf (stderr, "%02X", (uint8_t)convert_bits_into_output(Message+(i*8), 8));
  }

  fprintf (stderr, "%s", KNRM);

}

//This version works well with ASCII, but no support for other encodings
void NXDN_decode_Alias(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  UNUSED(opts);

  uint8_t CrcCorrect = 0;

  //alias can also hit on a facch1 -- do we still need this checkdown?
  if (state->nxdn_sacch_non_superframe == FALSE)
    CrcCorrect = state->NxdnElementsContent.VCallCrcIsGood;
  else CrcCorrect = 1; //FACCH1 with bad CRC won't make it this far anyways, so set as 1

  // uint8_t mfid = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);  //PROP_FORM MFID value
  // uint8_t unk1 = (uint8_t)ConvertBitIntoBytes(&Message[16], 8); //82
  // uint8_t unk2 = (uint8_t)ConvertBitIntoBytes(&Message[24], 8); //04

  uint8_t blocknumber = (uint8_t)ConvertBitIntoBytes(&Message[32], 4) & 0x7;
  uint8_t total = (uint8_t)ConvertBitIntoBytes(&Message[36], 4) & 0x7;

  //crc errs in one repitition may occlude an otherwise good alias, so test and change if needed
  //completed alias should still appear in ncurses terminal regardless, so this may be okay
  if (CrcCorrect)
  {
    fprintf (stderr, " (%d/%d) ", blocknumber, total);

    blocknumber--;
    total--;

    int ptr = blocknumber*4;

    for (int i = 0; i < 4; i++)
      state->generic_talker_alias[0][ptr++] = (char)ConvertBitIntoBytes(&Message[40+(i*8)], 8);

    //on block 4, the last two (14, and 15) will not be an alias value, but a CRC value (seemingly)
    state->generic_talker_alias[0][14] = '\0';
    state->generic_talker_alias[0][15] = '\0';

    //comb alias so far, if characters greater than ASCII 0x7F, this may be a UTF16 encoded alias
    //May be more beneficial in long run to use the ARIB alias with SJIS and/or UTF16 to UTF8 in it.
    //NOTE: This will not prevent 'splicing' issues if signal issue occurs and resumes with another alias
    //this alias string is cleared on TX_REL, TX_REL_EXT, noCarrier, and few other items
    for (int i = 0; i < 14; i++)
    {
      //substitute with a space if early terminator (partial decode) or greater than end of ASCII
      if (state->generic_talker_alias[0][i] < 0x20 || state->generic_talker_alias[0][i] > 0x7F)
        state->generic_talker_alias[0][i] = 0x20;
    }

    //walk backwards, remove any ending spaces with a terminator
    for (int i = 13; i >= 1; i--)
    {
      if (state->generic_talker_alias[0][i] == 0x20)
        state->generic_talker_alias[0][i] = '\0';

      if (state->generic_talker_alias[0][i-1] != 0x20)
        break;
    }

    sprintf (state->event_history_s[0].Event_History_Items[0].alias, "%s; ", state->generic_talker_alias[0]);

    setlocale(LC_ALL, "");
    // if (blocknumber == total)
    {
      if (opts->payload == 1)
        fprintf (stderr, "\n Talker Alias: %s", state->generic_talker_alias[0]);
      else fprintf (stderr, "%s", state->generic_talker_alias[0]);
    }

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
  UNUSED2(channel2sts, freq2);

  //DFA
  uint8_t  bw1 = 0;
  uint16_t OFN1 = 0;
  uint16_t IFN1 = 0;
  uint8_t  bw2 = 0;
  uint16_t OFN2 = 0;
  uint16_t IFN2 = 0;
  UNUSED(bw2);

  location_id = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
  channel1sts = (uint8_t)ConvertBitIntoBytes(&Message[32], 6);
  channel1 =    (uint16_t)ConvertBitIntoBytes(&Message[38], 10);
  channel2sts = (uint8_t)ConvertBitIntoBytes(&Message[48], 6);
  channel2 =    (uint16_t)ConvertBitIntoBytes(&Message[54], 10);

  fprintf (stderr, "%s", KYEL);
  nxdn_location_id_handler (state, location_id, 0);

  fprintf (stderr, "\n Control Channel Information \n");

  //Channel version
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

    fprintf (stderr, "  Location ID [%06X] OFN1 [%04X][%05d] IFN1 [%04X][%05d] ", location_id, OFN1, OFN1, IFN1, IFN1);

    //facch1 will not have the below items -- should be NULL or 0 if not available
    bw2  = (uint8_t)ConvertBitIntoBytes(&Message[78], 2);
    OFN2 = (uint16_t)ConvertBitIntoBytes(&Message[80], 16);
    IFN2 = (uint16_t)ConvertBitIntoBytes(&Message[96], 16);

    if (OFN2 && IFN2)
    {
      fprintf (stderr, "OFN2 [%04X][%05d] IFN2 [%04X][%05d]", OFN2, OFN2, IFN2, IFN2);
    }

    fprintf (stderr, "Status: ");
    if (channel1sts & 0x10) fprintf (stderr, "New ");
    if (channel1sts & 0x02) fprintf (stderr, "Current 1 ");
    if (channel1sts & 0x01) fprintf (stderr, "Current 2 ");

    //willing to assume that bw1 and bw2 would both be the same value
    if (bw1 == 0) fprintf (stderr, "BW: 6.25 kHz - 4800 bps");
    else if (bw1 == 1) fprintf (stderr, "BW: 12.5 kHz - 9600 bps");
    else fprintf (stderr, "BW: %d Reserved Value", bw1);

    freq1 = nxdn_channel_to_frequency (opts, state, OFN1);
    nxdn_channel_to_frequency (opts, state, IFN1);

    //run second -- if available and not equal to first
    if (OFN2 && IFN2 && OFN2 != OFN1)
    {
      nxdn_channel_to_frequency (opts, state, OFN2);
      nxdn_channel_to_frequency (opts, state, IFN2);
    }

    //add to lcn freq for hunting -- only when using pure DFA and not importing
    if (state->trunk_lcn_freq[0] == 0 && freq1 != 0)
    {
      state->trunk_lcn_freq[0] = freq1;
      state->p25_cc_freq = freq1;
      state->lcn_freq_count = 1;
    }

  }

  fprintf (stderr, "%s", KNRM);
}

void nxdn_decode_dst_info(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  char station_id_string[26]; //FACCH2 maxes out at 26 (largest message carrier)
  memset(station_id_string, 0, sizeof(station_id_string));

  uint8_t start = Message[8];
  uint8_t end   = Message[9];
  uint8_t option = convert_bits_into_output(Message+8, 8);
  uint8_t num_chars = convert_bits_into_output(Message+10, 6);

  //arbitrary value to stop on if not the first (not sure why first and not last? probably should be last)
  if (!start)
    num_chars = 24;
  
  for (int i = 0; i < num_chars+1; i++)
  {
    uint8_t c = convert_bits_into_output(Message+16+(i*8), 8);
    if (c >= 0x20 && c <= 0x7F) //in range of ASCII characters
      station_id_string[i] = c;
  }

  station_id_string[25] = '\0';

  fprintf (stderr, "%s", KYEL);

  fprintf (stderr, "\n Station Identification Information - ");

  if (start && end)
    fprintf (stderr, "Full ");
  else if (start)
    fprintf (stderr, "First ");
  else if (end)
    fprintf (stderr, "Last ");
  else fprintf (stderr, "Next ");

  fprintf (stderr, "ID: %s ", station_id_string);

  //push as a data event string
  if (start && end)
  {
    char event_string[55]; memset(event_string, 0, sizeof(event_string));
    sprintf (event_string, "NXDN Digital Station ID: %s", station_id_string);
    watchdog_event_datacall (opts, state, 65520, 0, event_string, 0);
  }

  //debug
  if (opts->payload == 1)
    fprintf (stderr, "\n Option: %02X; Start: %d; End %d; Characters: %d or Sequence: %02X;", option, start, end, num_chars, option & 0x3F);

  //This message has a CRC, but it appears that it is carried out across
  //multiple segments if this is a multi-part message, but CRC type is not
  //indicated, so will just rely on the overall CRC from each message prior

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
  nxdn_location_id_handler (state, location_id, 0);

  //run the srv info
  if (opts->payload == 1)
    nxdn_srv_info_handler (state, svc_info);

  //run the rst info, if not zero
  if (rst_info) nxdn_rst_info_handler (state, rst_info);

  fprintf (stderr, "%s", KNRM);

  //poll for current frequency, will always be the control channel
  //this PDU is constantly pumped out on the CC CAC Message
  if (opts->p25_trunk == 1 && opts->p25_is_tuned == 0) //changed this so the rtl tuning lag doesn't set RTCH frequency after tuning but before landing
  {
    long int ccfreq = 0;
    //if using rigctl, we can poll for the current frequency
    if (opts->use_rigctl == 1)
    {
      ccfreq = GetCurrentFreq (opts->rigctl_sockfd);
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
    //if using rtl input, we can ask for the current frequency tuned
    else if (opts->audio_in_type == 3) //after changing to rtl_dev_tune, this may lag a bit due to delay in sample delivery?
    {
      ccfreq = (long int)opts->rtlsdr_center_freq;
      if (ccfreq != 0) state->p25_cc_freq = ccfreq;
    }
  }

  //clear stale active channel listing -- consider best placement for this (NXDN Type C Trunking -- inside SRV_INFO)
	if ( (time(NULL) - state->last_active_time) > 3 )
	{
		memset (state->active_channel, 0, sizeof(state->active_channel));
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
  uint8_t adj_alloc = 0; //number of adjacent sites

  uint16_t channel1 = 0;
  uint16_t channel2 = 0;
  long int freq1 = 0;
  long int freq2 = 0;
  UNUSED2(freq1, freq2);

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
  nxdn_location_id_handler(state, location_id, 0);

  //run the srv info
  if (opts->payload == 1)
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

void NXDN_decode_adj_site(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  //the size of this PDU can vary, but the adj_site_location_id and/or channel will be NULL or 0 if not enough space to fill it
  //will want to monitor this PDU for potential overflow related issues with the Message or ElementContent size

  //up to four adj_site_location_ids can be conveyed -- see 6.4.3.4 for more information
  uint32_t adj1_site = 0;
  uint32_t adj2_site = 0;
  uint32_t adj3_site = 0;
  //options -- 6.5.38. Adjacent Site Option -- 4 LSB are Site Number, 2 MSB are spares
  uint8_t  adj1_opt = 0;
  uint8_t  adj2_opt = 0;
  uint8_t  adj3_opt = 0;
 //channel or OFN
  uint16_t adj1_chan = 0;
  uint16_t adj2_chan = 0;
  uint16_t adj3_chan = 0;
  //DFA only BW value
  uint8_t adj1_bw = 0;
  uint8_t adj2_bw = 0;

  fprintf (stderr, "%s", KYEL);

  //Channel Version
  if (state->nxdn_rcn == 0)
  {
    //1
    adj1_site = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
    adj1_opt = (uint8_t)ConvertBitIntoBytes(&Message[32], 6);
    adj1_chan = (uint16_t)ConvertBitIntoBytes(&Message[38], 10);
    //2
    adj2_site = (uint32_t)ConvertBitIntoBytes(&Message[48], 24);
    adj2_opt = (uint8_t)ConvertBitIntoBytes(&Message[72], 6);
    adj2_chan = (uint16_t)ConvertBitIntoBytes(&Message[78], 10);
    //3
    adj3_site = (uint32_t)ConvertBitIntoBytes(&Message[88], 24);
    adj3_opt = (uint8_t)ConvertBitIntoBytes(&Message[112], 6);
    adj3_chan = (uint16_t)ConvertBitIntoBytes(&Message[118], 10);
    //4 -- facch2 only
    // adj4_site = (uint32_t)ConvertBitIntoBytes(&Message[128], 24);
    // adj4_opt = (uint8_t)ConvertBitIntoBytes(&Message[152], 6);
    // adj4_chan = (uint16_t)ConvertBitIntoBytes(&Message[158], 10);

    if (adj1_opt & 0xF)
    {
      fprintf (stderr, "\n Adjacent Site %d ", adj1_opt & 0xF);
      fprintf (stderr, "Channel [%03X] [%04d]", adj1_chan, adj1_chan);
      nxdn_location_id_handler(state, adj1_site, 1);
      nxdn_channel_to_frequency (opts, state, adj1_chan);
    }
    if (adj2_opt & 0xF)
    {
      fprintf (stderr, "\n Adjacent Site %d ", adj2_opt & 0xF);
      fprintf (stderr, "Channel [%03X] [%04d]", adj2_chan, adj2_chan);
      nxdn_location_id_handler(state, adj2_site, 1);
      nxdn_channel_to_frequency (opts, state, adj2_chan);
    }
    if (adj3_opt & 0xF)
    {
      fprintf (stderr, "\n Adjacent Site %d ", adj3_opt & 0xF);
      fprintf (stderr, "Channel [%03X] [%04d]", adj3_chan, adj3_chan);
      nxdn_location_id_handler(state, adj3_site, 1);
      nxdn_channel_to_frequency (opts, state, adj3_chan);
    }
    // if (adj4_opt & 0xF) //facch2 only
    // {
    //   fprintf (stderr, "\n Adjacent Site %d: ", adj4_opt & 0xF);
    //   fprintf (stderr, "Channel [%03X] [%04d]", adj4_chan, adj4_chan);
    //   nxdn_location_id_handler(state, adj4_site, 1);
    //   nxdn_channel_to_frequency (opts, state, adj4_chan);
    // }

  }

  //DFA Version
  if (state->nxdn_rcn == 1)
  {
    //1
    adj1_site = (uint32_t)ConvertBitIntoBytes(&Message[8], 24);
    adj1_opt = (uint8_t)ConvertBitIntoBytes(&Message[32], 6);
    adj1_bw = (uint8_t)ConvertBitIntoBytes(&Message[38], 2);
    adj1_chan = (uint16_t)ConvertBitIntoBytes(&Message[40], 16);
    //2
    adj2_site = (uint32_t)ConvertBitIntoBytes(&Message[56], 24);
    adj2_opt = (uint8_t)ConvertBitIntoBytes(&Message[80], 6);
    adj2_bw = (uint8_t)ConvertBitIntoBytes(&Message[86], 2);
    adj2_chan = (uint16_t)ConvertBitIntoBytes(&Message[88], 16);
    //3 -- facch2 only
    // adj3_site = (uint32_t)ConvertBitIntoBytes(&Message[104], 24);
    // adj3_opt = (uint8_t)ConvertBitIntoBytes(&Message[128], 6);
    // adj3_bw = (uint8_t)ConvertBitIntoBytes(&Message[134], 2);
    // adj3_chan = (uint16_t)ConvertBitIntoBytes(&Message[136], 16);

    if (adj1_opt & 0xF)
    {
      fprintf (stderr, "\n Adjacent Site %d ", adj1_opt & 0xF);
      fprintf (stderr, "Channel [%04X] [%05d] ", adj1_chan, adj1_chan);
      if (adj1_bw == 0) fprintf (stderr, "BW: 6.25 kHz - 4800 bps");
      else if (adj1_bw == 1) fprintf (stderr, "BW: 12.5 kHz - 9600 bps");
      else fprintf (stderr, "BW: %d Reserved Value", adj1_bw);
      nxdn_location_id_handler(state, adj1_site, 1);
      nxdn_channel_to_frequency (opts, state, adj1_chan);
    }
    if (adj2_opt & 0xF)
    {
      fprintf (stderr, "\n Adjacent Site %d ", adj2_opt & 0xF);
      fprintf (stderr, "Channel [%04X] [%05d] ", adj2_chan, adj2_chan);
      if (adj2_bw == 0) fprintf (stderr, "BW: 6.25 kHz - 4800 bps");
      else if (adj2_bw == 1) fprintf (stderr, "BW: 12.5 kHz - 9600 bps");
      else fprintf (stderr, "BW: %d Reserved Value", adj2_bw);
      nxdn_location_id_handler(state, adj2_site, 1);
      nxdn_channel_to_frequency (opts, state, adj2_chan);
    }
    // if (adj3_opt & 0xF) //facch2 only
    // {
    //   fprintf (stderr, "\n Adjacent Site %d: ", adj3_opt & 0xF);
    //   fprintf (stderr, "Channel [%04X] [%05d] ", adj3_chan, adj3_chan);
    //   if (adj3_bw == 0) fprintf (stderr, "BW: 6.25 kHz - 4800 bps");
    //   else if (adj3_bw == 1) fprintf (stderr, "BW: 12.5 kHz - 9600 bps");
    //   else fprintf (stderr, "BW: %d Reserved Value", adj3_bw);
    //   nxdn_location_id_handler(state, adj3_site, 1);
    //   nxdn_channel_to_frequency (opts, state, adj3_chan);
    // }
  }

  fprintf (stderr, "%s", KNRM);

}

void NXDN_decode_VCALL(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  UNUSED(opts);

  uint8_t  CCOption = 0;
  uint8_t  CallType = 0;
  uint8_t  VoiceCallOption = 0;
  uint16_t SourceUnitID = 0;
  uint16_t DestinationID = 0;
  uint8_t  CipherType = 0;
  uint8_t  KeyID = 0;
  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};

  uint8_t MessageType;
  /* Get the "Message Type" field */
  MessageType  = (Message[2] & 1) << 5;
  MessageType |= (Message[3] & 1) << 4;
  MessageType |= (Message[4] & 1) << 3;
  MessageType |= (Message[5] & 1) << 2;
  MessageType |= (Message[6] & 1) << 1;
  MessageType |= (Message[7] & 1) << 0;

  if (MessageType == 0x1) fprintf (stderr, "%s", KGRN); //VCALL
  else if (MessageType == 0x07) fprintf (stderr, "%s", KYEL); //TX_REL_EXT
  else if (MessageType == 0x08) fprintf (stderr, "%s", KYEL); //TX_REL
  else if (MessageType == 0x11) fprintf (stderr, "%s", KRED); //DISC

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

  //On Type-D systems, need to truncate to an 11-bit value
  //other 5-bits are repeater or prefix value
  uint8_t idas = 0;
  uint8_t rep1 = 0;
  uint8_t rep2 = 0;
  UNUSED(rep2);
  if (strcmp (state->nxdn_location_category, "Type-D") == 0) idas = 1;
  if (idas)
  {
    rep1 = (SourceUnitID >> 11) & 0x1F;
    rep2 = (DestinationID >> 11) & 0x1F;
    SourceUnitID = SourceUnitID & 0x7FF;
    DestinationID = DestinationID & 0x7FF;
  }

  /* Decode the "Cipher Type" */
  CipherType = (uint8_t)ConvertBitIntoBytes(&Message[56], 2);
  state->NxdnElementsContent.CipherType = CipherType;

  /* Decode the "Key ID" */
  KeyID = (uint8_t)ConvertBitIntoBytes(&Message[58], 6);
  state->NxdnElementsContent.KeyID = KeyID;

  fprintf (stderr, "\n ");

  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  //Call String for Per Call WAV File
  sprintf (state->call_string[0], "%s", NXDN_Call_Type_To_Str(CallType));
  if (CCOption & 0x80) strcat (state->call_string[0], " Emergency");
  if (CipherType) strcat (state->call_string[0], " Enc");

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
  fprintf (stderr, "%s - ", NXDN_Call_Type_To_Str(CallType));
  sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));

  /* Print the "Voice Call Option" */
  if (MessageType == 0x1) NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  if (MessageType == 0x1) fprintf(stderr, "%s %s (%02X) - ", DuplexMode, TransmissionMode, VoiceCallOption);
  else if (MessageType == 0x07) fprintf (stderr, "Transmission Release Ex - "); //TX_REL_EX
  else if (MessageType == 0x08) fprintf (stderr, "  Transmission Release  - "); //TX_REL
  else if (MessageType == 0x11) fprintf (stderr, "       Disconnect       - "); //DISC

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);

  //Channel here appears to be the prefix or home channel of the caller, not necessarily the channel the call is occuring on
  if (idas) fprintf (stderr, "- Prefix Ch: %d ", rep1);

  fprintf (stderr, "%s", KNRM);

  //if using the keyloader, then check for a key value first by the key id,
  //and then if not available, check by the destination (TG) id value
  //also, for DES and AES, set the nxdn_key varialbe to the DestID for IV and KS gen
  if (state->keyloader == 1)
  {
    //if Scrambler Key (and not running NXDN96 since that has VCALL in the non-voice frames)
    //NOTE: The scrambler seed carries on the state->R variable so that will reset incorrectly on NXDN96
    //NOTE: Observed on system with scrambler and AES keys on same TG, disabling loading DES and AES key by DestID
    if (CipherType == 1 && opts->frame_nxdn48 == 1 && opts->frame_nxdn96 == 0)
    {
      if (state->rkey_array[KeyID] != 0) state->R = state->rkey_array[KeyID];
      else if (state->rkey_array[DestinationID] != 0) state->R = state->rkey_array[DestinationID];
    }

    //if DES Key
    else if (CipherType == 2)
    {
      if (state->rkey_array[KeyID] != 0) state->R = state->rkey_array[KeyID];
      // else if (state->rkey_array[DestinationID] != 0)
      // {
      //   state->R = state->rkey_array[DestinationID];
      //   state->nxdn_key = DestinationID;
      // }
    }

    //if AES Key
    else if (CipherType == 3)
    {
      uint32_t kidx = 0;
      if (state->rkey_array[KeyID] != 0) kidx = KeyID;
      // else if (state->rkey_array[DestinationID] != 0) 
      // {
      //   kidx = DestinationID;
      //   state->nxdn_key = DestinationID;
      // }

      state->A1[0] = state->rkey_array[kidx+0x000];
      state->A2[0] = state->rkey_array[kidx+0x101];
      state->A3[0] = state->rkey_array[kidx+0x201];
      state->A4[0] = state->rkey_array[kidx+0x301];

      //check to see if there is a value loaded or not
      if (state->A1[0] == 0 && state->A2[0] == 0 && state->A3[0] == 0 && state->A4[0] == 0)
        state->aes_key_loaded[0] = 0;
      else state->aes_key_loaded[0] = 1;

      for (int i = 0; i < 8; i++)
      {
        state->aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
      }

      state->R = state->A1[0]; //display KS stub
    }

  } //end state->keyloader == 1

  /* Print the "Cipher Type" */
  if(CipherType != 0 && MessageType == 0x1)
  {
    fprintf (stderr, "\n  %s", KYEL);
    fprintf(stderr, "%s - ", NXDN_Cipher_Type_To_Str(CipherType));
  }

  /* Print the Key ID */
  if(CipherType != 0 && MessageType == 0x1)
  {
    fprintf(stderr, "Key ID %u - ", KeyID & 0xFF);
    fprintf (stderr, "%s", KNRM);
  }


  if (CipherType == 0x01 && state->R > 0) //scrambler key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %05lld", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  if (CipherType == 0x02 && state->R > 0) //DES key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %016llX", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  if (CipherType == 0x03 && state->R > 0) //AES key stub
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "KS: %016llX", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  //only grab if VCALL
  if(MessageType == 0x1)
  {
    //only assign rid if not spare and not reserved (happens on private calls, unsure of its significance)
    if ( (VoiceCallOption & 0xF) < 4) //ideally, only want 0, 1, 2, 3, or 4 (or 6, 7 on telephone calls)
      state->nxdn_last_rid = SourceUnitID;
    state->nxdn_last_tg = DestinationID;
    state->nxdn_key = KeyID;
    if (CallType == 0 || CallType == 1) //broadcast and group
      state->gi[0] = 0;
    else if (CallType == 4) //private
      state->gi[0] = 1;
    else state->gi[0] = -1; //unassigned on any other values
    state->nxdn_cipher_type = CipherType;
  }
  else
  {
    state->nxdn_last_rid = 0;
    state->nxdn_last_tg = 0;
    state->gi[0] = -1;
    memset (state->generic_talker_alias[0], 0, sizeof(state->generic_talker_alias[0]));
    sprintf (state->generic_talker_alias[0], "%s", "");
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

  //TG ENC LO/B if ENC trunked following disabled #121 -- was locking out everything
  if (opts->p25_trunk == 1 && opts->trunk_tune_enc_calls == 0 && MessageType == 0x1 && state->dmr_encL == 1)
  {
    int i, lo = 0;
    uint16_t t = 0; char gm[8]; char gn[50];

    //check to see if this group already exists, or has already been locked out, or is allowed
    for (i = 0; i <= state->group_tally; i++)
    {
      t = (uint16_t)state->group_array[i].groupNumber;
      if (DestinationID == t && t != 0)
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
    {
      state->group_array[state->group_tally].groupNumber = DestinationID;
      sprintf (state->group_array[state->group_tally].groupMode, "%s", "DE");
      sprintf (state->group_array[state->group_tally].groupName, "%s", "ENC LO");
      sprintf (gm, "%s", "DE");
      sprintf (gn, "%s", "ENC LO");
      state->group_tally++;
    }

    //run a watchdog here so we can update this with the crypto variables and ENC LO
    if (DestinationID != 0 && lo == 0)
    {
      sprintf (state->event_history_s[0].Event_History_Items[0].internal_str, "Target: %d; has been locked out; Encryption Lock Out Enabled.", DestinationID);
      watchdog_event_current(opts, state, 0);
    }

    //Craft a fake DISC Message send it to return to CC
    uint8_t dbits[96]; memset (dbits, 0, sizeof(dbits)); dbits[3] = 1; dbits[7] = 1; //DISC = 0x11;
    if ( (strcmp(gm, "DE") == 0) && (strcmp(gn, "ENC LO") == 0)  )
      NXDN_Elements_Content_decode (opts, state, 1, dbits);

  }

} /* End NXDN_decode_VCALL() */

void NXDN_decode_VCALL_IV(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  UNUSED(opts);

  state->payload_miN = 0; //zero out
  unsigned long long int IV = 0;

  //NOTE: On Type-D systems, the FACCH1 version of message only carries a 4-octet payload and also only carries a 22-bit IV
  //which makes no sense since you still have the full 80-bits of FACCH1 to use
  uint8_t idas = 0;
  if (strcmp (state->nxdn_location_category, "Type-D") == 0) idas = 1;
  if (!idas) IV = (unsigned long long int)ConvertBitIntoBytes(&Message[8], 64);
  else IV = (unsigned long long int)ConvertBitIntoBytes(&Message[8], 22);
  //At this point, I would assume an LFSR function is needed to expand the 22-bit IV collected here into a 64-bit IV, or 128-bit IV

  state->payload_miN = IV;
  fprintf (stderr, "\n  VCALL_IV: %016llX", state->payload_miN);


  if ( ((state->nxdn_cipher_type == 0x02) || (state->nxdn_cipher_type == 0x03)) )
  {

    if (state->nxdn_cipher_type == 0x03 && state->keyloader == 1)
    {
      state->A1[0] = state->rkey_array[state->nxdn_key+0x000];
      state->A2[0] = state->rkey_array[state->nxdn_key+0x101];
      state->A3[0] = state->rkey_array[state->nxdn_key+0x201];
      state->A4[0] = state->rkey_array[state->nxdn_key+0x301];

      //check to see if there is a value loaded or not
      if (state->A1[0] == 0 && state->A2[0] == 0 && state->A3[0] == 0 && state->A4[0] == 0)
        state->aes_key_loaded[0] = 0;
      else state->aes_key_loaded[0] = 1;

      for (int i = 0; i < 8; i++)
      {
        state->aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
      }

      state->R = state->A1[0]; //display KS stub

    }

    //expand the IV into 128-bit
    if (state->nxdn_cipher_type == 0x03)
      LFSR128n(state);

    if (state->nxdn_cipher_type == 0x02 && state->keyloader == 1)
    {
      if (state->rkey_array[state->nxdn_key] != 0)
        state->R = state->rkey_array[state->nxdn_key];
    }

    //signal a new IV is available, but only when the other checks pass
    if (state->nxdn_cipher_type == 0x02 && state->R != 0)
      state->nxdn_new_iv = 1;

    if (state->nxdn_cipher_type == 0x03 && state->aes_key_loaded[0] == 1)
      state->nxdn_new_iv = 1;

  }

} /* End NXDN_decode_VCALL_IV() */

//SCCH messages have a unique format that can be used in a super frame, but each 'unit'
//can also (mostly) be decoded seperately, except an enc IV
void NXDN_decode_scch(dsd_opts * opts, dsd_state * state, uint8_t * Message, uint8_t direction)
{
  uint8_t sf = (uint8_t)ConvertBitIntoBytes(&Message[0], 2);
  uint8_t opcode = (direction << 2 | sf);

	fprintf (stderr, "\n " ); //initial line break

  if (opts->payload == 1)
  {
    if (direction == 0) fprintf (stderr, "ISM ");
    else fprintf (stderr, "OSM ");

    if (sf == 0) fprintf (stderr, "INFO4 ");
    else if (sf == 1) fprintf (stderr, "INFO3 ");
    else if (sf == 2) fprintf (stderr, "INFO2 ");
    else fprintf (stderr, "INFO1 ");
    fprintf (stderr, "- ");
    fprintf (stderr, "%02X ",opcode);
  }

	//elements used will be determined by other elements in the ISM/OSM messages

	//OSM 4 elements
	uint8_t area  = Message[2];
	uint8_t rep1  = (uint8_t)ConvertBitIntoBytes(&Message[3], 5); //repeater 1 value
	uint8_t rep2  = (uint8_t)ConvertBitIntoBytes(&Message[8], 5); //repeater 2 value
	uint16_t id   = (uint16_t)ConvertBitIntoBytes(&Message[13], 11); //id is context dependent
	uint8_t sitet = (uint8_t)ConvertBitIntoBytes(&Message[3], 5); //site type on id=2041 only
	uint8_t gu    = Message[24]; //group or unit bit

  //OSM 2 and 3 elements -- unique only
  unsigned long long int iv_a   = (uint64_t)ConvertBitIntoBytes(&Message[13], 12); //initialization vector (0-11)

  //OSM 1 elements -- unique only -- DOUBLE CHECK ALL OF THESE!
  unsigned long long int iv_b    = (uint64_t)ConvertBitIntoBytes(&Message[18], 6); //initialization vector (b12-17)
  unsigned long long int iv_c    = (uint64_t)ConvertBitIntoBytes(&Message[8], 5); //initialization vector (b18-22)
  uint8_t iv_type  = Message[24]; //0 or 1 tells us whether or not this is for IV, or for cipher/key
  uint8_t call_opt = (uint8_t)ConvertBitIntoBytes(&Message[13], 3); //Call Options
  uint8_t key_id   = (uint8_t)ConvertBitIntoBytes(&Message[18], 6); //Key ID
  uint8_t cipher   = (uint8_t)ConvertBitIntoBytes(&Message[16], 2); //Cipher Type
  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};

	//ISM messages are seemingly exactly the same info as OSM, so just going to use OSM versions
  //(manual doesn't specify other than that ISM 4 is the same as ISM2, but probably doesn't have IDLE/HALT/SITE ID messages)

  //set category to "Type-D" to alert users this is an a Distributed Trunking System
  sprintf (state->nxdn_location_category, "Type-D");

  //placeholder numbers, using 99 if these aren't set by the Site ID Message (single sites)
  // if (state->nxdn_location_site_code == 0) state->nxdn_location_site_code = 99;
  // if (state->nxdn_location_sys_code == 0)  state->nxdn_location_sys_code = 99;
  // if (state->nxdn_last_ran == -1)          state->nxdn_last_ran = 99;

  state->nxdn_last_ran = area;

  state->last_cc_sync_time = time(NULL);

	//OSM messages
	if (opcode == 0x4 || opcode == 0x0) //INFO 4
	{
    //clear stale active channel listing -- consider best placement for this (NXDN Type D Trunking -- inside a particular OSM Message?)
    //may not be entirely necessary here in this context
    if ( (time(NULL) - state->last_active_time) > 3 )
    {
      memset (state->active_channel, 0, sizeof(state->active_channel));
    }

		if (id == 2046)
		{
			fprintf (stderr, "Idle Repeater Message - ");
			fprintf (stderr, "Area: %d; ", area);
			fprintf (stderr, "Repeater 1: %d; ", rep1);
			fprintf (stderr, "Repeater 2: %d; ", rep2);
      sprintf (state->active_channel[rep1], "%s", "");
      sprintf (state->active_channel[rep2], "%s", "");

		}
		else if (id == 2045)
		{
			fprintf (stderr, "Halt Repeater Message - ");
			fprintf (stderr, "Area: %d; ", area);
			fprintf (stderr, "Repeater 1: %d; ", rep1);
			fprintf (stderr, "Repeater 2: %d; ", rep2);

		}
		else if (id == 2044)
		{
			fprintf (stderr, "Free Repeater Message - ");
			fprintf (stderr, "Area: %d; ", area);
			fprintf (stderr, "Free Repeater 1: %d; ", rep1);
			fprintf (stderr, "Free Repeater 2: %d; ", rep2);

		}
		else if (id == 2041)
		{
			fprintf (stderr, "Site ID Message - ");
			fprintf (stderr, "Area: %d; ", area);
			fprintf (stderr, "Site Type: %d ", sitet); //Roaming Algorithm by SU; See below;
			if (sitet == 0) fprintf (stderr, "Reserved; ");
			else if (sitet == 1) fprintf (stderr, "Wide; ");
			else if (sitet == 2) fprintf (stderr, "Middle; ");
			else fprintf (stderr, "Narrow; ");
			fprintf (stderr, "Site Code: %d ", rep2);
			if (rep2 == 0) fprintf (stderr, "Reserved; ");
			else if (rep2 < 251) fprintf (stderr, "Open Access; "); //Usable voluntary every TRS...?
			else fprintf (stderr, "Reserved; ");
			state->nxdn_location_site_code = sitet;
			state->nxdn_location_sys_code = sitet;
      state->nxdn_last_ran = sitet;

		}
		else //Busy Repeater Message is essentially our 'call in progess' message for a repeater
		{
			if (gu && rep1 == 0) fprintf (stderr, "REG_COMM; ");
			else fprintf (stderr, "Busy Repeater Message - ");
			fprintf (stderr, "Area: %d; ", area);
			fprintf (stderr, "Go to Repeater: %d; ", rep1);
			fprintf (stderr, "Home Repeater: %d; ", rep2);

			if (rep1 == 31)
			{
				fprintf (stderr, "\n%s ", KRED);
				state->nxdn_last_tg = 0;
				state->nxdn_last_rid = 0;
			}
			else
			{
				fprintf (stderr, "\n%s ", KGRN);
        //only set this is during a voice tx, and not a data tx
        if ( time(NULL) - state->last_vc_sync_time < 1 )
          state->nxdn_last_tg = id;
			}

			fprintf (stderr, " Channel Update - CH: %d - TGT: %d ", rep1, id);
			if (gu == 0) fprintf (stderr, "Group Call ");
			else fprintf (stderr, "Private Call ");

			if (rep1 == 31) fprintf (stderr, "Termination ");

      //add current active to display string -- may need tweaking on if to use rep1 (active ch), or rep2 (home prefix)
      if (rep1 != 0 && rep1 != 31)
      {
        if (gu == 0) sprintf (state->active_channel[rep1], "Active Group Ch: %d TG: %d-%d; ", rep1, rep2, id); //Group TG
        else sprintf (state->active_channel[rep1], "Active Private Ch: %d TGT: %d-%d; ", rep1, rep2, id); //Private TGT
        state->last_active_time = time(NULL);
      }

      //start tuning section here
      uint8_t tune = 0; //use this to check to see if okay to tune

      //only tune group calls if user set
      if (gu == 0 && opts->trunk_tune_group_calls == 1) tune = 1;

      //only tune private calls if user set
      if (gu == 1 && opts->trunk_tune_private_calls == 1) tune = 1;

      //tune back to 'channel 31' on termination, will be user provided 'default' listening channel
      if (rep1 == 31) tune = 1;

      //begin tuning
      if (tune == 1 && rep1 != 0)
      {
        //check for control channel frequency in the channel map if not available
        if (state->trunk_chan_map[31] != 0) state->p25_cc_freq = state->trunk_chan_map[31]; //user provided channel to go to for '31' -- may change to 0 later?
        else if (state->trunk_chan_map[rep2] != 0) state->p25_cc_freq = state->trunk_chan_map[rep2]; //rep2 is home repeater under this message

        //run group/tgt analysis and tune if available/desired
        //group list mode so we can look and see if we need to block tuning any groups, etc
        char mode[8]; //allow, block, digital, enc, etc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == id) //tg/tgt only on info4 unit
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on IDAS -- block non-matching target, allow matching DestinationID
        if (state->tg_hold != 0 && state->tg_hold != id) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == id)
        {
          sprintf (mode, "%s", "A");
          opts->p25_is_tuned = 0; //unlock tuner at this stage and not above check
        }

        long int freq = 0;
        freq = nxdn_channel_to_frequency(opts, state, rep1);

        //check to see if the source/target candidate is blocked first
        if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
        {
          //will need to monitor, 1 second may be too long on idas, may need to try 0 or manipulate another way
          if (state->p25_cc_freq != 0 && ((time(NULL) - state->last_vc_sync_time) > 1) && freq != 0)
          {
            //rigctl
            if (opts->use_rigctl == 1)
            {
              //extra safeguards due to sync issues with NXDN
              memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
              memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
              state->lastsynctype = -1;
              state->last_cc_sync_time = time(NULL);
              state->last_vc_sync_time = time(NULL); //should we use this here, or not?
              //
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
              opts->p25_is_tuned = 1;
              //check the rkey array for a scrambler key value
              //TGT ID and Key ID could clash though if csv or system has both with different keys
              if (state->rkey_array[id] != 0) state->R = state->rkey_array[id];
              if (state->forced_alg_id == 1) state->nxdn_cipher_type = 0x1;
            }
            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              //extra safeguards due to sync issues with NXDN
              memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
              memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
              state->lastsynctype = -1;
              state->last_cc_sync_time = time(NULL);
              state->last_vc_sync_time = time(NULL); //should we use this here, or not?
              //
              rtl_dev_tune (opts, freq);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
              opts->p25_is_tuned = 1;
              //check the rkey array for a scrambler key value
              //TGT ID and Key ID could clash though if csv or system has both with different keys
              if (state->rkey_array[id] != 0) state->R = state->rkey_array[id];
              if (state->forced_alg_id == 1) state->nxdn_cipher_type = 0x1;
              #endif
            }

          }
        }
      } //end tuning -- NOTE: since we are only going by last time voice sync detected, tuning here is always 'unlocked'

		} //end 'busy' repeater

	}

	if (opcode == 0x5 || opcode == 0x1) //INFO 3
	{
    fprintf (stderr, "Source Message - ");
    fprintf (stderr, "Area: %d; ", area);
    fprintf (stderr, "Free Repeater 1: %d; ", rep1);

    if (id == 31)
    {
      fprintf (stderr, "\n%s ", KYEL);
      fprintf (stderr, " Call IV A: %04llX", iv_a);
    }
    else
    {
      //rep2 is the home repeater/source prefix in this instance
      fprintf (stderr, "\n%s ", KGRN);
      fprintf (stderr, " Source Update - Prefix CH: %d SRC: %d - (%d-%d) ", rep2, id, rep2, id); //rep2
      //only set this is during a voice tx, and not a data tx
      if ( time(NULL) - state->last_vc_sync_time < 1 )
        state->nxdn_last_rid = id;
    }

	}

  if (opcode == 0x6 || opcode == 0x2) //OSM INFO 2 -- same as 4
	{
    fprintf (stderr, "Target Message - ");
    fprintf (stderr, "Area: %d; ", area);
    fprintf (stderr, "Go to Repeater: %d; ", rep1);
    if (id == 31)
    {
      fprintf (stderr, "\n%s ", KYEL);
      fprintf (stderr, " Call IV A: %04llX; ", iv_a); //MSB 0-11
      //zero out IV storage and append this chunk to MSB position
      state->payload_miN = 0;
      state->payload_miN = state->payload_miN | (iv_a << 11); //MSB 11, so shift 11 (22 total bits).
    }
    else
    {
      //rep2 is the home repeater/source prefix in this instance
      fprintf (stderr, "\n%s ", KGRN);
      fprintf (stderr, " Target Update - Prefix CH: %d SRC: %d - (%d-%d) ", rep2, id, rep2, id); //rep2
      //only set this is during a voice tx, and not a data tx
      if ( time(NULL) - state->last_vc_sync_time < 1 )
        state->nxdn_last_tg = id;
    }

	}

  if (opcode == 0x7 || opcode == 0x3) //INFO 1 - Call Option/Encryption Parms
  {
    fprintf (stderr, "Call Option - ");
    fprintf (stderr, "Area: %d; ", area);
    fprintf (stderr, "Free Repeater 1: %d; ", rep1);
    if (iv_type == 0)
    {
      fprintf (stderr, "Free Repeater 2: %d; ", rep2); //or Pass Character (31) on ISM
      fprintf (stderr, "\n%s ", KYEL);
      NXDN_Voice_Call_Option_To_Str(call_opt, DuplexMode, TransmissionMode);
      fprintf(stderr, " %s %s ", DuplexMode, TransmissionMode);
      if (cipher)
      {
        fprintf(stderr, "- %s - ", NXDN_Cipher_Type_To_Str(cipher));
        fprintf (stderr, "Key ID: %d; ", key_id);
        state->nxdn_cipher_type = cipher;
        state->nxdn_key = key_id;
      }

    }
    else
    {
      fprintf (stderr, "\n%s ", KYEL);
      fprintf (stderr, "Call IV B: %04llX; ", iv_b);
      fprintf (stderr, "Call IV C: %04llX; ", iv_c);
      //Append to Call IV storage
      state->payload_miN = state->payload_miN | (iv_c << 6); //middle 5 bits..shifts 6 to accomodate last 6 bits below
      state->payload_miN = state->payload_miN | (iv_b << 0); //last 6 bits...no shift needed
      //At this point, I would assume an LFSR function is needed to expand the 22-bit IV collected here into a 64-bit IV, or 128-bit IV
      fprintf (stderr, "Completed IV: %016llX", state->payload_miN);
    }
  }

}

//ARIB STD-B54 Messages

//ARIB STD-B54 Page 22 - 選択呼出音声通信 and 選択呼出終話 (Selective Call Info and Call End)
void NXDN_decode_VCALL_ARIB(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  //MFID is the only difference, this is added, the rest are shifted 8 bits down
  uint8_t  mfid = 0;
  uint8_t  CCOption = 0;
  uint8_t  CallType = 0;
  uint8_t  VoiceCallOption = 0;
  uint16_t SourceUnitID = 0;
  uint16_t DestinationID = 0;
  uint8_t  CipherType = 0;
  uint8_t  KeyID = 0;
  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};

  uint8_t MessageType;
  /* Get the "Message Type" field */
  MessageType  = (Message[2] & 1) << 5;
  MessageType |= (Message[3] & 1) << 4;
  MessageType |= (Message[4] & 1) << 3;
  MessageType |= (Message[5] & 1) << 2;
  MessageType |= (Message[6] & 1) << 1;
  MessageType |= (Message[7] & 1) << 0;

  if (MessageType == 0x21) fprintf (stderr, "%s", KGRN); //VCALL
  else if (MessageType == 0x28) fprintf (stderr, "%s", KYEL); //TX_REL

  mfid = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);

  /* Decode "CC Option" */
  CCOption = (uint8_t)ConvertBitIntoBytes(&Message[16], 8);
  state->NxdnElementsContent.CCOption = CCOption;

  /* Decode "Call Type" */
  CallType = (uint8_t)ConvertBitIntoBytes(&Message[24], 3);
  state->NxdnElementsContent.CallType = CallType;

  /* Decode "Voice Call Option" */
  VoiceCallOption = (uint8_t)ConvertBitIntoBytes(&Message[27], 5);
  state->NxdnElementsContent.VoiceCallOption = VoiceCallOption;

  /* Decode "Source Unit ID" */
  SourceUnitID = (uint16_t)ConvertBitIntoBytes(&Message[32], 16);
  state->NxdnElementsContent.SourceUnitID = SourceUnitID;

  /* Decode "Destination ID" */
  DestinationID = (uint16_t)ConvertBitIntoBytes(&Message[48], 16);
  state->NxdnElementsContent.DestinationID = DestinationID;

  /* Decode the "Cipher Type" */
  CipherType = (uint8_t)ConvertBitIntoBytes(&Message[64], 2);
  state->NxdnElementsContent.CipherType = CipherType;

  /* Decode the "Key ID" */
  KeyID = (uint8_t)ConvertBitIntoBytes(&Message[66], 6);
  state->NxdnElementsContent.KeyID = KeyID;

  fprintf (stderr, "\n MFID: %02X; ", mfid);

  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  //Call String for Per Call WAV File
  sprintf (state->call_string[0], "%s", NXDN_Call_Type_To_Str(CallType));
  if (CCOption & 0x80) strcat (state->call_string[0], " Emergency");
  if (CipherType) strcat (state->call_string[0], " Enc");

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
  fprintf (stderr, "%s - ", NXDN_Call_Type_To_Str(CallType));
  sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType));

  /* Print the "Voice Call Option" */
  if (MessageType == 0x21) NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  if (MessageType == 0x21) fprintf(stderr, "%s %s (%02X) - ", DuplexMode, TransmissionMode, VoiceCallOption);
  else if (MessageType == 0x28) fprintf (stderr, "  Transmission Release  - "); //TX_REL

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);

  fprintf (stderr, "%s", KNRM);

  //if using the keyloader, then check for a key value first by the key id,
  //and then if not available, check by the destination (TG) id value
  //also, for DES and AES, set the nxdn_key varialbe to the DestID for IV and KS gen
  if (state->keyloader == 1)
  {
    //if Scrambler Key (and not running NXDN96 since that has VCALL in the non-voice frames)
    //NOTE: The scrambler seed carries on the state->R variable so that will reset incorrectly on NXDN96
    //NOTE: Observed on system with scrambler and AES keys on same TG, disabling loading DES and AES key by DestID
    if (CipherType == 1 && opts->frame_nxdn48 == 1 && opts->frame_nxdn96 == 0)
    {
      if (state->rkey_array[KeyID] != 0) state->R = state->rkey_array[KeyID];
      else if (state->rkey_array[DestinationID] != 0) state->R = state->rkey_array[DestinationID];
    }

    //if DES Key
    else if (CipherType == 2)
    {
      if (state->rkey_array[KeyID] != 0) state->R = state->rkey_array[KeyID];
      // else if (state->rkey_array[DestinationID] != 0)
      // {
      //   state->R = state->rkey_array[DestinationID];
      //   state->nxdn_key = DestinationID;
      // }
    }

    //if AES Key
    else if (CipherType == 3)
    {
      uint32_t kidx = 0;
      if (state->rkey_array[KeyID] != 0) kidx = KeyID;
      // else if (state->rkey_array[DestinationID] != 0) 
      // {
      //   kidx = DestinationID;
      //   state->nxdn_key = DestinationID;
      // }

      state->A1[0] = state->rkey_array[kidx+0x000];
      state->A2[0] = state->rkey_array[kidx+0x101];
      state->A3[0] = state->rkey_array[kidx+0x201];
      state->A4[0] = state->rkey_array[kidx+0x301];

      //check to see if there is a value loaded or not
      if (state->A1[0] == 0 && state->A2[0] == 0 && state->A3[0] == 0 && state->A4[0] == 0)
        state->aes_key_loaded[0] = 0;
      else state->aes_key_loaded[0] = 1;

      for (int i = 0; i < 8; i++)
      {
        state->aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
        state->aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
      }

      state->R = state->A1[0]; //display KS stub
    }

  } //end state->keyloader == 1

  /* Print the "Cipher Type" */
  if(CipherType != 0 && MessageType == 0x21)
  {
    fprintf (stderr, "\n  %s", KYEL);
    fprintf(stderr, "%s - ", NXDN_Cipher_Type_To_Str(CipherType));
  }

  /* Print the Key ID */
  if(CipherType != 0 && MessageType == 0x21)
  {
    fprintf(stderr, "Key ID %u - ", KeyID & 0xFF);
    fprintf (stderr, "%s", KNRM);
  }

  if (CipherType == 0x01 && state->R > 0) //scrambler key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %05lld", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  if (CipherType == 0x02 && state->R > 0) //DES key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %016llX", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  if (CipherType == 0x03 && state->R > 0) //AES key stub
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "KS: %016llX", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  //only grab if VCALL
  if(MessageType == 0x21)
  {
    //only assign rid if not spare and not reserved (happens on private calls, unsure of its significance)
    if ( (VoiceCallOption & 0xF) < 4) //ideally, only want 0, 1, 2, 3, or 4 (or 6, 7 on telephone calls)
      state->nxdn_last_rid = SourceUnitID;
    state->nxdn_last_tg = DestinationID;
    state->nxdn_key = KeyID;
    if (CallType == 0 || CallType == 1) //broadcast and group
      state->gi[0] = 0;
    else if (CallType == 4) //private
      state->gi[0] = 1;
    else state->gi[0] = -1; //unassigned on any other values
    state->nxdn_cipher_type = CipherType;
  }
  else
  {
    state->nxdn_last_rid = 0;
    state->nxdn_last_tg = 0;
    state->gi[0] = -1;
    memset (state->generic_talker_alias[0], 0, sizeof(state->generic_talker_alias[0]));
    sprintf (state->generic_talker_alias[0], "%s", "");
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

} /* End NXDN_decode_VCALL_ARIB() */

void NXDN_decode_ALIAS_ARIB(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{

  uint8_t mfid = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);

  //ARIB STD-B54 (p171-172) describes this as always having 3 segments
  //but this is also signalled, so perhaps it can be variable
  uint8_t seg_num = (uint8_t)ConvertBitIntoBytes(&Message[16], 4);
  uint8_t seg_len = (uint8_t)ConvertBitIntoBytes(&Message[20], 4);

  uint8_t seg_bytes[12]; memset(seg_bytes, 0, sizeof(seg_bytes));

  int seg_byte_num = 6;
  for (int i = 0; i < seg_byte_num; i++)
    seg_bytes[i] = (uint8_t)ConvertBitIntoBytes(&Message[(i*8)+24], 8);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n Multi Segment Alias %d/%d; ", seg_num, seg_len);
    for (int i = 0; i < seg_byte_num; i++)
      fprintf (stderr, "%02X", seg_bytes[i]);
  }
  else fprintf (stderr, " %d/%d; ", seg_num, seg_len);

  //copy to PDU superframe
  memcpy(state->dmr_pdu_sf[0]+(seg_num-1)*seg_byte_num, seg_bytes, seg_byte_num*sizeof(uint8_t));

  int len = seg_byte_num * seg_len;

  //TODO: Do a CRC32 Check, currently relying on each SACCH frame CRC instead

  //extract CRC32
  uint32_t crc_ext = (state->dmr_pdu_sf[0][len-4] << 24) | (state->dmr_pdu_sf[0][len-3] << 16) | 
                     (state->dmr_pdu_sf[0][len-2] <<  8) |  state->dmr_pdu_sf[0][len-1];

  //remove CRC32 bytes
  if (len > 4)
    len -= 4;

  if (seg_num == seg_len)
  {
    if (opts->payload == 1)
    {
      fprintf (stderr, "\n Completed Alias Encoded: ");
      for (int i = 0; i < (seg_len*seg_byte_num); i++)
        fprintf (stderr, "%02X", state->dmr_pdu_sf[0][i]);
    }

    //Alias String
    char alias[500]; memset(alias, 0, sizeof(alias));
    int ptr = 0;        //pointer to position in alias buffer
    int k = 0;          //pointer to position in completed message
    uint16_t sjis = 0;  //SHIFT-JIS character
    uint8_t  utf8 = 0;  //UTF-8 Character
    uint32_t c16 = 0;   //Unicode Character returned from sjis_char_to_unicode

    //SHIFT-JIS to UNICODE Conversion
    setlocale(LC_ALL, ""); //needed when encoded alias contains Japanese (or probably any non-roman charset that isn't default on users terminal)

    if (opts->payload == 1)
      fprintf (stderr, "\n MFID: %02X; Encoded Len: %d; CRC: %04X; Alias: ", mfid, len, crc_ext);
    
    for (int i = 0; i < len; i++)
    {
      
      c16  = 0;
      utf8 = state->dmr_pdu_sf[0][k];
      sjis = (state->dmr_pdu_sf[0][k] << 8) | state->dmr_pdu_sf[0][k+1];

      if (utf8 >= 0x20 && utf8 < 0x80)
      {
        c16 = sjis_char_to_unicode(utf8);
        fprintf (stderr, "%lc", c16);
        k++;
      }
      else if (sjis != 0)
      {
        c16 = sjis_char_to_unicode(sjis);
        fprintf (stderr, "%lc", c16);
        k+=2;
      }
      else if (utf8 == 0 && sjis == 0)
        break;

      //Encode Unicode to UTF-8 if not a pass-through ASCII control character or 0
      if (c16 >= 0x0020)
        ptr += utf8_encode(c16, alias+ptr);

    }

    //terminate string failsafe
    alias[ptr++] = 0x00;
    alias[ptr++] = 0x00;

    sprintf (state->generic_talker_alias[0], "%s", alias);
    sprintf (state->event_history_s[0].Event_History_Items[0].alias, "%s; ", alias);

    //reset PDU superframe afterwards
    memset(state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
  }

} /* End NXDN_decode_ALIAS_ARIB() */

char * NXDN_Call_Type_To_Str(uint8_t CallType)
{
  char * Ptr = NULL;

  switch(CallType)
  {
    case 0:  Ptr = "Broadcast Call";        break;
    case 1:  Ptr = "Group Call";            break;
    case 2:  Ptr = "Idle";                  break; //"Unspecified Call" This value is used only on Idle Burst TX_REL message.
    case 3:  Ptr = "Session Call";          break; //"reserved" is session call on Type D
    case 4:  Ptr = "Private Call";          break;
    case 5:  Ptr = "Reserved";              break;
    case 6:  Ptr = "PSTN Interconnect Call"; break;
    case 7:  Ptr = "PSTN Speed Dial Call";   break;
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

  switch(VoiceCallOption & 0xF)
  { //added all options, getting a random one on an NXDN96 System, need to know what it is
    case 0:  Ptr = "4800bps/EHR"; break;
    case 1:  Ptr = "Reserved 1";  break;
    case 2:  Ptr = "9600bps/EHR"; break;
    case 3:  Ptr = "9600bps/EFR"; break;
    case 4:  Ptr = "Reserved 4";  break;
    case 5:  Ptr = "Reserved 5";  break;
    case 6:  Ptr = "Reserved 6";  break;
    case 7:  Ptr = "Reserved 7";  break;
    case 8:  Ptr = "4800bps/EHR S:1"; break; //spare bit enabled
    case 9:  Ptr = "Reserved 9; S:1"; break; //spare bit enabled
    case 10: Ptr = "9600bps/EHR S:1"; break; //spare bit enabled
    case 11: Ptr = "9600bps/EFR S:1"; break; //spare bit enabled
    case 12: Ptr = "Reserved C; S1;"; break; //spare bit enabled
    case 13: Ptr = "Reserved D; S1";  break; //spare bit enabled
    case 14: Ptr = "Reserved E; S1";  break; //spare bit enabled
    case 15: Ptr = "Reserved F: S1";  break; //spare bit enabled
    default: Ptr = "Unk;";  break; //should never get here
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