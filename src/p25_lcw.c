/*-------------------------------------------------------------------------------
 * p25_lcw.c
 * P25p1 Link Control Word Decoding
 *
 * LWVMOBILE
 * 2023-05 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//new p25_lcw function here -- TIA-102.AABF-D LCW Format Messages (if anybody wants to fill the rest out)
void p25_lcw (dsd_opts * opts, dsd_state * state, uint8_t LCW_bits[], uint8_t irrecoverable_errors)
{
  UNUSED(irrecoverable_errors);

  uint8_t lc_format = (uint8_t)ConvertBitIntoBytes(&LCW_bits[0], 8);  //format
  uint8_t lc_opcode = (uint8_t)ConvertBitIntoBytes(&LCW_bits[2], 6);  //opcode portion of format
  uint8_t lc_mfid   = (uint8_t)ConvertBitIntoBytes(&LCW_bits[8], 8);  //mfid
  uint8_t lc_svcopt = (uint8_t)ConvertBitIntoBytes(&LCW_bits[16], 8); //service options
  uint8_t lc_pf = LCW_bits[0]; //protect flag
  uint8_t lc_sf = LCW_bits[1]; //Implicit / Explicit MFID Format
  UNUSED2(lc_opcode, lc_sf);

  if (lc_pf == 1) //check the protect flag -- if set, its an encrypted lcw
  {
    fprintf (stderr, "%s",KRED);
    fprintf (stderr, " LCW Protected ");
    fprintf (stderr, "%s",KNRM);
  }

  if (lc_pf == 0) //not protected/encrypted lcw
  {

    if (opts->payload == 1) fprintf (stderr, " LCW"); 

    //check to see if we need to run these as MFID 0 or 1 only (standard)
    if (lc_mfid == 0 || lc_mfid == 1) //lc_mfid == 0
    {

      //check the service options on applicable formats
      if (lc_format == 0x4A || lc_format == 0x46 || lc_format == 0x45 
          || lc_format == 0x44 || lc_format == 0x03 || lc_format == 0x00)
      {

        if (lc_svcopt & 0x80) fprintf (stderr, " Emergency");
        if (lc_svcopt & 0x40) fprintf (stderr, " Encrypted");

        if (opts->payload == 1) //hide behind payload due to len
        {
          if (lc_svcopt & 0x20) fprintf (stderr, " Duplex");
          if (lc_svcopt & 0x10) fprintf (stderr, " Packet");
          else fprintf (stderr, " Circuit");
          if (lc_svcopt & 0x8) fprintf (stderr, " R"); //reserved bit is on
          fprintf (stderr, " Priority %d", lc_svcopt & 0x7); //call priority
        }
      }

      if (lc_format == 0x00)
      {
        fprintf (stderr, " Group Voice Channel User");
        uint8_t res    = (uint8_t)ConvertBitIntoBytes(&LCW_bits[24], 7);
        uint8_t explicit = LCW_bits[24]; //explicit source id required == 1, full SUID on seperate LCW
        uint16_t group = (uint16_t)ConvertBitIntoBytes(&LCW_bits[32], 16);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24);
        fprintf (stderr, " - Group %d Source %d", group, source);
        UNUSED2(res, explicit);

        //don't set this when zero, annoying blink occurs in ncurses
        if (group != 0) state->lasttg = group;
        if (source != 0) state->lastsrc = source;

        sprintf (state->call_string[0], "   Group ");
        if (lc_svcopt & 0x80) strcat (state->call_string[0], " Emergency  ");
        else if (lc_svcopt & 0x40) strcat (state->call_string[0], " Encrypted  ");
        else strcat (state->call_string[0], "            ");
      }

      else if (lc_format == 0x03)
      {
        fprintf (stderr, " Unit to Unit Voice Channel User");
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&LCW_bits[24], 24);
        uint32_t source = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24);
        fprintf (stderr, " - Target %d Source %d", target, source);

        //don't set this when zero, annoying blink occurs in ncurses
        if (target != 0) state->lasttg = target;
        if (source != 0) state->lastsrc = source;

        sprintf (state->call_string[0], " Private ");
        if (lc_svcopt & 0x80) strcat (state->call_string[0], " Emergency  ");
        else if (lc_svcopt & 0x40) strcat (state->call_string[0], " Encrypted  ");
        else strcat (state->call_string[0], "            ");

      }

      //TODO: Allow Tuning from Call Grants either in LDU1 or TDULC? (TDMA to p1 fallback?)
      //TODO: Allow TG Hold overrides here  either in LDU1 or TDULC?
      //NOTE: If we have an active TG hold, we really should't be here anyways
      else if (lc_format == 0x42) //is this group only, or group and private?
      {
        fprintf (stderr, " Group Voice Channel Update - ");
        uint16_t channel1 = (uint16_t)ConvertBitIntoBytes(&LCW_bits[8], 16);
        uint16_t group1   = (uint16_t)ConvertBitIntoBytes(&LCW_bits[24], 16);
        uint16_t channel2 = (uint16_t)ConvertBitIntoBytes(&LCW_bits[40], 16);
        uint16_t group2   = (uint16_t)ConvertBitIntoBytes(&LCW_bits[56], 16);

        if (channel1 && group1)
        {
          fprintf (stderr, "Ch: %04X TG: %d; ", channel1, group1);
          sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; ", channel1, group1);
          state->last_active_time = time(NULL);
        }

        if (channel2 && group2 && group1 != group2)
        {
          fprintf (stderr, "Ch: %04X TG: %d; ", channel2, group2);
          sprintf (state->active_channel[1], "Active Ch: %04X TG: %d; ", channel2, group2);
          state->last_active_time = time(NULL);
        }

      }

      else if (lc_format == 0x44)
      {
        fprintf (stderr, " Group Voice Channel Update – Explicit");
        uint16_t group1   = (uint16_t)ConvertBitIntoBytes(&LCW_bits[24], 16);
        uint16_t channelt = (uint16_t)ConvertBitIntoBytes(&LCW_bits[40], 16);
        uint16_t channelr = (uint16_t)ConvertBitIntoBytes(&LCW_bits[56], 16);
        fprintf (stderr, "Ch: %04X TG: %d; ", channelt, group1);
        UNUSED(channelr);
      }

      else if (lc_format == 0x45)
      {
        fprintf (stderr, " Unit to Unit Answer Request");
      }

      else if (lc_format == 0x46)
      {
        fprintf (stderr, " Telephone Interconnect Voice Channel User");
      }

      else if (lc_format == 0x47)
      {
        fprintf (stderr, " Telephone Interconnect Answer Request");
      }

      else if (lc_format == 0x49)
      {
        fprintf (stderr, " Source ID Extension -");
        uint32_t nid = (uint32_t)ConvertBitIntoBytes(&LCW_bits[16], 24);
        uint32_t src = (uint32_t)ConvertBitIntoBytes(&LCW_bits[40], 24);
        fprintf (stderr, " Full SUID: %08X-%08d", nid, src);

      }

      else if (lc_format == 0x4A) 
      {
        fprintf (stderr, " Unit to Unit Voice Channel User – Extended");
        uint32_t target = (uint32_t)ConvertBitIntoBytes(&LCW_bits[16], 24);
        uint32_t src   = (uint32_t)ConvertBitIntoBytes(&LCW_bits[40], 24);
        fprintf (stderr, "TGT: %d; SRC: %d; ", target, src);
      }

      else if (lc_format == 0x50)
      {
        fprintf (stderr, " Group Affiliation Query");
      }

      else if (lc_format == 0x51)
      {
        fprintf (stderr, " Unit Registration Command");
      }

      else if (lc_format == 0x52) //wonder if anybody uses this if its deleted/obsolete
      {
        fprintf (stderr, " Unit Authentication Command - OBSOLETE");
      }

      else if (lc_format == 0x53) 
      {
        fprintf (stderr, " Status Query");
      }

      else if (lc_format == 0x54) 
      {
        fprintf (stderr, " Status Update");
      }

      else if (lc_format == 0x55) 
      {
        fprintf (stderr, " Status Update");
      }

      else if (lc_format == 0x56) 
      {
        fprintf (stderr, " Call Alert");
      }


      else if (lc_format == 0x5A) 
      {
        fprintf (stderr, " Status Update – Source ID Extension Required");
      }

      else if (lc_format == 0x5C) 
      {
        fprintf (stderr, " Extended Function Command – Source ID Extension Required");
      }

      else if (lc_format == 0x60) 
      {
        fprintf (stderr, " System Service Broadcast");
      }

      //this PDU does not have an associated MFID, often seen on kiwi and matches its TSBK counterpart
      //its possible some of the other ones here don't as well, need to re-check all of them
      else if (lc_format == 0x61) 
      {
        fprintf (stderr, " Secondary Control Channel Broadcast");
      }

      else if (lc_format == 0x62) 
      {
        fprintf (stderr, " Adjacent Site Status Broadcast");
      }

      else if (lc_format == 0x63) 
      {
        fprintf (stderr, " RFSS Status Broadcast");
      }

      else if (lc_format == 0x64) 
      {
        fprintf (stderr, " Network Status Broadcast");
      }

      else if (lc_format == 0x65) 
      {
        fprintf (stderr, " Protection Parameter Broadcast - OBSOLETE");
      }


      else if (lc_format == 0x66) 
      {
        fprintf (stderr, " Secondary Control Channel Broadcast – Explicit (LCSCBX)");
      }

      else if (lc_format == 0x67) //explicit
      {
        fprintf (stderr, " Adjacent Site Status (LCASBX)");
        uint8_t lra = (uint8_t)ConvertBitIntoBytes(&LCW_bits[8], 8);
        uint16_t channelt = (uint16_t)ConvertBitIntoBytes(&LCW_bits[16], 16);
        uint8_t rfssid = (uint8_t)ConvertBitIntoBytes(&LCW_bits[32], 8);
        uint8_t siteid = (uint8_t)ConvertBitIntoBytes(&LCW_bits[40], 8);
        uint16_t channelr = (uint16_t)ConvertBitIntoBytes(&LCW_bits[48], 16);
        uint8_t cfva = (uint8_t)ConvertBitIntoBytes(&LCW_bits[64], 4);
        fprintf (stderr, " - RFSS %d Site %d CH %04X", rfssid, siteid, channelt);
        UNUSED2(lra, channelr);

        //debug print only
        // fprintf (stderr, "\n  ");
        // fprintf (stderr, "  LRA [%02X] RFSS [%03d] SITE [%03d] CHAN-T [%04X] CHAN-R [%02X] CFVA [%X]\n  ", lra, rfssid, siteid, channelt, channelr, cfva);
        // if (cfva & 0x8) fprintf (stderr, " Conventional");
        // if (cfva & 0x4) fprintf (stderr, " Failure Condition");
        // if (cfva & 0x2) fprintf (stderr, " Up to Date (Correct)");
        // else fprintf (stderr, " Last Known");
        if (cfva & 0x1) fprintf (stderr, " - Connection Active");
        // process_channel_to_freq (opts, state, channelt);
        //end debug, way too much for a simple link control line 


      }

      else if (lc_format == 0x68) 
      {
        fprintf (stderr, " RFSS Status Broadcast – Explicit (LCRSBX)");
      }

      else if (lc_format == 0x69) 
      {
        fprintf (stderr, " Network Status Broadcast – Explicit (LCNSBX)");
      }

      else if (lc_format == 0x6A) 
      {
        fprintf (stderr, " Conventional Fallback");
      }

      else if (lc_format == 0x6B) 
      {
        fprintf (stderr, " Message Update – Source ID Extension Required");
      }

      //tune back to CC here - save about 1-2 seconds
      else if (lc_format == 0x4F) //# Call Termination/Cancellation 
      {
        uint32_t tgt = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24); //can be individual, or all units (0xFFFFFF)
        fprintf (stderr, " Call Termination; TGT: %d;", tgt);
        if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && opts->p25_is_tuned == 1)
        {
          
          //Will we need to check for a symbolrate change here, can a P25p2 TDMA-CC system
          //revert back to a phase 1 traffic channel or carry a phase 1 traffic channel?
          if (state->p25_cc_is_tdma == 1)
          {
            state->samplesPerSymbol = 8;
            state->symbolCenter = 3;
            opts->frame_p25p1 = 0; //turn it back off
          }

          //clear heuristics from current traffic channel
          if (opts->frame_p25p1 == 1 && opts->use_heuristics == 1)
          {
            initialize_p25_heuristics(&state->p25_heuristics);
            initialize_p25_heuristics(&state->inv_p25_heuristics);
          }

          //re-enable both slots (failsafe)
          opts->slot1_on = 1;
          opts->slot2_on = 1;

          //rigctl
          if (opts->use_rigctl == 1)
          {
            state->lasttg = 0;
            state->lastsrc = 0;
            state->payload_algid = 0; 
            state->payload_keyid = 0;
            // state->payload_miP = 0;
            //reset some strings
            sprintf (state->call_string[0], "%s", "                     "); //21 spaces
            sprintf (state->call_string[1], "%s", "                     "); //21 spaces
            sprintf (state->active_channel[0], "%s", "");
            sprintf (state->active_channel[1], "%s", "");
            opts->p25_is_tuned = 0;
            state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
            if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
            SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);        
          }
          //rtl
          else if (opts->audio_in_type == 3)
          {
            #ifdef USE_RTLSDR
            state->lasttg = 0;
            state->lastsrc = 0;
            state->payload_algid = 0;
            state->payload_keyid = 0;
            // state->payload_miP = 0;
            //reset some strings
            sprintf (state->call_string[0], "%s", "                     "); //21 spaces
            sprintf (state->call_string[1], "%s", "                     "); //21 spaces
            sprintf (state->active_channel[0], "%s", "");
            sprintf (state->active_channel[1], "%s", "");
            opts->p25_is_tuned = 0;
            state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
            rtl_dev_tune (opts, state->p25_cc_freq);
            #endif
          }
        }
      }

      else fprintf (stderr, " Unknown Format %02X MFID %02X SVC %02X", lc_format, lc_mfid, lc_svcopt);
    }

    //TODO: Look through all LCW format messages and move here if they don't use the MFID field
    //just going to add/fix a few values I've observed for now, one issue with doing so may
    //be where there is a reserved field that is also used as an MFID, i.e., Call Termination 0xF vs Moto Talker EOT 0xF with
    //the reserved field showing the 0x90 for moto in it, and Harris 0xA vs Unit to Unit Voice Call 0xA

    //TODO: Add Identification of Special SU Address values
    //0 - No Unit
    //1-0xFFFFFB - Assignable Units
    //0xFFFFFC - FNE 16777212
    //0xFFFFFD - System Default (FNE Calling Functions, Registration, Mobility)
    //0xFFFFFE - Registration Default (registration transactions from SU)
    //0xFFFFFF - All Units

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x42) 
      fprintf (stderr, " Conventional Fallback Indication");

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x57)
      fprintf (stderr, " Extended Function Command");

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x58)
    {
      uint8_t iden = (uint8_t)ConvertBitIntoBytes(&LCW_bits[8], 4);
      uint32_t base = (uint32_t)ConvertBitIntoBytes(&LCW_bits[40], 32);
      fprintf (stderr, " Channel Identifier Update VU; Iden: %X; Base: %d;", iden, base*5);
    }

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x59)
    {
      uint8_t iden = (uint8_t)ConvertBitIntoBytes(&LCW_bits[8], 4);
      uint32_t base = (uint32_t)ConvertBitIntoBytes(&LCW_bits[40], 32);
      fprintf (stderr, " Channel Identifier Update VU; Iden: %X; Base: %d;", iden, base*5);
    }

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x61) 
      fprintf (stderr, " Secondary Control Channel Broadcast");

    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x62) 
      fprintf (stderr, " Adjacent Site Status Broadcast");
    
    //This lc_format doesn't use the MFID field
    else if (lc_format == 0x63) 
      fprintf (stderr, " RFSS Status Broadcast");

    //MFID 90 Embedded GPS
    else if (lc_mfid == 0x90 && lc_opcode == 0x6)
    {
      fprintf (stderr, " MFID90 (Moto)");
      apx_embedded_gps (opts, state, LCW_bits);
    }

    else if (lc_mfid == 0x90 && lc_opcode == 0x0)
    {
      //needed to fill this in, since tuning this on P1 will just leave the TG/SRC as zeroes
      fprintf (stderr, " MFID90 (Moto) Group Regroup Channel User (LCGRGR)");
      uint32_t sg = (uint32_t)ConvertBitIntoBytes(&LCW_bits[32], 16);
      uint32_t src = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24);
      fprintf (stderr, " SG: %d; SRC: %d;", sg, src);
      if (LCW_bits[16] == 1) fprintf (stderr, " Res;"); //res bit (octet 2)
      if (LCW_bits[17] == 1) fprintf (stderr, " ENC;"); //P-bit (octet 2)
      if (LCW_bits[31] == 1) fprintf (stderr, " EXT;"); //Full SUID next LC (external) (octet 3)
      state->lasttg = sg;
      state->lastsrc = src;
    }

    else if (lc_mfid == 0x90 && lc_opcode == 0x1)
    {
      fprintf (stderr, " MFID90 (Moto) Group Regroup Channel Update (LCGRGU)");
      uint32_t sg = (uint32_t)ConvertBitIntoBytes(&LCW_bits[24], 16);
      uint32_t ch = (uint32_t)ConvertBitIntoBytes(&LCW_bits[56], 16);
      fprintf (stderr, " SG: %d; CH: %04X;", sg, ch);
      if (LCW_bits[16] == 1) fprintf (stderr, " Res;"); //res bit (octet 2)
      if (LCW_bits[17] == 1) fprintf (stderr, " ENC;"); //P-bit (octet 2)
    }

    else if (lc_mfid == 0x90 && lc_opcode == 0x3)
      fprintf (stderr, " MFID90 (Moto) Group Regroup Add");

    else if (lc_mfid == 0x90 && lc_opcode == 0x4)
      fprintf (stderr, " MFID90 (Moto) Group Regroup Delete");

    else if (lc_mfid == 0x90 && lc_opcode == 0xF)
    {
      uint32_t src = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24);
      fprintf (stderr, " MFID90 (Moto) Talker EOT; SRC: %d;", src);
    }

    //observed format value on Harris SNDCP data channel (Phase 2 CC to Phase 1 MPDU channel)
    else if (lc_mfid == 0xA4 && lc_format == 0x0A)
    {
      //Could also just be a Unit to Unit Voice Channel User using the reserved field as the MFID?
      //if it were similar to Unit to Unit though, the TGT and SRC values seem to be reversed
      //this appears to be a data channel indicator, has a matching target and the FNE address in it
      uint32_t src = (uint32_t)ConvertBitIntoBytes(&LCW_bits[24], 24);
      uint32_t tgt = (uint32_t)ConvertBitIntoBytes(&LCW_bits[48], 24);
      fprintf (stderr, " MFIDA4 (Harris) Data Channel; SRC: %d; TGT: %d;", src, tgt);
    }

    //not a duplicate, this one will print if not MFID 0 or 1
    else
    {
      fprintf (stderr, " Unknown Format %02X MFID %02X ", lc_format, lc_mfid);
      if (lc_mfid == 0x90) fprintf (stderr, "(Moto)");
      else if (lc_mfid == 0xA4) fprintf (stderr, "(Harris)");
    }
  }

  //ending line break
  fprintf (stderr, "\n");

}


