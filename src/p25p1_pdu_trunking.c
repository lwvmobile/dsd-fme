/*-------------------------------------------------------------------------------
 * p25p1_pdu_trunking.c
 * P25p1 PDU Alt Format Trunking
 *
 * LWVMOBILE
 * 2025-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//trunking data delivered via PDU format
void p25_decode_pdu_trunking(dsd_opts * opts, dsd_state * state, uint8_t * mpdu_byte)
{
  //group list mode so we can look and see if we need to block tuning any groups, etc
  char mode[8]; //allow, block, digital, enc, etc
  sprintf (mode, "%s", "");

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

  uint8_t fmt = mpdu_byte[0] & 0x1F;
  uint8_t MFID = mpdu_byte[2];
  int blks = mpdu_byte[6] & 0x7F;
  uint8_t opcode = 0;

  if (fmt == 0x15) fprintf (stderr, " UNC");
  else fprintf (stderr, " ALT");
  fprintf (stderr, " MBT");
  if (fmt == 0x17) opcode = mpdu_byte[7] & 0x3F; //alt MBT
  else opcode = mpdu_byte[12] & 0x3F; //unconf MBT
  fprintf (stderr, " - OP: %02X", opcode);

  //NET_STS_BCST -- TIA-102.AABC-D 6.2.11.2
  if (opcode == 0x3B)
  {
    int lra = mpdu_byte[3];
    int sysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
    int res_a = mpdu_byte[8];
    int res_b = mpdu_byte[9];
    long int wacn = (mpdu_byte[12] << 12) | (mpdu_byte[13] << 4) | (mpdu_byte[14] >> 4);
    int channelt = (mpdu_byte[15] << 8) | mpdu_byte[16];
    int channelr = (mpdu_byte[17] << 8) | mpdu_byte[18];
    int ssc =  mpdu_byte[19];
    UNUSED3(res_a, res_b, ssc);
    fprintf (stderr, "%s",KYEL);
    fprintf (stderr, "\n Network Status Broadcast MBT - Extended \n");
    fprintf (stderr, "  LRA [%02X] WACN [%05lX] SYSID [%03X] NAC [%03llX]\n", lra, wacn, sysid, state->p2_cc);
    fprintf (stderr, "  CHAN-T [%04X] CHAN-R [%04X]", channelt, channelr);
    long int ct_freq = process_channel_to_freq(opts, state, channelt);
    long int cr_freq = process_channel_to_freq(opts, state, channelr);
    UNUSED(cr_freq);

    state->p25_cc_freq = ct_freq;
    state->p25_cc_is_tdma = 0; //flag off for CC tuning purposes when system is qpsk

    //place the cc freq into the list at index 0 if 0 is empty, or not the same,
    //so we can hunt for rotating CCs without user LCN list
    if (state->trunk_lcn_freq[0] == 0 || state->trunk_lcn_freq[0] != state->p25_cc_freq)
    {
      state->trunk_lcn_freq[0] = state->p25_cc_freq;
    }

    //only set IF these values aren't already hard set by the user
    if (state->p2_hardset == 0)
    {
      state->p2_wacn = wacn;
      state->p2_sysid = sysid;
    }
  }
  //RFSS Status Broadcast - Extended 6.2.15.2
  else if (opcode == 0x3A)
  {
    int lra = mpdu_byte[3];
    int lsysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
    int rfssid = mpdu_byte[12];
    int siteid = mpdu_byte[13];
    int channelt = (mpdu_byte[14] << 8) | mpdu_byte[15];
    int channelr = (mpdu_byte[16] << 8) | mpdu_byte[17];
    int sysclass = mpdu_byte[18];
    fprintf (stderr, "%s",KYEL);
    fprintf (stderr, "\n RFSS Status Broadcast MBF - Extended \n");
    fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%03d] SITE ID [%03d]\n  CHAN-T [%04X] CHAN-R [%02X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channelt, channelr, sysclass);
    process_channel_to_freq (opts, state, channelt);
    process_channel_to_freq (opts, state, channelr);

    state->p2_siteid = siteid;
    state->p2_rfssid = rfssid;
  }

  //Adjacent Status Broadcast (ADJ_STS_BCST) Extended 6.2.2.2
  else if (opcode == 0x3C)
  {
    int lra = mpdu_byte[3];
    int cfva = mpdu_byte[4] >> 4;
    int lsysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
    int rfssid = mpdu_byte[8];
    int siteid = mpdu_byte[9];
    int channelt = (mpdu_byte[12] << 8) | mpdu_byte[13];
    int channelr = (mpdu_byte[14] << 8) | mpdu_byte[15];
    int sysclass = mpdu_byte[16];
    long int wacn = (mpdu_byte[17] << 12) | (mpdu_byte[18] << 4) | (mpdu_byte[19] >> 4);
    fprintf (stderr, "%s",KYEL);
    fprintf (stderr, "\n Adjacent Status Broadcast - Extended\n");
    fprintf (stderr, "  LRA [%02X] CFVA [%X] RFSS[%03d] SITE [%03d] SYSID [%03X]\n  CHAN-T [%04X] CHAN-R [%04X] SSC [%02X] WACN [%05lX]\n  ", lra, cfva, rfssid, siteid, lsysid, channelt, channelr, sysclass, wacn);
    if (cfva & 0x8) fprintf (stderr, " Conventional");
    if (cfva & 0x4) fprintf (stderr, " Failure Condition");
    if (cfva & 0x2) fprintf (stderr, " Up to Date (Correct)");
    else fprintf (stderr, " Last Known");
    if (cfva & 0x1) fprintf (stderr, " Valid RFSS Connection Active");
    process_channel_to_freq (opts, state, channelt);
    process_channel_to_freq (opts, state, channelr);

  }

  //Group Voice Channel Grant - Extended
  else if (opcode == 0x0)
  {
    int svc = mpdu_byte[8];
    int channelt  = (mpdu_byte[14] << 8) | mpdu_byte[15];
    int channelr  = (mpdu_byte[16] << 8) | mpdu_byte[17];
    long int source = (mpdu_byte[3] << 16) |(mpdu_byte[4] << 8) | mpdu_byte[5];
    int group = (mpdu_byte[18] << 8) | mpdu_byte[19];
    long int freq1 = 0;
    long int freq2 = 0;
    UNUSED2(source, freq2);
    fprintf (stderr, "%s\n ",KYEL);
    if (svc & 0x80) fprintf (stderr, " Emergency");
    if (svc & 0x40) fprintf (stderr, " Encrypted");

    if (opts->payload == 1) //hide behind payload due to len
    {
      if (svc & 0x20) fprintf (stderr, " Duplex");
      if (svc & 0x10) fprintf (stderr, " Packet");
      else fprintf (stderr, " Circuit");
      if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
      fprintf (stderr, " Priority %d", svc & 0x7); //call priority
    }
    fprintf (stderr, " Group Voice Channel Grant Update - Extended");
    fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc, channelt, channelr, group, group);
    freq1 = process_channel_to_freq (opts, state, channelt);
    freq2 = process_channel_to_freq (opts, state, channelr);

    //add active channel to string for ncurses display
    sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; ", channelt, group);
    state->last_active_time = time(NULL);

    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == group)
      {
        fprintf (stderr, " [%s]", state->group_array[i].groupName);
        strcpy (mode, state->group_array[i].groupMode);
        break;
      }
    }

    //TG hold on P25p1 Ext -- block non-matching target, allow matching group
    if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
    if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");

    //Skip tuning group calls if group calls are disabled
    if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

    //Skip tuning encrypted calls if enc calls are disabled
    if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

    //tune if tuning available
    if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
    {
      //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
      if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
      {

        //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
        if (1 == 1)
        {
          if (state->p25_chan_tdma[channelt >> 12] == 1)
          {
            state->samplesPerSymbol = 8;
            state->symbolCenter = 3;

            //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
            //this will only occur in realtime tuning, not not required .bin or .wav playback
            if (channelt & 1) //VCH1
            {
              opts->slot1_on = 0;
              opts->slot2_on = 1;
            }
            else //VCH0
            {
              opts->slot1_on = 1;
              opts->slot2_on = 0;
            }

          }
        }
        //rigctl
        if (opts->use_rigctl == 1)
        {
          if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
          SetFreq(opts->rigctl_sockfd, freq1);
          state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
          opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
          state->last_vc_sync_time = time(NULL);
        }
        //rtl
        else if (opts->audio_in_type == 3)
        {
          #ifdef USE_RTLSDR
          rtl_dev_tune (opts, freq1);
          state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
          opts->p25_is_tuned = 1;
          state->last_vc_sync_time = time(NULL);
          #endif
        }
      }
    }
  }

  //Unit to Unit Voice Channel Grant - Extended
  else if (opcode == 0x6)
  {
    //I'm not doing EVERY element of this, just enough for tuning!
    int svc = mpdu_byte[8];
    int channelt  = (mpdu_byte[22] << 8) | mpdu_byte[23];
    int channelr  = (mpdu_byte[24] << 8) | mpdu_byte[25]; //optional!
    //using source and target address, not source and target id (is this correct?)
    long int source = (mpdu_byte[3] << 16)  | (mpdu_byte[4] << 8) | mpdu_byte[5];
    long int target = (mpdu_byte[19] << 16) |(mpdu_byte[20] << 8) | mpdu_byte[21];
    //TODO: Test Full Values added here for SUID, particular tgt_nid and tgt_sid
    long int src_nid = (mpdu_byte[12] << 24) | (mpdu_byte[13] << 16) | (mpdu_byte[14] << 8) | mpdu_byte[15];
    long int src_sid = (mpdu_byte[16] << 16) | (mpdu_byte[17] << 8) | mpdu_byte[18];
    long int tgt_nid = (mpdu_byte[26] << 16) | (mpdu_byte[27] << 8) | mpdu_byte[28]; //only has 3 octets on tgt nid, partial only?
    long int tgt_sid = (mpdu_byte[29] << 16) | (mpdu_byte[30] << 8) | mpdu_byte[31];
    long int freq1 = 0;
    long int freq2 = 0;
    UNUSED(freq2);
    fprintf (stderr, "%s\n ",KYEL);
    if (svc & 0x80) fprintf (stderr, " Emergency");
    if (svc & 0x40) fprintf (stderr, " Encrypted");

    if (opts->payload == 1) //hide behind payload due to len
    {
      if (svc & 0x20) fprintf (stderr, " Duplex");
      if (svc & 0x10) fprintf (stderr, " Packet");
      else fprintf (stderr, " Circuit");
      if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
      fprintf (stderr, " Priority %d", svc & 0x7); //call priority
    }
    fprintf (stderr, " Unit to Unit Voice Channel Grant Update - Extended");
    fprintf (stderr, "\n  SVC: %02X; CHAN-T: %04X; CHAN-R: %04X; SRC: %ld; TGT: %ld; FULL SRC: %08lX-%08ld; FULL TGT: %08lX-%08ld;", svc, channelt, channelr, source, target, src_nid, src_sid, tgt_nid, tgt_sid);
    freq1 = process_channel_to_freq (opts, state, channelt);
    freq2 = process_channel_to_freq (opts, state, channelr); //optional!

    //add active channel to string for ncurses display
    sprintf (state->active_channel[0], "Active Ch: %04X TGT: %ld; ", channelt, target);

    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == target)
      {
        fprintf (stderr, " [%s]", state->group_array[i].groupName);
        strcpy (mode, state->group_array[i].groupMode);
        break;
      }
    }

    //TG hold on P25p1 Ext UU -- will want to disable UU_V grants while TG Hold enabled
    if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
    // if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");

    //Skip tuning private calls if group calls are disabled
    if (opts->trunk_tune_private_calls == 0) goto SKIPCALL;

    //Skip tuning encrypted calls if enc calls are disabled
    if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

    //tune if tuning available
    if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
    {
      //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
      if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
      {

        //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
        if (1 == 1)
        {
          if (state->p25_chan_tdma[channelt >> 12] == 1)
          {
            state->samplesPerSymbol = 8;
            state->symbolCenter = 3;

            //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
            //this will only occur in realtime tuning, not not required .bin or .wav playback
            if (channelt & 1) //VCH1
            {
              opts->slot1_on = 0;
              opts->slot2_on = 1;
            }
            else //VCH0
            {
              opts->slot1_on = 1;
              opts->slot2_on = 0;
            }

          }
        }
        //rigctl
        if (opts->use_rigctl == 1)
        {
          if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
          SetFreq(opts->rigctl_sockfd, freq1);
          state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
          opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
          state->last_vc_sync_time = time(NULL);
        }
        //rtl
        else if (opts->audio_in_type == 3)
        {
          #ifdef USE_RTLSDR
          rtl_dev_tune (opts, freq1);
          state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
          opts->p25_is_tuned = 1;
          state->last_vc_sync_time = time(NULL);
          #endif
        }
      }
    }
  }

  //Telephone Interconnect Voice Channel Grant (or Update) -- Explicit Channel Form
  else if ( (opcode == 0x8 || opcode == 0x9) && MFID < 2) //This form does allow for other MFID's but Moto has a seperate function on 9
  {
    //TELE_INT_CH_GRANT or TELE_INT_CH_GRANT_UPDT
    int svc = mpdu_byte[8];
    int channel = (mpdu_byte[12] << 8) | mpdu_byte[13];
    int timer   = (mpdu_byte[16] << 8) | mpdu_byte[17];
    int target  = (mpdu_byte[3] << 16) | (mpdu_byte[4] << 8) | mpdu_byte[5];
    long int freq = 0;
    fprintf (stderr, "\n");
    if (svc & 0x80) fprintf (stderr, " Emergency");
    if (svc & 0x40) fprintf (stderr, " Encrypted");

    if (opts->payload == 1) //hide behind payload due to len
    {
      if (svc & 0x20) fprintf (stderr, " Duplex");
      if (svc & 0x10) fprintf (stderr, " Packet");
      else fprintf (stderr, " Circuit");
      if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
      fprintf (stderr, " Priority %d", svc & 0x7); //call priority
    }

    fprintf (stderr, " Telephone Interconnect Voice Channel Grant");
    if ( opcode & 1) fprintf (stderr, " Update");
    fprintf (stderr, " Extended");
    fprintf (stderr, "\n  CHAN: %04X; Timer: %f Seconds; Target: %d;", channel, (float)timer*0.1f, target); //timer unit is 100 ms, or 0.1 seconds
    freq = process_channel_to_freq (opts, state, channel);

    //add active channel to string for ncurses display
    sprintf (state->active_channel[0], "Active Tele Ch: %04X TGT: %d; ", channel, target);
    state->last_active_time = time(NULL);

    //Skip tuning private calls if private calls is disabled (are telephone int calls private, or talkgroup?)
    if (opts->trunk_tune_private_calls == 0) goto SKIPCALL;

    //Skip tuning encrypted calls if enc calls are disabled
    if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

    //telephone only has a target address (manual shows combined source/target of 24-bits)
    for (int i = 0; i < state->group_tally; i++)
    {
      if (state->group_array[i].groupNumber == target)
      {
        fprintf (stderr, " [%s]", state->group_array[i].groupName);
        strcpy (mode, state->group_array[i].groupMode);
        break;
      }
    }

    //TG hold on UU_V -- will want to disable UU_V grants while TG Hold enabled
    if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");

    //tune if tuning available
    if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
    {
      //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
      if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
      {
        //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
        if (1 == 1)
        {
          if (state->p25_chan_tdma[channel >> 12] == 1)
          {
            state->samplesPerSymbol = 8;
            state->symbolCenter = 3;

            //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
            //this will only occur in realtime tuning, not not required .bin or .wav playback
            if (channel & 1) //VCH1
            {
              opts->slot1_on = 0;
              opts->slot2_on = 1;
            }
            else //VCH0
            {
              opts->slot1_on = 1;
              opts->slot2_on = 0;
            }
          }

        }

        //rigctl
        if (opts->use_rigctl == 1)
        {
          if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
          SetFreq(opts->rigctl_sockfd, freq);
          if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
          opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
          state->last_vc_sync_time = time(NULL);
        }
        //rtl
        else if (opts->audio_in_type == 3)
        {
          #ifdef USE_RTLSDR
          rtl_dev_tune (opts, freq);
          if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
          opts->p25_is_tuned = 1;
          state->last_vc_sync_time = time(NULL);
          #endif
        }
      }
    }
    if (opts->p25_trunk == 0)
    {
      if (target == state->lasttg || target == state->lasttgR)
      {
        //P1 FDMA
        if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
        //P2 TDMA
        else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
      }
    }

  }

  //look at Harris Opcodes and payload portion of MPDU
  else if (MFID == 0xA4)
  {
    //TODO: Add Known Opcodes from Manual (all one of them)
    fprintf (stderr, "%s",KCYN);
    fprintf (stderr, "\n MFID A4 (Harris); Opcode: %02X; ", opcode);
    for (int i = 0; i < (12*(blks+1)%37); i++)
      fprintf (stderr, "%02X", mpdu_byte[i]);
    fprintf (stderr, " %s",KNRM);
  }

  //look at Motorola Opcodes and payload portion of MPDU
  else if (MFID == 0x90)
  {
    //TIA-102.AABH
    if (opcode == 0x02)
    {
      int svc = mpdu_byte[8]; //Just the Res, P-bit, and more res bits
      int channelt  = (mpdu_byte[12] << 8) | mpdu_byte[13];
      int channelr  = (mpdu_byte[14] << 8) | mpdu_byte[15];
      long int source = (mpdu_byte[3] << 16) |(mpdu_byte[4] << 8) | mpdu_byte[5];
      int group = (mpdu_byte[16] << 8) | mpdu_byte[17];
      long int freq1 = 0;
      long int freq2 = 0;
      UNUSED2(source, freq2);
      fprintf (stderr, "%s\n ",KYEL);

      if (svc & 0x40) fprintf (stderr, " Encrypted"); //P-bit

      fprintf (stderr, " MFID90 Group Regroup Channel Grant - Explicit");
      fprintf (stderr, "\n  RES/P [%02X] CHAN-T [%04X] CHAN-R [%04X] SG [%d][%04X]", svc, channelt, channelr, group, group);
      freq1 = process_channel_to_freq (opts, state, channelt);
      freq2 = process_channel_to_freq (opts, state, channelr);

      //add active channel to string for ncurses display
      sprintf (state->active_channel[0], "MFID90 Ch: %04X SG: %d ", channelt, group);
      state->last_active_time = time(NULL);

      for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == group)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
          break;
        }
      }

      //TG hold on MFID90 GRG -- block non-matching target, allow matching group
      if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
      if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");

      //Skip tuning group calls if group calls are disabled
      if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

      //Skip tuning encrypted calls if enc calls are disabled
      if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

      //tune if tuning available
      if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
      {
        //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
        if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
        {

          //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
          if (1 == 1)
          {
            if (state->p25_chan_tdma[channelt >> 12] == 1)
            {
              state->samplesPerSymbol = 8;
              state->symbolCenter = 3;

              //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
              //this will only occur in realtime tuning, not not required .bin or .wav playback
              if (channelt & 1) //VCH1
              {
                opts->slot1_on = 0;
                opts->slot2_on = 1;
              }
              else //VCH0
              {
                opts->slot1_on = 1;
                opts->slot2_on = 0;
              }

            }
          }
          //rigctl
          if (opts->use_rigctl == 1)
          {
            if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
            SetFreq(opts->rigctl_sockfd, freq1);
            state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
            opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
            state->last_vc_sync_time = time(NULL);
          }
          //rtl
          else if (opts->audio_in_type == 3)
          {
            #ifdef USE_RTLSDR
            rtl_dev_tune (opts, freq1);
            state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
            opts->p25_is_tuned = 1;
            state->last_vc_sync_time = time(NULL);
            #endif
          }
        }
      }
    }

    else
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n MFID 90 (Moto); Opcode: %02X; ", mpdu_byte[0] & 0x3F);
      for (int i = 0; i < (12*(blks+1)%37); i++)
        fprintf (stderr, "%02X", mpdu_byte[i]);
      fprintf (stderr, " %s",KNRM);
    }
  }

  else
  {
    fprintf (stderr, "%s",KCYN);
    fprintf (stderr, "\n MFID %02X (Unknown); Opcode: %02X; ", MFID, mpdu_byte[0] & 0x3F);
    for (int i = 0; i < (12*(blks+1)%37); i++)
      fprintf (stderr, "%02X", mpdu_byte[i]);
    fprintf (stderr, " %s",KNRM);
  }

  SKIPCALL: ; //do nothing
}