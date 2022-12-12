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
	 8,  0,  0,  7, 11,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //8F
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //9F
	16,  0,  0, 11, 13, 11, 11, 11, 10,  0,  0,  0,  0,  0,  0,  0, //AF
	 17,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //BF, b0 was 0, set to 17
	11,  0,  0,  8, 15, 12, 15, 32, 12, 12,  0, 27, 14, 29, 29, 32, //CF
	 0,  0,  0,  0,  0,  0,  9,  0, 14, 29, 11, 27, 14,  0, 40, 11, //DF 
	28,  0,  0, 14, 17, 14,  0,  0, 16,  8, 11,  0, 13, 19,  0,  0, //EF
	 0,  0, 16, 14,  0,  0, 12,  0, 22,  0, 11, 13, 11,  0, 15,  0 }; //FF


//MAC PDU 3-bit Opcodes BBAC (8.4.1) p 123:
//0 - reserved //1 - Mac PTT //2 - Mac End PTT //3 - Mac Idle //4 - Mac Active
//5 - reserved //6 - Mac Hangtime //7 - reserved //Mac PTT BBAC p80

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
		if (slot == 1) state->dmrburstL = 30;
		else state->dmrburstR = 30;
	}


	if (len_b == 0 || len_b > 18) 
	{
		goto END_PDU;
	}

	//group list mode so we can look and see if we need to block tuning any groups, etc
	char mode[8]; //allow, block, digital, enc, etc

	for (int i = 0; i < 2; i++) 
	{

		//MFID90 Voice Grants, A3, A4, and A5
		//MFID90 Group Regroup Channel Grant - Implicit
		if (MAC[1+len_a] == 0xA3 && MAC[2+len_a] == 0x90)
		{
			int mfid = MAC[2+len_a];
			int channel  = (MAC[5+len_a] << 8) | MAC[6+len_a];
			int sgroup = (MAC[7+len_a] << 8) | MAC[8+len_a];
			long int freq = 0;
			fprintf (stderr, "\n MFID90 Group Regroup Channel Grant - Implicit");
			fprintf (stderr, "\n  CHAN [%04X] Group [%d][%04X]", channel, sgroup, sgroup);
			freq = process_channel_to_freq (opts, state, channel);

			for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == sgroup)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
        }
      }

			//tune if tuning available
			if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
  		{
				//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
    		if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
    		{
					//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
					if (opts->mod_qpsk == 1)
					{
						int spacing = state->p25_chan_spac[channel >> 12];
						if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
						{
							state->samplesPerSymbol = 8;
							state->symbolCenter = 3;
						}	
					}
					//rigctl
          if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      			SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
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
			fprintf (stderr, "\n MFID90 Group Regroup Channel Grant - Explicit");
			fprintf (stderr, "\n  CHAN [%04X] Group [%d][%04X]", channel, sgroup, sgroup);
			freq = process_channel_to_freq (opts, state, channel);

			for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == sgroup)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
        }
      }

			//tune if tuning available
			if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
  		{
				//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
    		if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
    		{
					//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
					if (opts->mod_qpsk == 1)
					{
						int spacing = state->p25_chan_spac[channel >> 12];
						if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
						{
							state->samplesPerSymbol = 8;
							state->symbolCenter = 3;
						}	
					}
					
          if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      			SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
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
			fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
			freq2 = process_channel_to_freq (opts, state, channel2);

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
					}
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
						if (opts->mod_qpsk == 1) 
						{
							int spacing = state->p25_chan_spac[tunable_chan >> 12];
							if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;
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
							j = 8; //break loop
							
						}
						//rtl_udp
						else if (opts->audio_in_type == 3)
						{
							rtl_udp_tune (opts, state, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							j = 8; //break loop
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

			fprintf (stderr, "\n Group Voice Channel Grant Update");
			fprintf (stderr, "\n  SVC [%02X] CHAN [%04X] Group [%d] Source [%d]", svc, channel, group, source);
			freq = process_channel_to_freq (opts, state, channel);

			for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == group)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
        }
      }

			//tune if tuning available
			if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
  		{
				//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
    		if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
    		{
					//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
					if (opts->mod_qpsk == 1)
					{
						int spacing = state->p25_chan_spac[channel >> 12];
						if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
						{
							state->samplesPerSymbol = 8;
							state->symbolCenter = 3;
						}	
					}
					//rigctl
          if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      			SetFreq(opts->rigctl_sockfd, freq);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
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

		//Unit-to-Unit Voice Service Channel Grant (UU_V_CH_GRANT), or Grant Update (same format)
		if (MAC[1+len_a] == 0x44 || MAC[1+len_a] == 0x46)
		{
			int channel = (MAC[2+len_a] << 8) | MAC[3+len_a];
			int target  = (MAC[4+len_a] << 16) | (MAC[5+len_a] << 8) | MAC[6+len_a];
			int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];
			long int freq = 0;

			fprintf (stderr, "\n Unit to Unit Channel Grant");
			if ( MAC[1+len_a] == 0x46) fprintf (stderr, " Update");
			fprintf (stderr, "\n  CHAN [%04X] Source [%d] Target [%d]", channel, source, target);
			freq = process_channel_to_freq (opts, state, channel);

			//unit to unit needs work, may fail under certain conditions (first blocked, second allowed, etc) (labels should still work though)
			for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == source || state->group_array[i].groupNumber == target)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
        }
      }

			//tune if tuning available
			if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
  		{
				//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
    		if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
    		{
					//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
					if (opts->mod_qpsk == 1)
					{
						int spacing = state->p25_chan_spac[channel >> 12];
						if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
						{
							state->samplesPerSymbol = 8;
							state->symbolCenter = 3;
						}	
					}
					//rigctl
          if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      			SetFreq(opts->rigctl_sockfd, freq);
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
					}
					//rtl_udp
					else if (opts->audio_in_type == 3)
					{
						rtl_udp_tune (opts, state, freq);
						if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
						opts->p25_is_tuned = 1;
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
			long int freq2r = 0;
			fprintf (stderr, "\n Group Voice Channel Grant Update Multiple - Explicit");
			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc1, channelt1, channelr1, group1, group1);
			freq1t = process_channel_to_freq (opts, state, channelt1);
			freq1r = process_channel_to_freq (opts, state, channelr1);
			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc2, channelt2, channelr2, group2, group2);
			freq1t = process_channel_to_freq (opts, state, channelt2);
			freq1r = process_channel_to_freq (opts, state, channelr2);

			int loop = 1;
			if (channelt2 == channelt2) loop = 1;
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
					}
				}

				//tune if tuning available
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency
					{
						//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
						if (opts->mod_qpsk == 1)
						{
							int spacing = state->p25_chan_spac[tunable_chan >> 12];
							if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;
							}	
						}
						
						if (opts->use_rigctl == 1)
						{
							if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
							SetFreq(opts->rigctl_sockfd, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
							j = 8; //break loop
						}
						//rtl_udp
						else if (opts->audio_in_type == 3)
						{
							rtl_udp_tune (opts, state, tunable_freq);
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							j = 8; //break loop
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
		if (MAC[1+len_a] == 0x05) //wonder if this is MAC_SIGNAL only? Too long for a TSBK
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
			freq1 = process_channel_to_freq (opts, state, channel1);
			fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
			freq2 = process_channel_to_freq (opts, state, channel2);
			fprintf (stderr, "\n  Channel 3 [%04X] Group 3 [%d][%04X]", channel3, group3, group3);
			freq3 = process_channel_to_freq (opts, state, channel3);

			int loop = 3;

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
					}
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
						if (opts->mod_qpsk == 1) 
						{
							int spacing = state->p25_chan_spac[tunable_chan >> 12];
							if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;
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
							j = 8; //break loop
						}
						//rtl_udp
						else if (opts->audio_in_type == 3)
						{
							rtl_udp_tune (opts, state, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							j = 8; //break loop
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
			fprintf (stderr, "\n  Channel 2 [%04X] Group 2 [%d][%04X]", channel2, group2, group2);
			freq2 = process_channel_to_freq (opts, state, channel2);

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
					}
				}

				//check to see if the group candidate is blocked first
				if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0)) //DE is digital encrypted, B is block
				{
					//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
					if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && tunable_freq != 0) //if we aren't already on a VC and have a valid frequency already
					{
						//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
						if (opts->mod_qpsk == 1) 
						{
							int spacing = state->p25_chan_spac[tunable_chan >> 12];
							if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
							{
								state->samplesPerSymbol = 8;
								state->symbolCenter = 3;
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
							j = 8; //break loop
						}
						//rtl_udp
						else if (opts->audio_in_type == 3)
						{
							rtl_udp_tune (opts, state, tunable_freq);
							//probably best to only set these when really tuning
							state->p25_vc_freq[0] = state->p25_vc_freq[1] = tunable_freq;
							opts->p25_is_tuned = 1;
							j = 8; //break loop
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

			fprintf (stderr, "\n Group Voice Channel Grant Update - Explicit");
			fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc, channelt, channelr, group, group);
			freq1 = process_channel_to_freq (opts, state, channelt);
			freq2 = process_channel_to_freq (opts, state, channelr);
			if (slot == 0)
			{
				state->lasttg = group;
			}
			else state->lasttgR = group;

			for (int i = 0; i < state->group_tally; i++)
      {
        if (state->group_array[i].groupNumber == group)
        {
          fprintf (stderr, " [%s]", state->group_array[i].groupName);
          strcpy (mode, state->group_array[i].groupMode);
        }
      }

			//tune if tuning available
			if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
  		{
				//reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
    		if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
    		{
					//testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
					if (opts->mod_qpsk == 1)
					{
						int spacing = state->p25_chan_spac[channelt >> 12];
						if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
						{
							state->samplesPerSymbol = 8;
							state->symbolCenter = 3;
						}	
					}
					//rigctl
          if (opts->use_rigctl == 1)
					{
						if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      			SetFreq(opts->rigctl_sockfd, freq1);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
						opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
					}
					//rtl_udp
					else if (opts->audio_in_type == 3)
					{
						rtl_udp_tune (opts, state, freq1);
						state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
						opts->p25_is_tuned = 1;
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

		//SNDCP Data Channel Announcement
		if (MAC[1+len_a] == 0xD6)
		{
			fprintf (stderr, "\n SNDCP Data Channel Announcement ");	
		}

		//MFID90 Group Regroup Add Command
		if (MAC[1+len_a] == 0x81) //needs MAC message len update, may work same as explicit enc regroup?
		{
			fprintf (stderr, "\n MFID90 Group Regroup Add Command ");	
		}

		//Authentication Response - Abbreviated
		if (MAC[1+len_a] == 0x78) 
		{
			int res1 = 0; //don't really care about this value, just setting it to zero
			int source = (MAC[8+len_a] << 16) | (MAC[9+len_a] << 8) | MAC[10+len_a];
			fprintf (stderr, "\n Authentication Response - Abbreviated \n");
			fprintf (stderr, "  Source [%08d][%06X] ", source, source);
		}

		//Authentication Response - Extended
		if (MAC[1+len_a] == 0xF8) 
		{
			//only partial data will print on this, do we really care about the SUID?
			int source = (MAC[5+len_a] << 16) | (MAC[6+len_a] << 8) | MAC[7+len_a];
			fprintf (stderr, "\n Authentication Response - Extended \n");
			fprintf (stderr, "  Source [%08d][%06X] ", source, source);
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
			fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%02X] SITE ID [%02X] CHAN [%04X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channel, sysclass);
			process_channel_to_freq (opts, state, channel);
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
			fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%02X] SITE ID [%02X]\n  CHAN-T [%04X] CHAN-R [%02X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channelt, channelr, sysclass);
			process_channel_to_freq (opts, state, channelt);
			process_channel_to_freq (opts, state, channelr);
		}

		//Unit-to-Unit Answer Request (UU_ANS_REQ)
		if (MAC[1+len_a] == 0x45)
		{
			int svc     = MAC[2+len_a];
			int answer  = MAC[3+len_a];
			int target  = (MAC[4+len_a] << 16) | (MAC[5+len_a] << 8) | MAC[6+len_a];
			int source  = (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | MAC[9+len_a];

			fprintf (stderr, "\n Unit to Unit Channel Answer Request");
			fprintf (stderr, "\n  SVC [%02X] Answer [%02X] Source [%d] Target [%d]", svc, answer, source, target);
			
		}

		//Telephone Interconnect Voice Channel Grant (TELE_INT_CH_GRANT)
		if (MAC[1+len_a] == 0x48)
		{
			fprintf (stderr, "\n Telephone Interconnect Voice Channel Grant ");	
		}

		//Telephone Interconnect Voice Channel Grant Update (TELE_INT_CH_GRANT_UPDT)
		if (MAC[1+len_a] == 0x49)
		{
			fprintf (stderr, "\n Telephone Interconnect Voice Channel Grant Update ");	
		}

		//Synchronization Broadcast (SYNC_BCST)
		if (MAC[1+len_a] == 0x70)
		{
			fprintf (stderr, "\n Synchronization Broadcast \n");
			int ist =   (MAC[3+len_a] >> 2) & 0x1; //IST bit tells us if time is realiable, synced to external source
			int ltoff = (MAC[4+len_a] & 0x3F);
			int year  = MAC[5+len_a] >> 1;
			int month = ((MAC[5+len_a] & 0x1) << 3) | (MAC[6+len_a] >> 5);
			int day   = (MAC[6+len_a] & 0x1F);
			int hour  = MAC[7+len_a] >> 3;
			int min   = ((MAC[7+len_a] & 0x7) << 3) | (MAC[8+len_a] >> 5);
			int slots = ((MAC[8+len_a] & 0x1F) << 8) | MAC[9+len_a];
			int sign = (ltoff & 0b100000) >> 5;
			float offhour = 0;
			//calculate local time (on system) by looking at offset and subtracting 30 minutes increments, or divide by 2 for hourly
			if (sign == 1)
			{
				offhour = -( (ltoff & 0b11111 ) / 2);
			}
			else offhour = ( (ltoff & 0b11111 ) / 2);
			
			int seconds = slots / 135; //very rough estimation, but may be close enough for grins
			if (seconds > 60) seconds = 60; //sanity check for rounding error
			fprintf (stderr, "  Date: 20%02d.%02d.%02d Time: %02d:%02d:%02d UTC\n", 
			          year, month, day, hour, min, seconds); 
			fprintf (stderr, "  Local Time Offset: %.01f Hours;", offhour);
			//if ist bit is set, then time on system may be considered invalid (i.e., no external time sync)
			if (ist % 1)
			{
				fprintf (stderr, " Invalid System Time ");
			}
			else fprintf (stderr, " Valid System Time ");

		}

		//identifier update VHF/UHF
		if (MAC[1+len_a] == 0x74)
		{
			state->p25_chan_iden = MAC[2+len_a] >> 4;
			int iden = state->p25_chan_iden;

			state->p25_chan_type[iden] = 1;
			int bw_vu = (MAC[2+len_a] & 0xF);
			state->p25_trans_off[iden] = (MAC[3+len_a] << 6) | (MAC[4+len_a] >> 2);
			state->p25_chan_spac[iden] = ((MAC[4+len_a] & 0x3) << 8) | MAC[5+len_a];
			state->p25_base_freq[iden] = (MAC[6+len_a] << 24) | (MAC[7+len_a] << 16) | (MAC[8+len_a] << 8) | (MAC[9+len_a] << 0);

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
				fprintf (stderr, "  RFSS[%03d] SITE ID [%03X] CHAN-T [%04X] CHAN-R [%04X] SSC [%02X]", rfssid, siteid, channelt, channelr, sysclass);

				process_channel_to_freq (opts, state, channelt);
				process_channel_to_freq (opts, state, channelr);
			}

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
				fprintf (stderr, "  RFSS[%03d] SITE ID [%03X] CHAN1 [%04X] SSC [%02X] CHAN2 [%04X] SSC [%02X]", rfssid, siteid, channel1, sysclass1, channel2, sysclass2);

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
			int len_grg = MAC[3+len_a] & 0x3F;
			int grg = MAC[4+len_a] >> 5; //3 bits
			int ssn = MAC[4+len_a] & 0x1F; //5 bits
			fprintf (stderr, "\n MFIDA4 Group Regroup Explicit Encryption Command\n");
			//fprintf (stderr, " LEN [%02X] GRG [%02X]\n", len_grg, grg);
			if (grg & 0x2) //if grg == wgid //&0x2 according to OP25
			{
				int sg =  (MAC[5+len_a] << 8) | MAC[6+len_a];
				int key = (MAC[7+len_a] << 8) | MAC[8+len_a];
				int alg = MAC[9+len_a];
				int t1 = (MAC[10+len_a] << 8) | MAC[11+len_a];
				int t2 = (MAC[12+len_a] << 8) | MAC[13+len_a];
				int t3 = (MAC[14+len_a] << 8) | MAC[15+len_a];
				int t4 = (MAC[16+len_a] << 8) | MAC[17+len_a];
				fprintf (stderr, "  SG [%05d] KEY [%04X] ALG [%02X]\n  ", sg, key, alg);
				int a = 0;
				int wgid = 0;
				//worried a bad decode may cause an oob array crash on this one
				for (int i = 10; i < len_grg;)
				{
					//failsafe to prevent oob array
					if ( (i + len_a) > 20)
					{
						goto END_PDU;
					}
					wgid = (MAC[10+len_a+a] << 8) | MAC[11+len_a+a];
					fprintf (stderr, "WGID [%05d][%02X] ", wgid, wgid);
					a = a + 2;
					i = i + 2;
				}

			}
			else //if grg == wuid
			{
				int sg =  (MAC[5+len_a] << 8) | MAC[6+len_a];
				int key = (MAC[7+len_a] << 8) | MAC[8+len_a];
				int t1 = (MAC[9+len_a] << 16) | (MAC[10+len_a] << 8) | MAC[11+len_a];
				int t2 = (MAC[12+len_a] << 16) | (MAC[13+len_a] << 8) | MAC[14+len_a];
				int t3 = (MAC[15+len_a] << 16) | (MAC[16+len_a] << 8) | MAC[17+len_a];
				fprintf (stderr, "  SG [%02X] KEY [%04X]", sg, key);
				fprintf (stderr, " U1 [%02X] U2 [%02X] U3 [%02X] ", t1, t2, t3);

			}


		}

		//1 or 21, group voice channel message, abb and ext
		if (MAC[1+len_a] == 0x1 || MAC[1+len_a] == 0x21)
		{
			int svc =  MAC[2+len_a];
			int gr  = (MAC[3+len_a] << 8) | MAC[4+len_a];
			int src = (MAC[5+len_a] << 16) | (MAC[6+len_a] << 8) | MAC[7+len_a];

			fprintf (stderr, "\n VCH %d - TG %d SRC %d ", slot, gr, src);
			fprintf (stderr, "Group Voice");

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
		//1 or 21, group voice channel message, abb and ext
		if (MAC[1+len_a] == 0x2 || MAC[1+len_a] == 0x22)
		{
			int svc =  MAC[2+len_a];
			int gr  = (MAC[3+len_a] << 16) | (MAC[4+len_a] << 8) | MAC[5+len_a];
			int src = (MAC[6+len_a] << 16) | (MAC[7+len_a] << 8) | MAC[8+len_a];

			fprintf (stderr, "\n VCH %d - TG %d SRC %d ", slot, gr, src);
			fprintf (stderr, "Unit to Unit Voice");

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
			fprintf (stderr, "\n Network Status Broadcast - Extended \n");
			fprintf (stderr, "  LRA [%02X] WACN [%05X] SYSID [%03X] NAC [%03X] CHAN-T [%04X] CHAN-R [%04X]", lra, lwacn, lsysid, lcolorcode, channelt, channelr);
			process_channel_to_freq (opts, state, channelt);
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
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int sysclass = MAC[9+len_a];
			if (1 == 1) //state->p2_is_lcch == 1
			{
				fprintf (stderr, "\n Adjacent Status Broadcast - Abbreviated\n");
				fprintf (stderr, "  LRA [%02X] RFSS[%03d] SYSID [%03X] SITE [%03X] CHAN-T [%04X] SSC [%02X]", lra, rfssid, lsysid, siteid, channelt, sysclass);
				process_channel_to_freq (opts, state, channelt);
			}

		}

		//Adjacent Status Broadcast, extended
		if (MAC[1+len_a] == 0xFC) 
		{
			int lra = MAC[2+len_a];
			int lsysid = ((MAC[3+len_a] & 0xF) << 8) | MAC[4+len_a];
			int rfssid = MAC[5+len_a];
			int siteid = MAC[6+len_a];
			int channelt = (MAC[7+len_a] << 8) | MAC[8+len_a];
			int channelr = (MAC[9+len_a] << 8) | MAC[10+len_a];
			int sysclass = MAC[9+len_a];
			if (1 == 1) //state->p2_is_lcch == 1
			{
				fprintf (stderr, "\n Adjacent Status Broadcast - Extended\n");
				fprintf (stderr, "  LRA [%02X] RFSS[%03d] SYSID [%03X] SITE [%03X] CHAN-T [%04X] CHAN-R [%04X] SSC [%02X]", lra, rfssid, lsysid, siteid, channelt, channelr, sysclass);
				process_channel_to_freq (opts, state, channelt);
				process_channel_to_freq (opts, state, channelr);
			}

		}

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
