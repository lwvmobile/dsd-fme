/*-------------------------------------------------------------------------------
 * p25p2_vpdu.c
 * Phase 2 Variable PDU (and TSBK PDU) Handling
 *
 * LWVMOBILE
 * 2022-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//MAC message lengths
static const uint8_t mac_msg_len[256] = {
	 0,  7,  8,  7,  0, 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //0F
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //1F
	 0, 14, 15,  0,  0, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //2F
	 5,  7,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //3F 
	 9,  7,  9,  0,  9,  8,  9,  0, 10, 10,  9,  0, 10,  0,  0,  0, //4F
	 0,  0,  0,  0,  9,  7,  0,  0, 10,  0,  7,  0, 10,  8, 14,  7, //5F
	 9,  9,  0,  0,  9,  0,  0,  9, 10,  0,  7, 10, 10,  7,  0,  9, //6F
	 9, 29,  9,  9,  9,  9, 10, 13,  9,  9,  9, 11,  9,  9,  0,  0, //7F
	 8,  18,  0,  7, 11,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  7, //8F (needed to add 81 and 8f for Harris)
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //9F
	16,  0,  0, 11, 13, 11, 11, 11, 10,  0,  0,  0,  0,  0,  0,  0, //AF
	17,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //BF, B0 was 0, set to 17 (Harris again)
	11,  0,  0,  8, 15, 12, 15, 32, 12, 12,  0, 27, 14, 29, 29, 32, //CF
	 0,  0,  0,  0,  0,  0,  9,  0, 14, 29, 11, 27, 14,  0, 40, 11, //DF 
	28,  0,  0, 14, 17, 14,  0,  0, 16,  8, 11,  0, 13, 19,  0,  0, //EF
	 0,  0, 16, 14,  0,  0, 12,  0, 22,  0, 11, 13, 11,  0, 15,  0 }; //FF


//MAC PDU 3-bit Opcodes BBAC (8.4.1) p 123:
//0 - reserved //1 - Mac PTT //2 - Mac End PTT //3 - Mac Idle //4 - Mac Active
//5 - reserved //6 - Mac Hangtime //7 - reserved //Mac PTT BBAC p80

//TODO: Check for Non standard MFIDs first MAC[1], then set len on the MAC[2] if
//the result from the len table is 0 (had to manually enter a few observed values from Harris)
void process_MAC_VPDU(dsd_opts * opts, dsd_state * state, int type, unsigned long long int MAC[24])
{
	//handle variable content MAC PDUs (Active, Idle, Hangtime, or Signal)
	//use type to specify SACCH or FACCH, so we know if we should invert the currentslot when assigning ids etc

	//b values - 0 = Unique TDMA Message,  1 Phase 1 OSP/ISP abbreviated
	// 2 = Manufacturer Message, 3 Phase 1 OSP/ISP extended/explicit

	int len_a = 0; 
	int len_b = mac_msg_len[MAC[1]]; 
	int len_c = 0;

	//sanity check
	if (len_b < 19 && type == 1)
	{
		len_c = mac_msg_len[MAC[1+len_b]];
	}
	if (len_b < 16 && type == 0)
	{
		len_c = mac_msg_len[MAC[1+len_b]];
	}

	int slot = 9;
	if (type == 1) //0 for F, 1 for S
	{
		slot = (state->currentslot ^ 1) & 1; //flip slot internally for SACCH
	}
	else slot = state->currentslot;

	//assigning here if OECI MAC SIGNAL, after passing RS and CRC
	if (state->p2_is_lcch == 1)
	{
		//fix for blinking SIGNAL on Slot 2 during inverted slot in Ncurses
		//TEMP: assume LCH 0 is the SIGNAL slot, fix for blinking SIGNAL on Slot 2 during inverted slot
		if (type == 0 && slot == 0) state->dmrburstL = 30;
		if (type == 1 && slot == 0) state->dmrburstL = 30;
		// if (type == 0 && slot == 1) state->dmrburstR = 30; 
		// if (type == 1 && slot == 1) state->dmrburstR = 30;

		//Temp Fix: Disable Slot Playback when MAC_SIGNAL present (good CRC),
		//during trunking, will re-enable when call grant is received, prevent lagging:
		if (opts->p25_trunk == 1 && opts->p25_is_tuned == 0) opts->slot1_on = 0;
		if (opts->p25_trunk == 1 && opts->p25_is_tuned == 0) opts->slot2_on = 0;

		//TODO: Iron out issues with audio playing in every non SACCH slot when no voice present
		//without it stuttering during actual voice

	}


	if (len_b == 0 || len_b > 18) 
	{
		goto END_PDU;
	}

	//group list mode so we can look and see if we need to block tuning any groups, etc
	char mode[8]; //allow, block, digital, enc, etc
	sprintf (mode, "%s", "");

	//if we are using allow/whitelist mode, then write 'B' to mode for block
	//comparison below will look for an 'A' to write to mode if it is allowed
	if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

	for (int i = 0; i < 2; i++) 
	{

		//MFID90 Voice Grants, A3, A4, and A5 <--I bet A4 here was triggering a phantom call when TSBK sent PDUs here
		//MFID90 Group Regroup Channel Grant - Implicit
		if (MAC[1+len_a] == 0xA3 && MAC[2+len_a] == 0x90)
		{
			int mfid = MAC[2+len_a];
			UNUSED(mfid);
			int channel  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int sgroup = (MAC[7+len_a] << 8) | MAC[8+len_a];
			long int freq = 0;
			fprintf (stderr, "\n MFID90 Group Regroup Channel Grant - Implicit");
			fprintf (stderr, "\n  CHAN [%04X] Group [%d][%04X]", channel, sgroup, sgroup);
			freq = process_channel_to_freq (opts, state, channel);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "MFID90 Active Ch: %04X SG: %d; ", channel, sgroup);
			state->last_active_time = time(NULL);

			for (int i = 0; i < state->group_tally; i++)
			{
				if (state->group_array[i].groupNumber == sgroup)
				{
					fprintf (stderr, " [%s]", state->group_array[i].groupName);
					strcpy (mode, state->group_array[i].groupMode);
					break;
				}
			}

			//TG hold on MFID90 GRG -- block non-matching super group, allow matching group
			if (state->tg_hold != 0 && state->tg_hold != sgroup) sprintf (mode, "%s", "B");
			if (state->tg_hold != 0 && state->tg_hold == sgroup)
			{
				sprintf (mode, "%s", "A");
				opts->p25_is_tuned = 0; //unlock tuner
			}

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

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
							// if (channel & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }

						}
					}
					
					//rigctl
					if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
						SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
						state->last_vc_sync_time = time(NULL);
					}
					//rtl
					else if (opts->audio_in_type == 3)
					{
						#ifdef USE_RTLSDR
						rtl_dev_tune (opts, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1;
						state->last_vc_sync_time = time(NULL);
						#endif
					}
				}    
			}
			//if playing back files, and we still want to see what freqs are in use in the ncurses terminal
			//might only want to do these on a grant update, and not a grant by itself?
			if (opts->p25_trunk == 0)
			{
				//P1 FDMA
				if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
				//P2 TDMA
				else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
			}	
		}

		//MFID90 Group Regroup Channel Grant - Explicit
		if (MAC[1+len_a] == 0xA4 && MAC[2+len_a] == 0x90)
		{
			int mfid = MAC[2+len_a];
			int channel  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int channelr = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sgroup = (MAC[9+len_a] << 8) | MAC[10+len_a];
			long int freq = 0;
			UNUSED2(mfid, channelr);
			fprintf (stderr, "\n MFID90 Group Regroup Channel Grant - Explicit");
			fprintf (stderr, "\n  CHAN [%04X] Group [%d][%04X]", channel, sgroup, sgroup);
			freq = process_channel_to_freq (opts, state, channel);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "MFID90 Active Ch: %04X SG: %d ", channel, sgroup);
			state->last_active_time = time(NULL);

			for (int i = 0; i < state->group_tally; i++)
			{
				if (state->group_array[i].groupNumber == sgroup)
				{
					fprintf (stderr, " [%s]", state->group_array[i].groupName);
					strcpy (mode, state->group_array[i].groupMode);
					break;
				}
			}

			//TG hold on MFID90 GRG -- block non-matching super group, allow matching group
			if (state->tg_hold != 0 && state->tg_hold != sgroup) sprintf (mode, "%s", "B");
			if (state->tg_hold != 0 && state->tg_hold == sgroup)
			{
				sprintf (mode, "%s", "A");
				opts->p25_is_tuned = 0; //unlock tuner
			}

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

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
							// if (channel & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }

						}
					}
					
					if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
						SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
						state->last_vc_sync_time = time(NULL);
					}
					//rtl
					else if (opts->audio_in_type == 3)
					{
						#ifdef USE_RTLSDR
						rtl_dev_tune (opts, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1;
						state->last_vc_sync_time = time(NULL);
						#endif
					}
				}    
			}
			//if playing back files, and we still want to see what freqs are in use in the ncurses terminal
			//might only want to do these on a grant update, and not a grant by itself?
			if (opts->p25_trunk == 0)
			{
				if (sgroup == state->lasttg || sgroup == state->lasttgR)
				{
					//P1 FDMA
					if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
					//P2 TDMA
					else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
				}
			}
		}

		//MFID90 Group Regroup Channel Grant Update
		if (MAC[1+len_a] == 0xA5 && MAC[2+len_a] == 0x90)
		{
			int channel1  = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int group1 = (MAC[6+len_a] << 8) | MAC[7+len_a];
			int channel2  = (MAC[8+len_a] << 8) | MAC[9+len_a];
			int group2 = (MAC[10+len_a] << 8) | MAC[11+len_a];
			long int freq1 = 0;
			long int freq2 = 0;

			fprintf (stderr, "\n MFID90 Group Regroup Channel Grant Update");
			fprintf (stderr, "\n  Channel 1 [%04X] Group 1 [%d][%04X]", channel1, group1, group1);
			freq1 = process_channel_to_freq (opts, state, channel1);
			if (channel2 != channel1 && channel2 != 0 && channel2 != 0xFFFF)
			{
				fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
				freq2 = process_channel_to_freq (opts, state, channel2);
			}

			//add active channel to string for ncurses display
			if (channel2 != channel1 && channel2 != 0 && channel2 != 0xFFFF)
				sprintf (state->active_channel[0], "MFID90 Active Ch: %04X SG: %d; Ch: %04X SG: %d; ", channel1, group1, channel2, group2);
			else sprintf (state->active_channel[0], "MFID90 Active Ch: %04X SG: %d; ", channel1, group1);
			state->last_active_time = time(NULL);

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

			//monstrocity below should get us evaluating and tuning groups...hopefully, will be first come first served though, no priority
			//see how many loops we need to run on when to tune if first group is blocked
			int loop = 1;
			if (channel1 == channel2) loop = 1;
			else loop = 2;
			//assigned inside loop
			long int tunable_freq = 0;
			int tunable_chan = 0; 
			int tunable_group = 0;

			for (int j = 0; j < loop; j++)
			{
				//assign our internal variables for check down on if to tune one freq/group or not
				if (j == 0)
				{
					tunable_freq = freq1;
					tunable_chan = channel1;
					tunable_group = group1;
				}
				else 
				{
					tunable_freq = freq2;
					tunable_chan = channel2;
					tunable_group = group2;
				}

				for (int i = 0; i < state->group_tally; i++)
				{
					if (state->group_array[i].groupNumber == tunable_group)
					{
						fprintf (stderr, " [%s]", state->group_array[i].groupName);
						strcpy (mode, state->group_array[i].groupMode);
						break;
					}
				}

				//TG hold on MFID90 GRG -- block non-matching super group, allow matching group
				if (state->tg_hold != 0 && state->tg_hold != tunable_group) sprintf (mode, "%s", "B");
				if (state->tg_hold != 0 && state->tg_hold == tunable_group)
				{
					sprintf (mode, "%s", "A");
					opts->p25_is_tuned = 0; //unlock tuner
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
						if (1 == 1)
						{
							if (state->p25_chan_tdma[tunable_chan >> 12] == 1)
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;

								//shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
								//this will only occur in realtime tuning, not not required .bin or .wav playback
								// if (tunable_chan & 1) //VCH1
								// {
								// 	opts->slot1_on = 0;
								// 	opts->slot2_on = 1;
								// }
								// else //VCH0
								// {
								// 	opts->slot1_on = 1;
								// 	opts->slot2_on = 0;
								// }

							}
						}

						//rigctl
						if (opts->use_rigctl == 1)
						{
							if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
							SetFreq(opts->rigctl_sockfd, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
							
						}
						//rtl
						else if (opts->audio_in_type == 3)
						{
							#ifdef USE_RTLSDR
							rtl_dev_tune (opts, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
							#endif
						}
					}    
				}
				//if playing back files, and we still want to see what freqs are in use in the ncurses terminal
				//might only want to do these on a grant update, and not a grant by itself?
				if (opts->p25_trunk == 0)
				{
					if (tunable_group == state->lasttg || tunable_group == state->lasttgR)
					{
						//P1 FDMA
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = tunable_freq;
						//P2 TDMA
						else state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
					}
				}
			}
		}

		//Standard P25 Tunable Commands
		//Group Voice Channel Grant (GRP_V_CH_GRANT)
		if (MAC[1+len_a] == 0x40)
		{
			int svc      = MAC[2+len_a];
			int channel = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int group   = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
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

			fprintf (stderr, " Group Voice Channel Grant");
			fprintf (stderr, "\n  SVC [%02X] CHAN [%04X] Group [%d] Source [%d]", svc, channel, group, source);
			freq = process_channel_to_freq (opts, state, channel);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; ", channel, group);
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

			//TG hold on GRP_V -- block non-matching group, allow matching group
			if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
			if (state->tg_hold != 0 && state->tg_hold == group)
			{
				sprintf (mode, "%s", "A");
				opts->p25_is_tuned = 0; //unlock tuner
			}

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

			//Skip tuning encrypted calls if enc calls are disabled
			if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

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
							// if (channel & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }

						}
					}

					//rigctl
					if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
						SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
						state->last_vc_sync_time = time(NULL);
					}
					//rtl
					else if (opts->audio_in_type == 3)
					{
						#ifdef USE_RTLSDR
						rtl_dev_tune (opts, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1;
						state->last_vc_sync_time = time(NULL);
						#endif
					}
				}    
			}
			//if playing back files, and we still want to see what freqs are in use in the ncurses terminal
			//might only want to do these on a grant update, and not a grant by itself?
			if (opts->p25_trunk == 0)
			{
				if (group == state->lasttg || group == state->lasttgR)
				{
					//P1 FDMA
					if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
					//P2 TDMA
					else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
				}
			}
		}

		//Telephone Interconnect Voice Channel Grant (or Update) -- Implicit and Explicit (MFID != Moto and opcode 8 and opcode 9)
		if (MAC[1+len_a] == 0x48 || MAC[1+len_a] == 0x49 || MAC[1+len_a] == 0xC8 || MAC[1+len_a] == 0xC9)
		{
			//TELE_INT_CH_GRANT or TELE_INT_CH_GRANT_UPDT
			int k = 1; //vPDU
			if (MAC[len_a] == 0x07) k = 0; //TSBK
			int svc = MAC[2+len_a+k];
			int channel = (MAC[3+len_a+k] << 8) | MAC[4+len_a+k];
			int timer   = (MAC[5+len_a+k] << 8) | MAC[6+len_a+k];
			int target  = (MAC[7+len_a+k] << 16) | (MAC[8+len_a+k] << 8) | MAC[9+len_a+k];
			long int freq = 0;
			if ( MAC[1+len_a] & 0x80) //vPDU only
			{
				timer   = (MAC[8+len_a] << 8) | MAC[9+len_a];
				target  = (MAC[10+len_a] << 16) | (MAC[11+len_a] << 8) | MAC[12+len_a];
			}

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
			if ( MAC[1+len_a] & 0x01) fprintf (stderr, " Update");
			if ( MAC[1+len_a] & 0x80) fprintf (stderr, " Explicit");
			else fprintf (stderr, " Implicit");
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

			//TG hold on UU_V -- will want to disable UU_V grants while TG Hold enabled -- same for Telephone?
			if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
			// if (state->tg_hold != 0 && state->tg_hold == target)
			// {
			// 	sprintf (mode, "%s", "A");
			// 	opts->p25_is_tuned = 0; //unlock tuner
			// }

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
							// if (channel & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }
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

		//Unit-to-Unit Voice Service Channel Grant (UU_V_CH_GRANT), or Grant Update (same format)
		if (MAC[1+len_a] == 0x44 || MAC[1+len_a] == 0x46 || MAC[1+len_a] == 0xC4) //double check these opcodes
		{
			int channel = (MAC[2+len_a] << 8) | MAC[3+len_a];
			int target  = (MAC[4+len_a] << 16) | (MAC[5+len_a] << 8) | MAC[6+len_a];
			int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			unsigned long long int src_suid = 0;
			long int freq = 0;

			if (MAC[1+len_a] == 0x21)
			{
				src_suid = (MAC[6+len_a] << 48ULL) | (MAC[7+len_a] << 40ULL) | (MAC[8+len_a] << 32ULL) | (MAC[9+len_a] << 24ULL) |
										(MAC[10+len_a] << 16ULL) | (MAC[11+len_a] << 8ULL) | (MAC[12+len_a] << 0ULL);

				source = src_suid & 0xFFFFFF;

				target  = (MAC[13+len_a] << 16) | (MAC[14+len_a] << 8) | MAC[15+len_a];

				// channel = (MAC[4+len_a] << 8) | MAC[5+len_a]; //CH-R, above is CH-T value
			}

			fprintf (stderr, "\n Unit to Unit Channel Grant");
			if ( MAC[1+len_a] == 0x46) fprintf (stderr, " Update");
			if ( MAC[1+len_a] == 0xC4) fprintf (stderr, " Extended");
			fprintf (stderr, "\n  CHAN: %04X; SRC: %d; TGT: %d; ", channel, source, target);
			if (MAC[1+len_a] == 0xC4) fprintf (stderr, "SUID: %08llX-%08d; ", src_suid >> 24, source);
			freq = process_channel_to_freq (opts, state, channel);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "Active Ch: %04X TGT: %d; ", channel, target);
			state->last_active_time = time(NULL);

			//Skip tuning private calls if private calls is disabled
			if (opts->trunk_tune_private_calls == 0) goto SKIPCALL; 

			//Skip tuning encrypted calls if enc calls are disabled -- abb formats do not carry svc bits :(
			// if (opts->trunk_tune_enc_calls == 0) goto SKIPCALL; //enable, or disable?

			//unit to unit needs work, may fail under certain conditions (first blocked, second allowed, etc) (labels should still work though)
			for (int i = 0; i < state->group_tally; i++)
			{
				if (state->group_array[i].groupNumber == source || state->group_array[i].groupNumber == target)
				{
					fprintf (stderr, " [%s]", state->group_array[i].groupName);
					strcpy (mode, state->group_array[i].groupMode);
					break;
				}
			}

			//TG hold on UU_V -- will want to disable UU_V grants while TG Hold enabled
			if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
			// if (state->tg_hold != 0 && state->tg_hold == target)
			// {
			// 	sprintf (mode, "%s", "A");
			// 	opts->p25_is_tuned = 0; //unlock tuner
			// }

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
							// if (channel & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }
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

		//Group Voice Channel Grant Update Multiple - Explicit
		if (MAC[1+len_a] == 0x25)
		{
			int svc1 = MAC[2+len_a];
			int channelt1  = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int channelr1  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int group1 = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int svc2 = MAC[9+len_a];
			int channelt2  = (MAC[10+len_a] << 8) | MAC[11+len_a];
			int channelr2  = (MAC[12+len_a] << 8) | MAC[13+len_a];
			int group2 = (MAC[14+len_a] << 8) | MAC[15+len_a];
			long int freq1t = 0;
			long int freq1r = 0;
			long int freq2t = 0;
			UNUSED(freq1r);

			fprintf (stderr, "\n Group Voice Channel Grant Update Multiple - Explicit");
			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc1, channelt1, channelr1, group1, group1);
			if (svc1 & 0x80) fprintf (stderr, " Emergency");
			if (svc1 & 0x40) fprintf (stderr, " Encrypted");
			if (opts->payload == 1) //hide behind payload due to len
			{
				if (svc1 & 0x20) fprintf (stderr, " Duplex");
				if (svc1 & 0x10) fprintf (stderr, " Packet");
				else fprintf (stderr, " Circuit");
				if (svc1 & 0x8) fprintf (stderr, " R"); //reserved bit is on
				fprintf (stderr, " Priority %d", svc1 & 0x7); //call priority
			}
			freq1t = process_channel_to_freq (opts, state, channelt1);
			if (channelr1 != 0 && channelr1 != 0xFFFF) freq1r = process_channel_to_freq (opts, state, channelr1);

			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc2, channelt2, channelr2, group2, group2);
			if (svc2 & 0x80) fprintf (stderr, " Emergency");
			if (svc2 & 0x40) fprintf (stderr, " Encrypted");
			if (opts->payload == 1) //hide behind payload due to len
			{
				if (svc2 & 0x20) fprintf (stderr, " Duplex");
				if (svc2 & 0x10) fprintf (stderr, " Packet");
				else fprintf (stderr, " Circuit");
				if (svc2 & 0x8) fprintf (stderr, " R"); //reserved bit is on
				fprintf (stderr, " Priority %d", svc2 & 0x7); //call priority
			}
			freq1t = process_channel_to_freq (opts, state, channelt2);
			if (channelr2 != 0 && channelr2 != 0xFFFF) freq1r = process_channel_to_freq (opts, state, channelr2);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; Ch: %04X TG: %d; ", channelt1, group1, channelt2, group2);
			state->last_active_time = time(NULL);

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

			//Skip tuning encrypted calls if enc calls are disabled
			if ( (svc1 & 0x40) && (svc2 & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

			int loop = 1;
			if (channelt1 == channelt2) loop = 1;
			else loop = 2;
			//assigned inside loop
			long int tunable_freq = 0;
			int tunable_chan = 0; 
			int tunable_group = 0;

			for (int j = 0; j < loop; j++)
			{
				//test svc opts for enc to tune or skip
				if (j == 0)
				{
					if ( (svc1 & 0x40) && opts->trunk_tune_enc_calls == 0) j++; //skip to next
				}
				if (j == 1)
				{
					if (( svc2 & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL; //skip to end
				}

				//assign our internal variables for check down on if to tune one freq/group or not
				if (j == 0)
				{
					tunable_freq = freq1t;
					tunable_chan = channelt1;
					tunable_group = group1;
				}
				else 
				{
					tunable_freq = freq2t;
					tunable_chan = channelt2;
					tunable_group = group2;
				}
				for (int i = 0; i < state->group_tally; i++)
				{
					if (state->group_array[i].groupNumber == tunable_group)
					{
						fprintf (stderr, " [%s]", state->group_array[i].groupName);
						strcpy (mode, state->group_array[i].groupMode);
						break;
					}
				}

				//TG hold on GRP_V Multi -- block non-matching group, allow matching group
				if (state->tg_hold != 0 && state->tg_hold != tunable_group) sprintf (mode, "%s", "B");
				if (state->tg_hold != 0 && state->tg_hold == tunable_group)
				{
					sprintf (mode, "%s", "A");
					opts->p25_is_tuned = 0; //unlock tuner
				}

				//tune if tuning available
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency
					{
						//changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
						if (1 == 1)
						{
							if (state->p25_chan_tdma[tunable_chan >> 12] == 1)
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;

								//shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
								//this will only occur in realtime tuning, not not required .bin or .wav playback
								// if (tunable_chan & 1) //VCH1
								// {
								// 	opts->slot1_on = 0;
								// 	opts->slot2_on = 1;
								// }
								// else //VCH0
								// {
								// 	opts->slot1_on = 1;
								// 	opts->slot2_on = 0;
								// }

							}
						}
						
						if (opts->use_rigctl == 1)
						{
							if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
							SetFreq(opts->rigctl_sockfd, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
						}
						//rtl
						else if (opts->audio_in_type == 3)
						{
							#ifdef USE_RTLSDR
							rtl_dev_tune (opts, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
							#endif
						}
					}    
				}
				if (opts->p25_trunk == 0)
				{
					if (tunable_group == state->lasttg || tunable_group == state->lasttgR)
					{
						//P1 FDMA
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = tunable_freq;
						//P2 TDMA
						else state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
					}
				}
			}
		}

		//Group Voice Channel Grant Update Multiple - Implicit
		if (MAC[1+len_a] == 0x05)
		{
			int so1 = MAC[2+len_a];
			int channel1  = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int group1 = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int so2 = MAC[7+len_a];
			int channel2  = (MAC[8+len_a] << 8) | MAC[9+len_a];
			int group2 = (MAC[10+len_a] << 8) | MAC[11+len_a];
			int so3 = MAC[12+len_a];
			int channel3  = (MAC[13+len_a] << 8) | MAC[14+len_a];
			int group3 = (MAC[15+len_a] << 8) | MAC[16+len_a];
			long int freq1 = 0;
			long int freq2 = 0;
			long int freq3 = 0;

			fprintf (stderr, "\n Group Voice Channel Grant Update Multiple - Implicit");
			fprintf (stderr, "\n  Channel 1 [%04X] Group 1 [%d][%04X]", channel1, group1, group1);
			if (so1 & 0x80) fprintf (stderr, " Emergency");
			if (so1 & 0x40) fprintf (stderr, " Encrypted");
			if (opts->payload == 1) //hide behind payload due to len
			{
				if (so1 & 0x20) fprintf (stderr, " Duplex");
				if (so1 & 0x10) fprintf (stderr, " Packet");
				else fprintf (stderr, " Circuit");
				if (so1 & 0x8) fprintf (stderr, " R"); //reserved bit is on
				fprintf (stderr, " Priority %d", so1 & 0x7); //call priority
			}
			freq1 = process_channel_to_freq (opts, state, channel1);

			if (channel2 != channel1 && channel2 != 0 && channel2 != 0xFFFF)
			{
				fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
				if (so2 & 0x80) fprintf (stderr, " Emergency");
				if (so2 & 0x40) fprintf (stderr, " Encrypted");
				if (opts->payload == 1) //hide behind payload due to len
				{
					if (so2 & 0x20) fprintf (stderr, " Duplex");
					if (so2 & 0x10) fprintf (stderr, " Packet");
					else fprintf (stderr, " Circuit");
					if (so2 & 0x8) fprintf (stderr, " R"); //reserved bit is on
					fprintf (stderr, " Priority %d", so2 & 0x7); //call priority
				}
				freq2 = process_channel_to_freq (opts, state, channel2);
			}

			if (channel3 != channel2 && channel3 != 0 && channel3 != 0xFFFF)
			{
				fprintf (stderr, "\n  Channel 3 [%04X] Group 3 [%d][%04X]", channel3, group3, group3);
				if (so3 & 0x80) fprintf (stderr, " Emergency");
				if (so3 & 0x40) fprintf (stderr, " Encrypted");
				if (opts->payload == 1) //hide behind payload due to len
				{
					if (so3 & 0x20) fprintf (stderr, " Duplex");
					if (so3 & 0x10) fprintf (stderr, " Packet");
					else fprintf (stderr, " Circuit");
					if (so3 & 0x8) fprintf (stderr, " R"); //reserved bit is on
					fprintf (stderr, " Priority %d", so3 & 0x7); //call priority
				}
				freq3 = process_channel_to_freq (opts, state, channel3);
			}

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; Ch: %04X TG: %d; Ch: %04X TG: %d; ", channel1, group1, channel2, group2, channel3, group3);

			//add active channel to string for ncurses display (multi check)
			// if (channel3 != channel2 && channel2 != channel1 && channel3 != 0 && channel3 != 0xFFFF)
			// 	sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; Ch: %04X TG: %d; Ch: %04X TG: %d; ", channel1, group1, channel2, group2, channel3, group3);

			state->last_active_time = time(NULL);

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

			//Skip tuning encrypted calls if enc calls are disabled
			if ( (so1 & 0x40) && (so2 & 0x40) && (so3 & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL; //this?

			int loop = 3;

			long int tunable_freq = 0;
			int tunable_chan = 0; 
			int tunable_group = 0;

			for (int j = 0; j < loop; j++)
			{
				//test svc opts for enc to tune or skip
				if (j == 0)
				{
					if ( (so1 & 0x40) && opts->trunk_tune_enc_calls == 0) j++; //skip to next
				}
				if (j == 1)
				{
					if ( (so2 & 0x40) && opts->trunk_tune_enc_calls == 0) j++; //skip to next
				}
				if (j == 2)
				{
					if ( (so3 & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL; //skip to end
				}

				//assign our internal variables for check down on if to tune one freq/group or not
				if (j == 0)
				{
					tunable_freq = freq1;
					tunable_chan = channel1;
					tunable_group = group1;
				}
				else if (j == 1)
				{
					tunable_freq = freq2;
					tunable_chan = channel2;
					tunable_group = group2;
				}
				else 
				{
					tunable_freq = freq3;
					tunable_chan = channel3;
					tunable_group = group3;
				}

				for (int i = 0; i < state->group_tally; i++)
				{
					if (state->group_array[i].groupNumber == tunable_group)
					{
						fprintf (stderr, " [%s]", state->group_array[i].groupName);
						strcpy (mode, state->group_array[i].groupMode);
						break;
					}
				}

				//TG hold on GRP_V Multi -- block non-matching group, allow matching group
				if (state->tg_hold != 0 && state->tg_hold != tunable_group) sprintf (mode, "%s", "B");
				if (state->tg_hold != 0 && state->tg_hold == tunable_group)
				{
					sprintf (mode, "%s", "A");
					opts->p25_is_tuned = 0; //unlock tuner
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
						if (1 == 1)
						{
							if (state->p25_chan_tdma[tunable_chan >> 12] == 1)
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;

								//shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
								//this will only occur in realtime tuning, not not required .bin or .wav playback
								// if (tunable_chan & 1) //VCH1
								// {
								// 	opts->slot1_on = 0;
								// 	opts->slot2_on = 1;
								// }
								// else //VCH0
								// {
								// 	opts->slot1_on = 1;
								// 	opts->slot2_on = 0;
								// }

							}
						}

						//rigctl
						if (opts->use_rigctl == 1)
						{
							if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
							SetFreq(opts->rigctl_sockfd, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
						}
						//rtl
						else if (opts->audio_in_type == 3)
						{
							#ifdef USE_RTLSDR
							rtl_dev_tune (opts, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
							#endif
						}
					}    
				}
				if (opts->p25_trunk == 0)
				{
					if (tunable_group == state->lasttg || tunable_group == state->lasttgR)
					{
						//P1 FDMA
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = tunable_freq;
						//P2 TDMA
						else state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
					}
				} 
			}
		}

		//Group Voice Channel Grant Update - Implicit
		if (MAC[1+len_a] == 0x42)
		{
			int channel1  = (MAC[2+len_a] << 8) | MAC[3+len_a];
			int group1 = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int channel2  = (MAC[6+len_a] << 8) | MAC[7+len_a];
			int group2 = (MAC[8+len_a] << 8) | MAC[9+len_a];
			long int freq1 = 0;
			long int freq2 = 0;

			fprintf (stderr, "\n Group Voice Channel Grant Update - Implicit");
			fprintf (stderr, "\n  Channel 1 [%04X] Group 1 [%d][%04X]", channel1, group1, group1);
			freq1 = process_channel_to_freq (opts, state, channel1);
			if (channel2 != channel1 && channel2 != 0 && channel2 != 0xFFFF)
			{
				fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
				freq2 = process_channel_to_freq (opts, state, channel2);
			}

			//add active channel to string for ncurses display
			if (channel2 != channel1 && channel2 != 0 && channel2 != 0xFFFF)
				sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; Ch: %04X TG: %d; ", channel1, group1, channel2, group2);
			else sprintf (state->active_channel[0], "Active Ch: %04X TG: %d; ", channel1, group1);
			state->last_active_time = time(NULL);

			//Skip tuning group calls if group calls are disabled
			if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

			//Skip tuning encrypted calls if enc calls are disabled -- abb formats do not carry svc bits :(
			// if (opts->trunk_tune_enc_calls == 0) goto SKIPCALL; //enable, or disable?

			int loop = 1;
			if (channel1 == channel2) loop = 1;
			else loop = 2;
			//assigned inside loop
			long int tunable_freq = 0;
			int tunable_chan = 0; 
			int tunable_group = 0;

			for (int j = 0; j < loop; j++)
			{
				//assign our internal variables for check down on if to tune one freq/group or not
				if (j == 0)
				{
					tunable_freq = freq1;
					tunable_chan = channel1;
					tunable_group = group1;
				}
				else 
				{
					tunable_freq = freq2;
					tunable_chan = channel2;
					tunable_group = group2;
				}

				for (int i = 0; i < state->group_tally; i++)
				{
					if (state->group_array[i].groupNumber == tunable_group)
					{
						fprintf (stderr, " [%s]", state->group_array[i].groupName);
						strcpy (mode, state->group_array[i].groupMode);
						break;
					}
				}

				//TG hold on GRP_V Multi -- block non-matching group, allow matching group
				if (state->tg_hold != 0 && state->tg_hold != tunable_group) sprintf (mode, "%s", "B");
				if (state->tg_hold != 0 && state->tg_hold == tunable_group)
				{
					sprintf (mode, "%s", "A");
					opts->p25_is_tuned = 0; //unlock tuner
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
						if (1 == 1)
						{
							if (state->p25_chan_tdma[tunable_chan >> 12] == 1)
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;

								//shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
								//this will only occur in realtime tuning, not not required .bin or .wav playback
								// if (tunable_chan & 1) //VCH1
								// {
								// 	opts->slot1_on = 0;
								// 	opts->slot2_on = 1;
								// }
								// else //VCH0
								// {
								// 	opts->slot1_on = 1;
								// 	opts->slot2_on = 0;
								// }

							}
						}

						//rigctl
						if (opts->use_rigctl == 1)
						{
							if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
							SetFreq(opts->rigctl_sockfd, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
						}
						//rtl
						else if (opts->audio_in_type == 3)
						{
							#ifdef USE_RTLSDR
							rtl_dev_tune (opts, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							state->last_vc_sync_time = time(NULL);
							j = 8; //break loop
							#endif
						}
					}    
				}
				if (opts->p25_trunk == 0)
				{
					if (tunable_group == state->lasttg || tunable_group == state->lasttgR)
					{
						//P1 FDMA
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = tunable_freq;
						//P2 TDMA
						else state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
					}
				}
			}
		}

		//Group Voice Channel Grant Update - Explicit
		if (MAC[1+len_a] == 0xC3)
		{
			int svc = MAC[2+len_a];
			int channelt  = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int channelr  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int group = (MAC[7+len_a] << 8) | MAC[8+len_a];
			long int freq1 = 0;
			long int freq2 = 0;
			UNUSED(freq2);

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
			fprintf (stderr, " Group Voice Channel Grant Update - Explicit");
			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc, channelt, channelr, group, group);
			freq1 = process_channel_to_freq (opts, state, channelt);
			if (channelr != 0 && channelr != 0xFFFF) freq2 = process_channel_to_freq (opts, state, channelr); //one system had this as channel 0xFFFF -- look up any particular meaning for that

			//don't set the tg here, one multiple grant updates, will mess up the TG value on current call
			// if (slot == 0)
			// {
			// 	state->lasttg = group;
			// }
			// else state->lasttgR = group;

			for (int i = 0; i < state->group_tally; i++)
			{
				if (state->group_array[i].groupNumber == group)
				{
					fprintf (stderr, " [%s]", state->group_array[i].groupName);
					strcpy (mode, state->group_array[i].groupMode);
					break;
				}
			}

			//TG hold on GRP_V Exp -- block non-matching group, allow matching group
			if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
			if (state->tg_hold != 0 && state->tg_hold == group)
			{
				sprintf (mode, "%s", "A");
				opts->p25_is_tuned = 0; //unlock tuner
			}

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
							// if (channelt & 1) //VCH1
							// {
							// 	opts->slot1_on = 0;
							// 	opts->slot2_on = 1;
							// }
							// else //VCH0
							// {
							// 	opts->slot1_on = 1;
							// 	opts->slot2_on = 0;
							// }
							
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
			if (opts->p25_trunk == 0)
			{
				if (group == state->lasttg || group == state->lasttgR)
				{
					//P1 FDMA
					if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq1;
					//P2 TDMA
					else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
				}
			}
		}

		//SNDCP Data Page Response -- ISP
		// if (MAC[1+len_a] == 0x53)
		// {
		// 	fprintf (stderr, "\n SNDCP Data Page Response ");
		// 	int dso = MAC[3+len_a];
		// 	int ans = MAC[4+len_a];
		// 	int dac = (MAC[5+len_a] << 8) | MAC[6+len_a];
		// 	int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
		// 	fprintf (stderr, "\n ANS: %02X; DSO: %02X DAC: %02X; Source: %d;", ans, dso, dac, source);
		// 	if (ans == 0x20)      fprintf (stderr, " - Proceed;");
		// 	else if (ans == 0x21) fprintf (stderr, " - Deny;");
		// 	else if (ans == 0x22) fprintf (stderr, " - Wait;");
		// 	else                  fprintf (stderr, " - Other;"); //unspecified
		// }

		//SNDCP Reconnect Request -- ISP
		// if (MAC[1+len_a] == 0x54)
		// {
		// 	fprintf (stderr, "\n SNDCP Reconnect Request ");
		// 	int dso = MAC[3+len_a];
		// 	int dac = (MAC[4+len_a] << 8) | MAC[5+len_a];
		// 	int ds  = (MAC[6+len_a] >> 7) & 1;
		// 	int res = MAC[6+len_a] & 7;
		// 	int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
		// 	fprintf (stderr, "\n DS: %d; DSO: %02X DAC: %02X; RES: %02X; Source: %d;", ds, dso, dac, res, source);
		// }

		//SNDCP Data Channel Grant
		if (MAC[1+len_a] == 0x54)
		{
			fprintf (stderr, "\n SNDCP Data Channel Grant - Explicit");
			int dso = MAC[2+len_a];
			int channelt = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int channelr = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int target   = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			fprintf (stderr, "\n  DSO: %02X; CHAN-T: %04X; CHAN-R: %04X; Target: %d;", dso, channelt, channelr, target);

			for (int i = 0; i < state->group_tally; i++)
			{
				if (state->group_array[i].groupNumber == target)
				{
					fprintf (stderr, " [%s]", state->group_array[i].groupName);
					strcpy (mode, state->group_array[i].groupMode);
					break;
				}
			}

			long int freq = process_channel_to_freq (opts, state, channelt);

			//add active channel to string for ncurses display
			sprintf (state->active_channel[0], "Active Data Ch: %04X TGT: %d; ", channelt, target);
			state->last_active_time = time(NULL);

			//Skip tuning data calls if data calls is disabled
			if (opts->trunk_tune_data_calls == 0) goto SKIPCALL;

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

						//because SNDCP data channels are Phase 1 channels, we will want to check to see if we need
						//to enable p1 frames and switch sample rate in reverse if on a TDMA-CC system
						//in the future, we may consider needing to do this if an SU causes the sytem to revert to P1 on a channel?
						else if (state->p25_chan_tdma[channelt >> 12] == 0 && state->p25_cc_is_tdma == 1)
						{
							state->samplesPerSymbol = 10;
							state->symbolCenter = 4;
							opts->frame_p25p1 = 1; //enable, just in case it isn't already

							//enable voice on slot 1 (just in case they start talking too, but probably won't)
							opts->slot1_on = 1;
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

		//SNDCP Data Page Request
		if (MAC[1+len_a] == 0x55)
		{
			fprintf (stderr, "\n SNDCP Data Page Request ");
			int dso = MAC[2+len_a];
			int dac = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int target = (MAC[5+len_a] << 16) | (MAC[6+len_a] << 8) | MAC[7+len_a];
			//P25p1 TSBK is shifted slightly on these two values
			if (state->synctype == 0 || state->synctype == 1)
			{
				dac = (MAC[5+len_a] << 8) | MAC[6+len_a];
				target = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			}
			fprintf (stderr, "\n  DSO: %02X; DAC: %02X; Target: %d;", dso, dac, target);
		}

		//SNDCP Data Channel Announcement
		if (MAC[1+len_a] == 0xD6)
		{
			fprintf (stderr, "\n SNDCP Data Channel Announcement ");
			int aa = (MAC[2+len_a] >> 7) & 1;
			int ra = (MAC[2+len_a] >> 6) & 1;
			int dso = MAC[2+len_a];
			int channelt = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int channelr = (MAC[6+len_a] << 8) | MAC[7+len_a];
			int dac = (MAC[8+len_a] << 8) | MAC[9+len_a];
			fprintf (stderr, "\n  AA: %d; RA: %d; DSO: %02X; DAC: %02X; CHAN-T: %04X; CHAN-R: %04X;", aa, ra, dso, dac, channelt, channelr);
			long int freq = 0; UNUSED(freq);
			if (channelt != 0)
				freq = process_channel_to_freq (opts, state, channelt);
			if (channelr != 0)
				freq = process_channel_to_freq (opts, state, channelt);

		}

		//MFID90 Group Regroup Add Command
		if (MAC[1+len_a] == 0x81 && MAC[2+len_a] == 0x90) //needs MAC message len update, may work same as explicit enc regroup?
		{
			fprintf (stderr, "\n MFID90 Group Regroup Add Command ");	
		}

		//System Service Broadcast
		if (MAC[1+len_a] == 0x78) 
		{
			int TWV = MAC[2+len_a]; //TWUID Validity
			int SSA = (MAC[3+len_a] << 16) | (MAC[4+len_a] << 8) | MAC[5+len_a];
			int SSS = (MAC[6+len_a] << 16) | (MAC[7+len_a] << 8) | MAC[8+len_a];
			int RPL = MAC[9+len_a];
			fprintf (stderr, "\n System Service Broadcast - Abbreviated \n");
			fprintf (stderr, "  TWV: %02X SSA: %06X; SSS: %06X; RPL: %02X", TWV, SSA, SSS, RPL);
		}

		//RFSS Status Broadcast - Implicit
		if (MAC[1+len_a] == 0x7A) 
		{
			int lra = MAC[2+len_a];
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channel = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sysclass = MAC[9+len_a];
			fprintf (stderr, "\n RFSS Status Broadcast - Implicit \n");
			fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%03d] SITE ID [%03d] CHAN [%04X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channel, sysclass);
			process_channel_to_freq (opts, state, channel);
			
			state->p2_siteid = siteid;
			state->p2_rfssid = rfssid;
		}

		//RFSS Status Broadcast - Explicit
		if (MAC[1+len_a] == 0xFA) 
		{
			int lra = MAC[2+len_a];
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int channelr = (MAC[9+len_a] << 8) | MAC[10+len_a];
			int sysclass = MAC[11+len_a];
			fprintf (stderr, "\n RFSS Status Broadcast - Explicit \n");
			fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%03d] SITE ID [%03d]\n  CHAN-T [%04X] CHAN-R [%02X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channelt, channelr, sysclass);
			process_channel_to_freq (opts, state, channelt);
			process_channel_to_freq (opts, state, channelr);

			state->p2_siteid = siteid;
			state->p2_rfssid = rfssid;
		}

		//Unit-to-Unit Answer Request (UU_ANS_REQ) -- ISP?
		// if (MAC[1+len_a] == 0x45 && MAC[2+len_a] != 0x90)
		// {
		// 	int svc     = MAC[2+len_a];
		// 	int ans  = MAC[3+len_a];
		// 	int target  = (MAC[4+len_a] << 16) | (MAC[5+len_a] << 8) | MAC[6+len_a];
		// 	int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];

		// 	fprintf (stderr, "\n Unit to Unit Channel Answer Request");
		// 	fprintf (stderr, "\n  SVC [%02X] Answer [%02X] Source [%d] Target [%d]", svc, ans, source, target);

		// 	if (ans == 0x20)      fprintf (stderr, " - Proceed;");
		// 	else if (ans == 0x21) fprintf (stderr, " - Deny;");
		// 	else if (ans == 0x22) fprintf (stderr, " - Wait;");
		// 	else                  fprintf (stderr, " - Other;"); //unspecified
			
		// }

		//TODO: Restructure this function to group standard Opcodes away from MFID 90 and MFID A4, and Unknown MFID
		//or just go to if-elseif-else layout (probably easier that way)
		//Harris A4 Opcodes
		if (MAC[1+len_a] != 0xB0 && MAC[2+len_a] == 0xA4)
		{
			// 6.2.36 Manufacturer Specific regarding octet 3 as len
			int len = MAC[3+len_a] & 0x3F;
			int res = MAC[3+len_a] >> 6;
			int k = 0;

			//storage info for storing to groupName, if not available
			char str[20]; char ttemp[20];
			int wr = 0, tsrc = 0, ttg = 0;
			if (slot == 0 && state->lastsrc != 0) tsrc = state->lastsrc;
			if (slot == 1 && state->lastsrcR != 0) tsrc = state->lastsrcR;
			if (slot == 0 && state->lasttg != 0) ttg = state->lasttg;
			if (slot == 1 && state->lasttgR != 0) ttg = state->lasttgR;

			//init str and ttemp as NULL zeroes
			for (i = 0; i < 20; i++)
			{
				str[i] = 0;
				ttemp[i] = 0;
			}

			//NOTE: slot is already checked
			
			//sanity check that we don't exceed the max MAC array size
			if (len > 24)
				len = 24; //should never exceed this len, but just in case it does

			//Harris "Talker" Alias -- Not always in the SACCH, has been seen in the FACCH as well (before MAC_PTT)
			if (MAC[1+len_a] == 0xA8) //1010 1000 (so, its opcode is 0x28, with the b = 0b10 value for manufacturer message?)
			{
				fprintf (stderr, "\n MFID A4 (Harris); VCH %d; TG: %d; SRC: %d; Talker Alias: ", slot, ttg, tsrc);
				for (i = 4; i <= len; i++) //len doesn't seem to include the CRC12
				{
					if ( (MAC[i+len_a] > 0x19) && (MAC[i+len_a] < 0x7F) )
						fprintf (stderr, "%c", (char)MAC[i+len_a]);
					else fprintf (stderr, " ");

					if ( (MAC[i+len_a] > 0x19) && (MAC[i+len_a] < 0x7F) )
						state->dmr_alias_block_segment[slot][0][k/4][k%4] = MAC[i+len_a];
					else state->dmr_alias_block_segment[slot][0][k/4][k%4] = 0x20;

					if ( (MAC[i+len_a] > 0x19) && (MAC[i+len_a] < 0x7F) )
						ttemp[k] = MAC[i+len_a];

					k++;
				}

				//assign completed talker to a more useful string instead
				snprintf (str, k+1, "%s", ttemp); //k+1, because we start at index k = 0;

				//see if we can fill in the groupName if we have a good src value that isn't 0
				//usually, we have TG values in the groupName field, but this is just for dumping
				//information for research, etc
				if (tsrc != 0)
				{
					for (int x = 0; x < state->group_tally; x++)
					{
						if (state->group_array[x].groupNumber == tsrc)
						{
							wr = 1; //already in there, so no need to assign it
							break;
						}
					}

					if (wr == 0) //not already in there, so save it there now
					{
						state->group_array[state->group_tally].groupNumber = tsrc;
						sprintf (state->group_array[state->group_tally].groupMode, "%s", "D");
						sprintf (state->group_array[state->group_tally].groupName, "%s", str);
						state->group_tally++;

						//if we have an opened group file, let's write what info we found into it
						if (opts->group_in_file[0] != 0) //file is available
						{
							FILE * pFile; //file pointer
							//open file by name that is supplied in the ncurses terminal, or cli
							pFile = fopen (opts->group_in_file, "a");
							// fprintf (pFile, "%d,D,%s,TG:%d,SYS:%03llX,RFSS:%lld,SITE:%lld\n", tsrc, str, ttg, state->p2_sysid, state->p2_rfssid, state->p2_siteid); //Cygwin Hates this for some reason
							fprintf (pFile, "%d,D,", tsrc);
							fprintf (pFile, "%s", str); //for whatever reason, Cygwin likes the string seperate (overflow on pFile?)
							fprintf (pFile, ",TG:%d,SYS:%03llX,RFSS:%lld,SITE:%lld\n", ttg, state->p2_sysid, state->p2_rfssid, state->p2_siteid);
							fclose (pFile);
						}

					}
				}

				//debug
				// fprintf (stderr, "\n WR: %d TG: %d SRC: %d Res: %d Len: %d STR: %s", wr, ttg, tsrc, res, len, str);
			}

			else
			{
				fprintf (stderr, "\n MFID A4 (Harris); Res: %d; Len: %d; Opcode: %02llX; ", res, len, MAC[1+len_a] & 0x3F); //first two bits are the b0 and b1 
				for (i = 4; i <= len; i++)
					fprintf (stderr, "%02llX", MAC[i+len_a]);
			}
			
		}

		//This is now confirmed to have the Harris Talker GPS, but the structure is unusual compared to other MFID messages, 
		//the A4 indicator is one octet more to the 'right' than is normative, so I cannot verify the len value (0x11, or 17).
		//Its possible the 0x80 is the 'Manufacturer Message' Opcode, which in the manual shows the rest of the
		//octets are not normative, and as such, can't say there is a len value (but should assume entire the SACCH/FACCH field)
		if (MAC[len_a+1] == 0x80 && MAC[len_a+2] != 0xA4 && MAC[len_a+2] != 0x90)
		{
			int unk1 = MAC[len_a+1]; //assuming this is the octet set for the 'manufacturer specific' message, may only be the MSBit
			int unk2 = MAC[len_a+2]; //This field is observed as 0xAA, unknown if this is an opcode, or other MFID
			int mfid = MAC[len_a+3]; //This is where the 0xA4 (Harris) Identifier is found in this message, as opposed to +2
			int len  = MAC[len_a+4]; //0x11 or 17 dec sounds reasonable, but cannot verify
			fprintf (stderr, "\n MFID %02X (Harris); Len: %d; Opcode: %02X/%02X;", mfid, len, unk1, unk2);

			//convert bytes to bits, may move this up top
			uint8_t mac_bits[24*8];
			memset (mac_bits, 0, sizeof(mac_bits));
			int l, x, z = 0;
			for (l = 0; l < 24; l++)
			{
				for (x = 0; x < 8; x++)
					mac_bits[z++] = (((uint8_t)MAC[l] << x) & 0x80) >> 7;
			}

			int tsrc = 0;
			if (slot == 0 && state->lastsrc != 0) tsrc = state->lastsrc;
			if (slot == 1 && state->lastsrcR != 0) tsrc = state->lastsrcR;

			// harris_gps (opts, state, slot, mac_bits); //fallback
			nmea_harris (opts, state, mac_bits+0, tsrc, slot); //new

			//debug - just dump payload
			// for (i = 0; i < 24; i++)
			// 		fprintf (stderr, " %02llX", MAC[i]);

			len_b = 17;

		}

		/*
		Maybe this is why some of these SYNC_BCST PDUs have zeroes in most fields

		Information provided in the SYNC_BCST message may be used for purposes other
		than providing synchronization to a TDMA voice channel. These purposes are beyond
		the scope of this document. SUs not interested in synchronization with the TDMA voice
		channel may ignore the status of the US bit.
		*/

		//Synchronization Broadcast (SYNC_BCST)
		if (MAC[1+len_a] == 0x70)
		{
			//NOTE: I've observed the minute value on Harris (Duke) does not work the same (rolls over every ~3 minutes)
			//as it does on a Moto system (actual minute value), may want to expose and check mm or mc bits (AABC-D Page 199-200)
			fprintf (stderr, "\n Synchronization Broadcast");
			int us  =   (MAC[3+len_a] >> 3) & 0x1; //synced or unsynced FDMA to TDMA
			int ist =   (MAC[3+len_a] >> 2) & 0x1; //IST bit tells us if time is realiable, synced to external source
			int mm  =   (MAC[3+len_a] >> 1) & 0x1; //Minute / Microslot Boundary Unlocked
			int mc  =   (((MAC[3+len_a] >> 0) & 0x1) << 1) + (((MAC[4+len_a] >> 7) & 0x1) << 0); //Minute Correction
			int vl  =   (MAC[4+len_a] >> 6) & 0x1; //Local Time Offset if Valid
			int ltoff = (MAC[4+len_a] & 0x3F);
			int year  = MAC[5+len_a] >> 1;
			int month = ((MAC[5+len_a] & 0x1) << 3) | (MAC[6+len_a] >> 5);
			int day   = (MAC[6+len_a] & 0x1F);
			int hour  = MAC[7+len_a] >> 3;
			int min   = ((MAC[7+len_a] & 0x7) << 3) | (MAC[8+len_a] >> 5);
			int slots = ((MAC[8+len_a] & 0x1F) << 8) | MAC[9+len_a];
			int sign = (ltoff & 0b100000) >> 5;
			float offhour = 0;

			if (opts->payload == 1)
			{
				fprintf (stderr, "\n");
				if (us)  fprintf (stderr, " Unsynchronized Slots;");
				if (ist) fprintf (stderr, " External System Time Sync;");
				if (mm)  fprintf (stderr, " Minute / Microslots Boundary Unlocked;"); //just a rolling counter
				if (mc)  fprintf (stderr, " Minute Correction: +%.01f ms;", (float)mc * 2.5f);
				if (vl)  fprintf (stderr, " Local Time Offset Valid;");
			}

			//calculate local time (on system) by looking at offset and subtracting 30 minutes increments, or divide by 2 for hourly
			if (sign == 1)
			{
				offhour = -( (ltoff & 0b11111 ) / 2);
			}
			else offhour = ( (ltoff & 0b11111 ) / 2);
			
			int seconds = slots / 135; //very rough estimation, but may be close enough for grins
			if (seconds > 59) seconds = 59; //sanity check for rounding error

			if (year != 0) //if time is synced in this PDU
			{
				fprintf (stderr, "\n  Date: 20%02d.%02d.%02d Time: %02d:%02d:%02d UTC", 
								year, month, day, hour, min, seconds);
				if (offhour != 0) //&& vl == 1
					fprintf (stderr, "\n  Local Time Offset: %.01f Hours;", offhour);
			}
			if (opts->payload == 1)
				fprintf (stderr, "\n US: %d; IST: %d; MM: %d; MC: %d; VL: %d; Sync Slots: %d; ", us, ist, mm, mc, vl, slots);
		}

		//identifier update VHF/UHF 
		if (MAC[1+len_a] == 0x74) 
		{
			state->p25_chan_iden = MAC[2+len_a] >> 4;
			int iden = state->p25_chan_iden;
			int bw_vu = (MAC[2+len_a] & 0xF);
			state->p25_trans_off[iden] = (MAC[3+len_a] << 6) | (MAC[4+len_a] >> 2);
			state->p25_chan_spac[iden] = ((MAC[4+len_a] & 0x3) << 8) | MAC[5+len_a];
			state->p25_base_freq[iden] = (MAC[6+len_a] << 24) | (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | (MAC[9+len_a] << 0);

			//this is causing more issues -- need a different way to check on this
			// if (state->p25_chan_spac[iden] == 0x64) //tdma
			// {
			// 	state->p25_chan_type[iden] = 4; //
			// 	state->p25_chan_tdma[iden] = 1; //
			// }
			// else //fdma
			// {
			// 	state->p25_chan_type[iden] = 1; //
			// 	state->p25_chan_tdma[iden] = 0; //
			// }

			state->p25_chan_type[iden] = 1; //set as old values for now
			state->p25_chan_tdma[iden] = 0; //set as old values for now

			fprintf (stderr, "\n Identifier Update UHF/VHF\n");
			fprintf (stderr, "  Channel Identifier [%01X] BW [%01X] Transmit Offset [%04X]\n  Channel Spacing [%03X] Base Frequency [%08lX] [%09ld]",
													state->p25_chan_iden, bw_vu, state->p25_trans_off[iden], state->p25_chan_spac[iden], state->p25_base_freq[iden], state->p25_base_freq[iden] * 5);
		}

		//identifier update (Non-TDMA 6.2.22) (Non-VHF-UHF) //with signed offset, bit trans_off >> 8; bit number 9
		if (MAC[1+len_a] == 0x7D)
		{
			state->p25_chan_iden = MAC[2+len_a] >> 4;
			int iden = state->p25_chan_iden;

			state->p25_chan_type[iden] = 1;
			state->p25_chan_tdma[iden] = 0;
			int bw = ((MAC[2+len_a] & 0xF) << 5) | ((MAC[3+len_a] & 0xF8) >> 2);
			state->p25_trans_off[iden] = (MAC[3+len_a] << 6) | (MAC[4+len_a] >> 2);
			state->p25_chan_spac[iden] = ((MAC[4+len_a] & 0x3) << 8) | MAC[5+len_a];
			state->p25_base_freq[iden] = (MAC[6+len_a] << 24) | (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | (MAC[9+len_a] << 0);

			fprintf (stderr, "\n Identifier Update (8.3.1.23)\n");
			fprintf (stderr, "  Channel Identifier [%01X] BW [%01X] Transmit Offset [%04X]\n  Channel Spacing [%03X] Base Frequency [%08lX] [%09ld]",
													state->p25_chan_iden, bw, state->p25_trans_off[iden], state->p25_chan_spac[iden], state->p25_base_freq[iden], state->p25_base_freq[iden] * 5);
		}

		//identifier update for TDMA, Abbreviated
		if (MAC[1+len_a] == 0x73)
		{
			state->p25_chan_iden = MAC[2+len_a] >> 4;
			int iden = state->p25_chan_iden;
			state->p25_chan_tdma[iden] = 1;
			state->p25_chan_type[iden] = MAC[2+len_a] & 0xF;
			state->p25_trans_off[iden] = (MAC[3+len_a] << 6) | (MAC[4+len_a] >> 2);
			state->p25_chan_spac[iden] = ((MAC[4+len_a] & 0x3) << 8) | MAC[5+len_a];
			state->p25_base_freq[iden] = (MAC[6+len_a] << 24) | (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | (MAC[9+len_a] << 0);

			fprintf (stderr, "\n Identifier Update for TDMA - Abbreviated\n");
			fprintf (stderr, "  Channel Identifier [%01X] Channel Type [%01X] Transmit Offset [%04X]\n  Channel Spacing [%03X] Base Frequency [%08lX] [%09ld]",
													state->p25_chan_iden, state->p25_chan_type[iden], state->p25_trans_off[iden], state->p25_chan_spac[iden], state->p25_base_freq[iden], state->p25_base_freq[iden] * 5);
		}

		//identifier update for TDMA, Extended
		if (MAC[1+len_a] == 0xF3)
		{
			state->p25_chan_iden = MAC[3+len_a] >> 4;
			int iden = state->p25_chan_iden;
			state->p25_chan_tdma[iden] = 1;
			state->p25_chan_type[iden] = MAC[3+len_a] & 0xF;
			state->p25_trans_off[iden] = (MAC[4+len_a] << 6) | (MAC[5+len_a] >> 2);
			state->p25_chan_spac[iden] = ((MAC[5+len_a] & 0x3) << 8) | MAC[6+len_a];
			state->p25_base_freq[iden] = (MAC[7+len_a] << 24) | (MAC[8+len_a] << 16) | (MAC[9+len_a] << 8) | (MAC[10+len_a] << 0);
			int lwacn  = (MAC[11+len_a] << 12) | (MAC[12+len_a] << 4) | ((MAC[13+len_a] & 0xF0) >> 4); 
			int lsysid = ((MAC[13+len_a] & 0xF) << 8) | MAC[14+len_a];

			fprintf (stderr, "\n Identifier Update for TDMA - Extended\n");
			fprintf (stderr, "  Channel Identifier [%01X] Channel Type [%01X] Transmit Offset [%04X]\n  Channel Spacing [%03X] Base Frequency [%08lX] [%09ld]",
													state->p25_chan_iden, state->p25_chan_type[iden], state->p25_trans_off[iden], state->p25_chan_spac[iden], state->p25_base_freq[iden], state->p25_base_freq[iden] * 5);
			fprintf (stderr, "\n  WACN [%04X] SYSID [%04X]", lwacn, lsysid);
		}

		//Secondary Control Channel Broadcast, Explicit
		if (MAC[1+len_a] == 0xE9)
		{
			int rfssid = MAC[2+len_a];
			int siteid = MAC[3+len_a];
			int channelt = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int channelr = (MAC[6+len_a] << 8) | MAC[7+len_a];
			int sysclass = MAC[8+len_a];

			if (1 == 1) //state->p2_is_lcch == 1
			{

				fprintf (stderr, "\n Secondary Control Channel Broadcast - Explicit\n");
				fprintf (stderr, "  RFSS [%03d] SITE ID [%03d] CHAN-T [%04X] CHAN-R [%04X] SSC [%02X]", rfssid, siteid, channelt, channelr, sysclass);

				process_channel_to_freq (opts, state, channelt);
				process_channel_to_freq (opts, state, channelr);
			}

			state->p2_siteid = siteid;
			state->p2_rfssid = rfssid;

		}

		//Secondary Control Channel Broadcast, Implicit
		if (MAC[1+len_a] == 0x79)
		{
			int rfssid = MAC[2+len_a];
			int siteid = MAC[3+len_a];
			int channel1 = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int sysclass1 = MAC[6+len_a];
			int channel2 = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sysclass2 = MAC[9+len_a];
			long int freq1 = 0;
			long int freq2 = 0;
			if (1 == 1) //state->p2_is_lcch == 1
			{

				fprintf (stderr, "\n Secondary Control Channel Broadcast - Implicit\n");
				fprintf (stderr, "  RFSS[%03d] SITE ID [%03d] CHAN1 [%04X] SSC [%02X] CHAN2 [%04X] SSC [%02X]", rfssid, siteid, channel1, sysclass1, channel2, sysclass2);

				freq1 = process_channel_to_freq (opts, state, channel1);
				freq2 = process_channel_to_freq (opts, state, channel2);
			}

			//place the cc freq into the list at index 0 if 0 is empty so we can hunt for rotating CCs without user LCN list
			if (state->trunk_lcn_freq[1] == 0)
			{
				state->trunk_lcn_freq[1] = freq1; 
				state->trunk_lcn_freq[2] = freq2;
				state->lcn_freq_count = 3; //increment to three
			}

			state->p2_siteid = siteid;
			state->p2_rfssid = rfssid; 

		}

		//MFID90 Group Regroup Voice Channel User - Abbreviated
		if (MAC[1+len_a] == 0x80 && MAC[2+len_a] == 0x90)
		{

			int gr  = (MAC[4+len_a] << 8) | MAC[5+len_a];
			int src = (MAC[6+len_a] << 16) | (MAC[7+len_a] << 8) | MAC[8+len_a];
			fprintf (stderr, "\n VCH %d - Super Group %d SRC %d ", slot, gr, src);
			fprintf (stderr, "MFID90 Group Regroup Voice");

			if (slot == 0)
			{
				state->lasttg = gr;
				if (src != 0) state->lastsrc = src;
			}
			else
			{
				state->lasttgR = gr;
				if (src != 0) state->lastsrcR = src;
			}
		}
		//MFID90 Group Regroup Voice Channel User - Extended
		if (MAC[1+len_a] == 0xA0 && MAC[2+len_a] == 0x90)
		{

			int gr  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int src = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			fprintf (stderr, "\n VCH %d - Super Group %d SRC %d ", slot, gr, src);
			fprintf (stderr, "MFID90 Group Regroup Voice");

			if (slot == 0)
			{
				state->lasttg = gr;
				if (src != 0) state->lastsrc = src;
			}
			else
			{
				state->lasttgR = gr;
				if (src != 0) state->lastsrcR = src;
			}
		}

		//MFIDA4 Group Regroup Explicit Encryption Command
		if (MAC[1+len_a] == 0xB0 && MAC[2+len_a] == 0xA4) //&& MAC[2+len_a] == 0xA4
		{
			int len_grg = MAC[3+len_a] & 0x3F; //MFID Len in Octets
			int tga = MAC[4+len_a] >> 5; //3 bit TGA values from GRG_Options
			int ssn = MAC[4+len_a] & 0x1F; //5 bit SSN from from GRG_Options

			fprintf (stderr, "\n MFID A4 (Harris) Group Regroup Explicit Encryption Command\n");
			// if (len_grg) fprintf (stderr, " Len: %02d", len_grg); //debug
			if ( (tga & 4) == 4) fprintf (stderr, " Simulselect"); //one-way regroup
			else fprintf (stderr, " Patch"); //two-way regroup
			if (tga & 1) fprintf (stderr, " Active;"); //activated
			else fprintf (stderr, " Inactive;"); //deactivated

			//debug
			// fprintf (stderr, " T:%d G:%d A:%d;", (tga >> 2) & 1, (tga >> 1) & 1, tga & 1);

			fprintf (stderr, " SSN: %02d;", ssn);

			if ( (tga & 0x2) == 2) //group WGID to supergroup
			{ 
				int sg =  (MAC[5+len_a] << 8) | MAC[6+len_a];
				int key = (MAC[7+len_a] << 8) | MAC[8+len_a];
				int alg = MAC[9+len_a];
				int t1 = (MAC[10+len_a] << 8) | MAC[11+len_a];
				int t2 = (MAC[12+len_a] << 8) | MAC[13+len_a];
				int t3 = (MAC[14+len_a] << 8) | MAC[15+len_a];
				int t4 = (MAC[16+len_a] << 8) | MAC[17+len_a];
				UNUSED4(t1, t2, t3, t4);
				fprintf (stderr, " SG: %d; KEY: %04X; ALG: %02X;\n  ", sg, key, alg);
				int a = 0;
				int wgid = 0;

				//sanity check
				// if ((len_grg + len_a > 20)) len_grg = 20 - len_a;

				for (int i = 10; i <= len_grg;)
				{
					//failsafe to prevent oob array
					if ( (i + len_a) > 20)
					{
						goto END_PDU;
					}
					wgid = (MAC[10+len_a+a] << 8) | MAC[11+len_a+a];
					fprintf (stderr, "WGID: %d; ", wgid);
					a = a + 2;
					i = i + 2;
				}

			}

			else if ( (tga & 0x2) == 0) //individual WUID to supergroup
			{
				int sg =  (MAC[5+len_a] << 8) | MAC[6+len_a];
				int key = (MAC[7+len_a] << 8) | MAC[8+len_a];
				int t1 = (MAC[9+len_a] << 16) | (MAC[10+len_a] << 8) | MAC[11+len_a];
				int t2 = (MAC[12+len_a] << 16) | (MAC[13+len_a] << 8) | MAC[14+len_a];
				int t3 = (MAC[15+len_a] << 16) | (MAC[16+len_a] << 8) | MAC[17+len_a];
				fprintf (stderr, "  SG: %d KEY: %04X", sg, key);
				fprintf (stderr, " WUID: %d; WUID: %d; WUID: %d; ", t1, t2, t3);
			}

		}

		//Unit Registration Response -- Extended vPDU (MBT will not make it here)
		if ( MAC[1+len_a] == 0xEC && MAC[0] != 0x07)
		{
			int res = (MAC[3+len_a] >> 2) & 0x3F;
			int RV = (MAC[2+len_a] >> 0) & 0x3;
			int src = (MAC[8+len_a] << 16) | (MAC[9+len_a] << 8) | MAC[10+len_a];
			int uwacn = (MAC[4+len_a] << 12) | (MAC[5+len_a] << 4) | ((MAC[6+len_a] & 0xF0) >> 4); 
			int usys = ((MAC[6+len_a] & 0xF) << 8) | MAC[7+len_a];
			fprintf (stderr, "\n Unit Registration Response - WACN: %05X; SYS: %03X; SRC: %d", uwacn, usys, src);
			if (res) fprintf (stderr, " RES: %d;", res);
			if (RV == 0) fprintf (stderr, " REG_ACCEPT;");
			if (RV == 1) fprintf (stderr, " REG_FAIL;"); //RFSS was unable to verify
			if (RV == 2) fprintf (stderr, " REG_DENY;"); //Not allowed at this location
			if (RV == 3) fprintf (stderr, " REG_REFUSED;"); //WUID invalid but re-register after a user stimulus
			fprintf (stderr, " - Extended;");
		}

		//Unit Registration Response -- Abbreviated TSBK and vPDU
		if (MAC[1+len_a] == 0x6C)
		{
			/*
			Unit Registration Response TSBK
			P25 PDU Payload
				[07][6C][01][EC][72][70][EC][72][70][EC][00][00]
				[00][00][00][00][00][00][00][00][00][00][00][00]
			*/
			int k = 1; //vPDU is one octet higher than TSBK
			if (MAC[len_a] == 0x07) k = 0;
			int res = (MAC[2+len_a+k] >> 6) & 0x3;
			int RV = (MAC[2+len_a+k] >> 4) & 0x3;
			int usite = ((MAC[2+len_a+k] & 0xF) << 8) | MAC[3+len_a+k];
			int sid = (MAC[4+len_a+k] << 16) | (MAC[5+len_a+k] << 8) | MAC[6+len_a+k];
			int src = (MAC[7+len_a+k] << 16) | (MAC[8+len_a+k] << 8) | MAC[9+len_a+k];
			fprintf (stderr, "\n Unit Registration Response - SITE: %03X SRC_ID: %d SRC: %d", usite, sid, src );
			if (res) fprintf (stderr, " RES: %d;", res);
			if (RV == 0) fprintf (stderr, " REG_ACCEPT;");
			if (RV == 1) fprintf (stderr, " REG_FAIL;"); //RFSS was unable to verify
			if (RV == 2) fprintf (stderr, " REG_DENY;"); //Not allowed at this location
			if (RV == 3) fprintf (stderr, " REG_REFUSED;"); //WUID invalid but re-register after a user stimulus

		}

		//Unit Registration Command -- TSBK and vPDU
		if (MAC[1+len_a] == 0x6D && MAC[0+len_a] != 0x07)
		{
			int k = 2; //vPDU is two octets higher than TSBK
			if (MAC[len_a] == 0x07) k = 0;
			int src = (MAC[2+len_a+k] << 16) | (MAC[3+len_a+k] << 8) | MAC[4+len_a+k];
			int tgt = (MAC[5+len_a+k] << 16) | (MAC[6+len_a+k] << 8) | MAC[7+len_a+k];
			fprintf (stderr, "\n Unit Registration - SRC: %d; TGT: %d;", src, tgt);
		}

		//Unit Deregistration Acknowlegement (TSBK and vPDU are the same)
		if (MAC[1+len_a] == 0x6F)
		{
			int src = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			int uwacn = (MAC[3+len_a] << 12) | (MAC[4+len_a] << 4) | ((MAC[5+len_a] & 0xF0) >> 4); 
			int usys = ((MAC[5+len_a] & 0xF) << 8) | MAC[6+len_a];
			fprintf (stderr, "\n Unit Deregistration Acknowlegement - WACN: %05X; SYS: %03X; SRC: %d", uwacn, usys, src);
			if (MAC[1+len_a] == 0xEF) fprintf (stderr, " - Extended;");
		}

		//Authentication Demand
		if (MAC[1+len_a] == 0x71 || MAC[1+len_a] == 0xF1)
		{
			fprintf (stderr, "\n Authentication Demand;");
			if (MAC[1+len_a] == 0xF1) fprintf (stderr, " - Extended;");
		}

		//Authentication FNE Response
		if (MAC[1+len_a] == 0x72 || MAC[1+len_a] == 0xF2)
		{
			fprintf (stderr, "\n Authentication FNE Response;");
			if (MAC[1+len_a] == 0xF2) fprintf (stderr, " - Extended;");
		}

		//MAC_Release for Forced/Unforced Audio or Call Preemption vPDU
		if (MAC[1+len_a] == 0x31)
		{
			int uf = (MAC[2+len_a] >> 7) & 1;
			int ca = (MAC[2+len_a] >> 6) & 1;
			int resr1 = MAC[2+len_a] & 0x1F;
			int add = (MAC[3+len_a] << 16) | (MAC[4+len_a] << 8) | MAC[5+len_a];
			int resr2 = MAC[6+len_a] >> 4;
			int cc = ( (MAC[6+len_a] & 0xF) << 8) | MAC[7+len_a];

			fprintf (stderr, "\n MAC Release:  ");
			if(uf) fprintf (stderr, "Forced; ");
			else fprintf (stderr, "Unforced; ");
			if(ca) fprintf (stderr, "Audio Preemption; ");
			else fprintf (stderr, "Call Preemption; ");
			fprintf (stderr, "RES1: %d; ", resr1);
			fprintf (stderr, "RES2: %d; ", resr2);
			fprintf (stderr, "TGT: %d; ", add);
			fprintf (stderr, "CC: %03X; ", cc);

			// if (slot == 0) state->lastsrc = 0;
			// if (slot == 1) state->lastsrcR = 0;
		}

		//1 or 21, group voice channel message, abb and ext
		if (MAC[1+len_a] == 0x1 || MAC[1+len_a] == 0x21)
		{
			int svc =  MAC[2+len_a];
			int gr  = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int src = (MAC[5+len_a] << 16) | (MAC[6+len_a] << 8) | MAC[7+len_a];
			unsigned long long int src_suid = 0;

			if (MAC[1+len_a] == 0x21)
			{
				src_suid = (MAC[8+len_a] << 48ULL) | (MAC[9+len_a] << 40ULL) | (MAC[10+len_a] << 32ULL) | (MAC[11+len_a] << 24ULL) |
										(MAC[12+len_a] << 16ULL) | (MAC[13+len_a] << 8ULL) | (MAC[14+len_a] << 0ULL);

				src = src_suid & 0xFFFFFF; //last 24 of completed SUID?
			}

			fprintf (stderr, "\n VCH %d - TG: %d; SRC: %d; ", slot, gr, src);

			if (MAC[1+len_a] == 0x21) fprintf (stderr, "SUID: %08llX-%08d; ", src_suid >> 24, src);

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

			fprintf (stderr, " Group Voice");

			sprintf (state->call_string[slot], "   Group ");
			if (svc & 0x80) strcat (state->call_string[slot], " Emergency  ");
			else if (svc & 0x40) strcat (state->call_string[slot], " Encrypted  ");
			else strcat (state->call_string[slot], "            ");

			if (MAC[1+len_a] == 0x21) fprintf (stderr, " - Extended ");
			else fprintf (stderr, " - Abbreviated ");

			if (slot == 0)
			{
				state->lasttg = gr;
				if (src != 0) state->lastsrc = src;
			}
			else
			{
				state->lasttgR = gr;
				if (src != 0) state->lastsrcR = src;
			}
		}

		//1 or 21, unit to unit voice channel message, abb and ext
		if (MAC[1+len_a] == 0x2 || MAC[1+len_a] == 0x22)
		{
			int svc =  MAC[2+len_a];
			int gr  = (MAC[3+len_a] << 16) | (MAC[4+len_a] << 8) | MAC[5+len_a];
			int src = (MAC[6+len_a] << 16) | (MAC[7+len_a] << 8) | MAC[8+len_a];
			unsigned long long int src_suid = 0;

			if (MAC[1+len_a] == 0x21)
			{
				src_suid = (MAC[9+len_a] << 48ULL) | (MAC[10+len_a] << 40ULL) | (MAC[11+len_a] << 32ULL) | (MAC[12+len_a] << 24ULL) |
										(MAC[13+len_a] << 16ULL) | (MAC[14+len_a] << 8ULL) | (MAC[15+len_a] << 0ULL);

				src = src_suid & 0xFFFFFF;
			}

			fprintf (stderr, "\n VCH %d - TGT: %d; SRC %d; ", slot, gr, src);

			if (MAC[1+len_a] == 0x22) fprintf (stderr, "SUID: %08llX-%08d; ", src_suid >> 24, src);

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

			fprintf (stderr, " Unit to Unit Voice");

			sprintf (state->call_string[slot], " Private ");
			if (svc & 0x80) strcat (state->call_string[slot], " Emergency  ");
			else if (svc & 0x40) strcat (state->call_string[slot], " Encrypted  ");
			else strcat (state->call_string[slot], "            ");

			if (slot == 0)
			{
				state->lasttg = gr;
				if (src != 0) state->lastsrc = src;
			}
			else
			{
				state->lasttgR = gr;
				if (src != 0) state->lastsrcR = src;
			}
		}

		

		//network status broadcast, abbreviated
		if (MAC[1+len_a] == 0x7B) 
		{
			int lra = MAC[2+len_a];
			int lwacn  = (MAC[3+len_a] << 12) | (MAC[4+len_a] << 4) | ((MAC[5+len_a] & 0xF0) >> 4);
			int lsysid = ((MAC[5+len_a] & 0xF) << 8) | MAC[6+len_a];
			int channel = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sysclass = MAC[9+len_a];
			int lcolorcode = ((MAC[10+len_a] & 0xF) << 8) | MAC[11+len_a];
			UNUSED(sysclass);
			fprintf (stderr, "\n Network Status Broadcast - Abbreviated \n");
			fprintf (stderr, "  LRA [%02X] WACN [%05X] SYSID [%03X] NAC [%03X] CHAN-T [%04X]", lra, lwacn, lsysid, lcolorcode, channel);
			state->p25_cc_freq = process_channel_to_freq (opts, state, channel);
			state->p25_cc_is_tdma = 1; //flag on for CC tuning purposes when system is qpsk
			if (state->p2_hardset == 0 ) //state->p2_is_lcch == 1 shim until CRC is working, prevent bogus data
			{
				state->p2_wacn = lwacn;
				state->p2_sysid = lsysid;
				state->p2_cc = lcolorcode;
			}

			//place the cc freq into the list at index 0 if 0 is empty, or not the same, 
			//so we can hunt for rotating CCs without user LCN list
			if (state->trunk_lcn_freq[0] == 0 || state->trunk_lcn_freq[0] != state->p25_cc_freq)
			{
				state->trunk_lcn_freq[0] = state->p25_cc_freq; 
			} 

		}
		//network status broadcast, extended
		if (MAC[1+len_a] == 0xFB)
		{
			int lra = MAC[2+len_a];
			int lwacn  = (MAC[3+len_a] << 12) | (MAC[4+len_a] << 4) | ((MAC[5+len_a] & 0xF0) >> 4);
			int lsysid = ((MAC[5+len_a] & 0xF) << 8) | MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int channelr = (MAC[9+len_a] << 8) | MAC[10+len_a];
			int sysclass = MAC[9+len_a];
			int lcolorcode = ((MAC[12+len_a] & 0xF) << 8) | MAC[13+len_a];
			UNUSED(sysclass);
			fprintf (stderr, "\n Network Status Broadcast - Extended \n");
			fprintf (stderr, "  LRA [%02X] WACN [%05X] SYSID [%03X] NAC [%03X] CHAN-T [%04X] CHAN-R [%04X]", lra, lwacn, lsysid, lcolorcode, channelt, channelr);
			state->p25_cc_freq = process_channel_to_freq (opts, state, channelt);
			process_channel_to_freq (opts, state, channelr);
			state->p25_cc_is_tdma = 1; //flag on for CC tuning purposes when system is qpsk
			if (state->p2_hardset == 0 ) //state->p2_is_lcch == 1 shim until CRC is working, prevent bogus data
			{
				state->p2_wacn = lwacn;
				state->p2_sysid = lsysid;
				state->p2_cc = lcolorcode;
			}

		}

		//Adjacent Status Broadcast, abbreviated
		if (MAC[1+len_a] == 0x7C) 
		{
			int lra = MAC[2+len_a];
			int cfva = MAC[3+len_a] >> 4;
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sysclass = MAC[9+len_a];
			fprintf (stderr, "\n Adjacent Status Broadcast - Abbreviated\n");
			fprintf (stderr, "  LRA [%02X] RFSS[%03d] SITE [%03d] SYSID [%03X] CHAN-T [%04X] SSC [%02X]\n  ", lra, rfssid, siteid, lsysid, channelt, sysclass);
			if (cfva & 0x8) fprintf (stderr, " Conventional");
			if (cfva & 0x4) fprintf (stderr, " Failure Condition");
			if (cfva & 0x2) fprintf (stderr, " Up to Date (Correct)");
			else fprintf (stderr, " Last Known");
			if (cfva & 0x1) fprintf (stderr, " Valid RFSS Connection Active");
			process_channel_to_freq (opts, state, channelt);


		}

		//Adjacent Status Broadcast, extended
		if (MAC[1+len_a] == 0xFC) 
		{
			int lra = MAC[2+len_a];
			int cfva = MAC[3+len_a] >> 4; 
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int channelr = (MAC[9+len_a] << 8) | MAC[10+len_a];
			int sysclass = MAC[11+len_a];  //need to re-check this 
			fprintf (stderr, "\n Adjacent Status Broadcast - Extended\n");
			fprintf (stderr, "  LRA [%02X] RFSS[%03d] SITE [%03d] SYSID [%03X] CHAN-T [%04X] CHAN-R [%04X] SSC [%02X]\n  ", lra, rfssid, siteid, lsysid, channelt, channelr, sysclass);
			if (cfva & 0x8) fprintf (stderr, " Conventional");
			if (cfva & 0x4) fprintf (stderr, " Failure Condition");
			if (cfva & 0x2) fprintf (stderr, " Up to Date (Correct)");
			else fprintf (stderr, " Last Known");
			if (cfva & 0x1) fprintf (stderr, " Valid RFSS Connection Active");
			process_channel_to_freq (opts, state, channelt);
			process_channel_to_freq (opts, state, channelr);


		}

		SKIPCALL: ; //do nothing

		if ( (len_b + len_c) < 24 && len_c != 0) 
		{
			len_a = len_b;
		}
		else
		{
			goto END_PDU;
		}

	}

	END_PDU:
	state->p2_is_lcch = 0; 
	//debug printing
	if (opts->payload == 1 && MAC[1] != 0) //print only if not a null type //&& MAC[1] != 0 //&& MAC[2] != 0
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, "\n P25 PDU Payload\n  ");
		for (int i = 0; i < 24; i++)
		{
			fprintf (stderr, "[%02llX]", MAC[i]);
			if (i == 11) fprintf (stderr, "\n  ");
		}
		fprintf (stderr, "%s", KNRM);
	}

}
