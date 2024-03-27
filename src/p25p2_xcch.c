/*-------------------------------------------------------------------------------
 * p25p2_xcch.c
 * Phase 2 SACCH/FACCH/LCCH Handling
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void process_SACCH_MAC_PDU (dsd_opts * opts, dsd_state * state, int payload[180])
{
	//Figure out which PDU we are looking at, see above info on 8.4.1
	//reorganize bits into bytes and process accordingly

	//new slot variable with flipped assignment for SACCH
	uint8_t slot = (state->currentslot ^ 1) & 1;

	unsigned long long int SMAC[24] = {0}; //22.5 bytes for SACCH MAC PDUs
	int byte = 0;
	int k = 0;
	for (int j = 0; j < 22; j++)
	{
		for (int i = 0; i < 8; i++)
		{
			byte = byte << 1;
			byte = byte | payload[k];
			k++;
		}
		SMAC[j] = byte;
		byte = 0; //reset byte
	}
	SMAC[22] = (payload[176] << 7) | (payload[177] << 6) | (payload[178] << 5) | (payload[179] << 4);

	int opcode = 0;
	opcode = (payload[0] << 2) | (payload[1] << 1) | (payload[2] << 0);
	int mac_offset = 0;
	mac_offset = (payload[3] << 2) | (payload[4] << 1) | (payload[5] << 0);
	int res = (payload[6] << 1) | (payload[7] << 0);
	int b = 9;
	b = (payload[8] << 1) | (payload[9] << 0); //combined b1 and b2
	int mco_a = 69;
	//mco will tell us the number of octets to use in variable length MAC PDUs, need a table or something
	mco_a = (payload[10] << 5) | (payload[11] << 4) | (payload[12] << 3) | (payload[13] << 2) | (payload[14] << 0) | (payload[15] << 0);
	UNUSED3(mac_offset, b, mco_a);

	//get the second mco after determining first message length, see what second mco is and plan accordingly
	int mco_b = 69;
	UNUSED(mco_b);

	//attempt CRC12 check to validate or reject PDU
	int err = -2;
	if (state->p2_is_lcch == 0)
	{
		err = crc12_xb_bridge(payload, 180-12);
		if (err != 0) //CRC Failure, warn or skip.
		{
			if (SMAC[1] == 0x0) //NULL PDU Check, pass if NULL type
			{
				//fprintf (stderr, " NULL ");
			}
			else
			{
				fprintf (stderr, " CRC12 ERR S");
				//if (state->currentslot == 0) state->dmrburstL = 14;
				//else state->dmrburstR = 14;
				goto END_SMAC;
			}
		}
	}
	if (state->p2_is_lcch == 1)
	{
		int len = 0;
		//int len = mac_msg_len[SMAC[1]] * 8;
		//if (len > 164) len = 164; //prevent potential stack smash or other issue.
		len = 164;
		err = crc16_lb_bridge(payload, len);
		if (err != 0) //CRC Failure, warn or skip.
		{
			if (SMAC[1] == 0x0) //NULL PDU Check, pass if NULL type
			{
				//fprintf (stderr, " NULL ");
				state->p2_is_lcch = 0; //turn flag off here
				goto END_SMAC;
			}
			else //permit MAC_SIGNAL on CRC ERR if -F option called
			{
				if (opts->aggressive_framesync == 1)
				{
					fprintf (stderr, " CRC16 ERR L");
					state->p2_is_lcch = 0; //turn flag off here
					//if (state->currentslot == 0) state->dmrburstL = 14;
					//else state->dmrburstR = 14;
					goto END_SMAC;
				}				
			}
		}
	}

	//remember, slots are inverted here, so set the opposite ones
	//monitor, test, and remove these if they cause issues due to inversion
	if (opcode == 0x0)
	{
		fprintf (stderr, " MAC_SIGNAL ");
		//warn user instead of failing
		if (err != 0)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, "CRC16 ERR ");
		}
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 1, SMAC);
		fprintf (stderr, "%s", KNRM);
	}
	//do not permit MAC_PTT with CRC errs, help prevent false positives on calls
	if (opcode == 0x1 && err == 0)
	{
		fprintf (stderr, " MAC_PTT ");
		fprintf (stderr, "%s", KGRN);
		//remember, slots are inverted here, so set the opposite ones
		if (state->currentslot == 1)
		{
			//reset fourv_counter and dropbyte on PTT
			state->fourv_counter[0] = 0;
			state->voice_counter[0] = 0;
			state->dropL = 256;

			state->dmrburstL = 20;
			fprintf (stderr, "\n VCH 0 - ");

			state->lastsrc = (SMAC[13] << 16) | (SMAC[14] << 8) | SMAC[15];
			state->lasttg  = (SMAC[16] << 8) | SMAC[17];

			fprintf (stderr, "TG %d ", state->lasttg);
			fprintf (stderr, "SRC %d ", state->lastsrc);

			/*
			When the talker radio is initiating an individual call (unit to unit or telephone interconnect), the reserved group ID of zero
			is used in the group address portion of the MAC_PTT PDU and the source ID is that of the talker radio.

			The outbound SACCH for the talker radio containing the talker ID information is required at the
			talker radio site and optional at other sites. (So, if its from an external site, then it can be zero?)
			
			*/

			// if (state->lastsrc == 0) fprintf (stderr, "External ");

			state->payload_algid =  SMAC[10];
			state->payload_keyid = (SMAC[11] << 8) | SMAC[12];
			state->payload_miP =   (SMAC[1] << 56) | (SMAC[2] << 48) | (SMAC[3] << 40) | (SMAC[4] << 32) |
			                       (SMAC[5] << 24) | (SMAC[6] << 16) | (SMAC[7] << 8)  | (SMAC[8] << 0);

			if (state->payload_algid != 0x80 && state->payload_algid != 0x0)
			{
				fprintf (stderr, "%s", KYEL);
				fprintf (stderr, "\n         ALG ID 0x%02X", state->payload_algid);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyid);
				fprintf (stderr, " MI 0x%016llX", state->payload_miP);
				if (state->R != 0 && state->payload_algid == 0xAA) fprintf (stderr, " Key 0x%010llX", state->R);
				fprintf(stderr, " MPTT");
				// fprintf (stderr, " %s", KRED);
				// fprintf (stderr, "ENC");
			}

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gain = opts->audio_gain;

		}

		if (state->currentslot == 0)
		{
			//reset fourv_counter and dropbyte on PTT
			state->fourv_counter[1] = 0;
			state->voice_counter[1] = 0;
			state->dropR = 256;
			state->payload_algidR = 0; //zero this out as well

			state->dmrburstR = 20;
			fprintf (stderr, "\n VCH 1 - ");

			state->lastsrcR = (SMAC[13] << 16) | (SMAC[14] << 8) | SMAC[15];
			state->lasttgR  = (SMAC[16] << 8) | SMAC[17];

			fprintf (stderr, "TG %d ", state->lasttgR);
			fprintf (stderr, "SRC %d ", state->lastsrcR);

			// if (state->lastsrcR == 0) fprintf (stderr, "External ");

			state->payload_algidR =  SMAC[10];
			state->payload_keyidR = (SMAC[11] << 8) | SMAC[12];
			state->payload_miN =    (SMAC[1] << 56) | (SMAC[2] << 48) | (SMAC[3] << 40) | (SMAC[4] << 32) |
			                        (SMAC[5] << 24) | (SMAC[6] << 16) | (SMAC[7] << 8)  | (SMAC[8] << 0);

			if (state->payload_algidR != 0x80 && state->payload_algidR != 0x0)
			{
				fprintf (stderr, "%s", KYEL);
				fprintf (stderr, "\n         ALG ID 0x%02X", state->payload_algidR);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyidR);
				fprintf (stderr, " MI 0x%016llX", state->payload_miN);
				if (state->RR != 0 && state->payload_algidR == 0xAA) fprintf (stderr, " Key 0x%010llX", state->RR);
				fprintf(stderr, " MPTT");
				// fprintf (stderr, " %s", KRED);
				// fprintf (stderr, "ENC");
			}

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gainR = opts->audio_gain;

		}

		if (opts->payload == 1)
		{
			fprintf (stderr, "\n MAC_PTT_PAYLOAD_S OFFSET: %d RES: %d \n ", mac_offset, res);
			for (int i = 0; i < 24; i++)
			{
				if (i == 12) fprintf (stderr, "\n ");
				fprintf (stderr, "[%02llX]", SMAC[i]);
			}
		}

		fprintf (stderr, "%s", KNRM);
	}
	//do not permit MAC_PTT_END with CRC errs, help prevent false positives on calls
	if (opcode == 0x2 && err == 0)
	{
		fprintf (stderr, " MAC_END_PTT ");
		fprintf (stderr, "%s", KRED);
		//remember, slots are inverted here, so set the opposite ones
		if (state->currentslot == 1)
		{
			
			state->fourv_counter[0] = 0;
			state->voice_counter[0] = 0;
			state->dropL = 256;
			state->dmrburstL = 23;
			state->payload_algid = 0; 
			state->payload_keyid = 0; 

			fprintf (stderr, "\n VCH 0 - ");
			fprintf (stderr, "TG %d ", state->lasttg);
			fprintf (stderr, "SRC %d ", state->lastsrc);

			//print it and then zero out
			state->lastsrc = 0;
			state->lasttg = 0;

			//close any open MBEout files
 			if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);

			//blank the call string here -- slot variable is already flipped accordingly for sacch
			sprintf (state->call_string[slot], "%s", "                     "); //21 spaces -- wrong placement!

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gain = opts->audio_gain;

		}
		if (state->currentslot == 0)
		{
			
			state->fourv_counter[1] = 0;
			state->voice_counter[1] = 0;
			state->dropR = 256;
			state->dmrburstR = 23;
			state->payload_algidR = 0;
			state->payload_keyidR	= 0;

			fprintf (stderr, "\n VCH 1 - ");
			fprintf (stderr, "TG %d ", state->lasttgR);
			fprintf (stderr, "SRC %d ", state->lastsrcR);

			//print it and then zero out
			state->lastsrcR = 0;
			state->lasttgR = 0;

			//close any open MBEout files
			if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

			//blank the call string here -- slot variable is already flipped accordingly for sacch
			sprintf (state->call_string[slot], "%s", "                     "); //21 spaces -- wrong placement!

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gainR = opts->audio_gain;

		}

		//Return to CC on MAC_END_PTT if other slot is idle
		//TODO: Look at the source address of the END_PTT, if it is 0xFFFFFF, then its from the FNE?
		//if from the FNE, that may be when we know channel teardown will really occur vs using MAC_END_PTT directly

		/*
		Upon receipt of a MAC_END_PTT PDU on the assigned VCH, and having verified it contains the
		correct color code, the SU shall return to the idle state on the control channel.
		*/

		//NOTE: Disable return later on if not desirable to use

		//end_ptt in this slot, idle in the other slot (Normal Slots)
		if ( ((state->currentslot == 0) && (state->dmrburstL == 24)) ||
		     ((state->currentslot == 1) && (state->dmrburstR == 24))   )
		{
			if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1)
			{

				//if P25p2 VCH and going back to P25p1 CC, flip symbolrate
				if (state->p25_cc_is_tdma == 0)
				{
					state->samplesPerSymbol = 10;
					state->symbolCenter = 4;
					//re-enable both slots
					opts->slot1_on = 1;
					opts->slot2_on = 1;
				}

				//rigctl
				if (opts->use_rigctl == 1)
				{
					state->lasttg = 0;
					state->lastsrc = 0;
					state->lasttgR = 0;
					state->lastsrcR = 0;
					state->payload_algid = 0; 
					state->payload_keyid = 0;
					state->payload_algidR = 0; 
					state->payload_keyidR = 0;
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
					state->lasttgR = 0;
					state->lastsrcR = 0;
					state->payload_algid = 0; 
					state->payload_keyid = 0;
					state->payload_algidR = 0; 
					state->payload_keyidR = 0;
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

		fprintf (stderr, "%s", KNRM);
	}
	if (opcode == 0x3 && err == 0)
	{
		if (state->currentslot == 1) state->dmrburstL = 24;
		else state->dmrburstR = 24;
		fprintf (stderr, " MAC_IDLE ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 1, SMAC);
		fprintf (stderr, "%s", KNRM);

		// if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1)
		// {
		// 	if (state->currentslot == 1)
		// 	{
		// 		opts->slot1_on = 0;
		// 		opts->slot2_on = 1;
		// 	}
		// 	else
		// 	{
		// 		opts->slot1_on = 1;
		// 		opts->slot2_on = 0;
		// 	}
		// }

		//blank the call string here -- slot variable is already flipped accordingly for sacch
		sprintf (state->call_string[slot], "%s", "                     "); //21 spaces

	}
	if (opcode == 0x4 && err == 0)
	{
		//disable to prevent blinking in ncurses terminal due to OSS preemption shim
		#ifdef AERO_BUILD
		if (opts->audio_out_type != 5)
		{
			if (state->currentslot == 1) state->dmrburstL = 21;
			else state->dmrburstR = 21;
		}
		#else
		if (state->currentslot == 1) state->dmrburstL = 21;
		else state->dmrburstR = 21;
		#endif
		
		fprintf (stderr, " MAC_ACTIVE ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 1, SMAC);
		fprintf (stderr, "%s", KNRM);
	}
	if (opcode == 0x6 && err == 0)
	{
		if (state->currentslot == 1)
		{
			state->dmrburstL = 22;
			//close any open MBEout files
 			if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
		} 
		else
		{
			state->dmrburstR = 22;
			//close any open MBEout files
 			if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);
		}
		fprintf (stderr, " MAC_HANGTIME ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 1, SMAC);
		fprintf (stderr, "%s", KNRM);
	}

	END_SMAC:
	if (1 == 2)
	{
		//CRC Failure!
	}

}

void process_FACCH_MAC_PDU (dsd_opts * opts, dsd_state * state, int payload[156])
{
	//Figure out which PDU we are looking at, see above info on 8.4.1
	//reorganize bits into bytes and process accordingly

	//new slot variable
	uint8_t slot = state->currentslot;

	unsigned long long int FMAC[24] = {0}; //19.5 bytes for FACCH MAC PDUs, add padding to end
	int byte = 0;
	int k = 0;
	for (int j = 0; j < 19; j++)
	{
		for (int i = 0; i < 8; i++)
		{
			byte = byte << 1;
			byte = byte | payload[k];
			k++;
		}
		FMAC[j] = byte;
		byte = 0; //reset byte
	}
	FMAC[19] = (payload[152] << 7) | (payload[153] << 6) | (payload[154] << 5) | (payload[155] << 4);

	//add padding bytes so we can have a unified variable MAC PDU handler
	for (int i = 0; i < 3; i++)
	{
		FMAC[i+20] = 0;
	}

	int opcode = 0;
	opcode = (payload[0] << 2) | (payload[1] << 1) | (payload[2] << 0);
	int mac_offset = 0;
	mac_offset = (payload[3] << 2) | (payload[4] << 1) | (payload[5] << 0);
	int res = (payload[6] << 1) | (payload[7] << 0);
	UNUSED(mac_offset);

	//attempt CRC check to validate or reject PDU
	int err = -2;
	if (state->p2_is_lcch == 0)
	{
		err = crc12_xb_bridge(payload, 156-12);
		if (err != 0) //CRC Failure, warn or skip.
		{
			if (FMAC[1] == 0x0) //NULL PDU Check, pass if NULL
			{
				//fprintf (stderr, " NULL ");
			}
			else
			{
				fprintf (stderr, " CRC12 ERR F");
				//if (state->currentslot == 0) state->dmrburstL = 14;
				//else state->dmrburstR = 14;
				goto END_FMAC;
			}
		}
	}


	//Not sure if a MAC Signal will come on a FACCH or not, so disable to prevent falsing
	// if (opcode == 0x0)
	// {
	// 	fprintf (stderr, " MAC_SIGNAL ");
	// 	fprintf (stderr, "%s", KYEL);
	// 	process_MAC_VPDU(opts, state, 0, FMAC);
	// 	fprintf (stderr, "%s", KNRM);
	// }

	if (opcode == 0x1 && err == 0)
	{

		fprintf (stderr, " MAC_PTT  ");
		fprintf (stderr, "%s", KGRN);
		if (state->currentslot == 0)
		{
			//reset fourv_counter and dropbyte on PTT
			state->fourv_counter[0] = 0;
			state->voice_counter[0] = 0;
			state->dropL = 256;

			state->dmrburstL = 20;
			fprintf (stderr, "\n VCH 0 - ");

			state->lastsrc = (FMAC[13] << 16) | (FMAC[14] << 8) | FMAC[15];
			state->lasttg  = (FMAC[16] << 8) | FMAC[17];

			fprintf (stderr, "TG %d ", state->lasttg);
			fprintf (stderr, "SRC %d ", state->lastsrc);

			// if (state->lastsrc == 0) fprintf (stderr, "External ");

			state->payload_algid =  FMAC[10];
			state->payload_keyid = (FMAC[11] << 8) | FMAC[12];
			state->payload_miP =   (FMAC[1] << 56) | (FMAC[2] << 48) | (FMAC[3] << 40) | (FMAC[4] << 32) |
			                       (FMAC[5] << 24) | (FMAC[6] << 16) | (FMAC[7] << 8)  | (FMAC[8] << 0);

			if (state->payload_algid != 0x80 && state->payload_algid != 0x0)
			{
				fprintf (stderr, "%s", KYEL);
				fprintf (stderr, "\n         ALG ID 0x%02X", state->payload_algid);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyid);
				fprintf (stderr, " MI 0x%016llX", state->payload_miP);
				if (state->R != 0 && state->payload_algid == 0xAA) fprintf (stderr, " Key 0x%010llX", state->R);
				fprintf(stderr, " MPTT");
				// fprintf (stderr, " %s", KRED);
				// fprintf (stderr, "ENC");
			}

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gain = opts->audio_gain;

		}

		if (state->currentslot == 1)
		{
			//reset fourv_counter and dropbyte on PTT
			state->fourv_counter[1] = 0;
			state->voice_counter[1] = 0;
			state->dropR = 256;

			state->dmrburstR = 20;
			fprintf (stderr, "\n VCH 1 - ");

			state->lastsrcR = (FMAC[13] << 16) | (FMAC[14] << 8) | FMAC[15];
			state->lasttgR  = (FMAC[16] << 8) | FMAC[17];

			fprintf (stderr, "TG %d ", state->lasttgR);
			fprintf (stderr, "SRC %d ", state->lastsrcR);

			// if (state->lastsrcR == 0) fprintf (stderr, "External ");

			state->payload_algidR =  FMAC[10];
			state->payload_keyidR = (FMAC[11] << 8) | FMAC[12];
			state->payload_miN =    (FMAC[1] << 56) | (FMAC[2] << 48) | (FMAC[3] << 40) | (FMAC[4] << 32) |
			                        (FMAC[5] << 24) | (FMAC[6] << 16) | (FMAC[7] << 8)  | (FMAC[8] << 0);

			if (state->payload_algidR != 0x80 && state->payload_algidR != 0x0)
			{
				fprintf (stderr, "%s", KYEL);
				fprintf (stderr, "\n         ALG ID 0x%02X", state->payload_algidR);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyidR);
				fprintf (stderr, " MI 0x%016llX", state->payload_miN);
				if (state->RR != 0 && state->payload_algidR == 0xAA) fprintf (stderr, " Key 0x%010llX", state->RR);
				fprintf(stderr, " MPTT");
				// fprintf (stderr, " %s", KRED);
				// fprintf (stderr, "ENC");
			}

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gainR = opts->audio_gain;

		}

		if (opts->payload == 1)
		{
			fprintf (stderr, "\n MAC_PTT_PAYLOAD_F OFFSET: %d RES: %d \n ", mac_offset, res);
			for (int i = 0; i < 24; i++)
			{
				if (i == 12) fprintf (stderr, "\n ");
				fprintf (stderr, "[%02llX]", FMAC[i]);
			}
		}
		fprintf (stderr, "%s", KNRM);

	}
	if (opcode == 0x2 && err == 0)
	{
		fprintf (stderr, " MAC_END_PTT ");
		fprintf (stderr, "%s", KRED);
		if (state->currentslot == 0)
		{
			
			state->fourv_counter[0] = 0;
			state->voice_counter[0] = 0;
			state->dropL = 256;
			state->dmrburstL = 23;
			state->payload_algid = 0; //zero this out as well
			state->payload_keyid = 0;

			fprintf (stderr, "\n VCH 0 - ");
			fprintf (stderr, "TG %d ", state->lasttg);
			fprintf (stderr, "SRC %d ", state->lastsrc);

			//print it and then zero out
			state->lastsrc = 0;
			state->lasttg = 0;

			//close any open MBEout files
 			if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);

			//blank the call string here 
			sprintf (state->call_string[slot], "%s", "                     "); //21 spaces

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gain = opts->audio_gain; //reset
			
		}
		if (state->currentslot == 1)
		{
			
			state->fourv_counter[1] = 0;
			state->voice_counter[1] = 0;
			state->dropR = 256;
			state->dmrburstR = 23;
			state->payload_algidR = 0; //zero this out as well
			state->payload_keyidR = 0;

			fprintf (stderr, "\n VCH 1 - ");
			fprintf (stderr, "TG %d ", state->lasttgR);
			fprintf (stderr, "SRC %d ", state->lastsrcR);

			//print it and then zero out
			state->lastsrcR = 0;
			state->lasttgR = 0;

			//close any open MBEout files
			if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

			//reset gain
			if (opts->floating_point == 1)
				state->aout_gainR = opts->audio_gain;
		}

		//Return to CC on MAC_END_PTT if other slot is idle
		//NOTE: Disable return later on if not desirable to use

		//end_ptt in this slot, idle in the other slot (Normal Slots)
		if ( ((state->currentslot == 0) && (state->dmrburstR == 24)) ||
		     ((state->currentslot == 1) && (state->dmrburstL == 24))   )
		{
			if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1)
			{
				
				//if P25p2 VCH and going back to P25p1 CC, flip symbolrate
				if (state->p25_cc_is_tdma == 0)
				{
					state->samplesPerSymbol = 10;
					state->symbolCenter = 4;
					//re-enable both slots
					opts->slot1_on = 1;
					opts->slot2_on = 1;
				}

				//rigctl
				if (opts->use_rigctl == 1)
				{
					state->lasttg = 0;
					state->lastsrc = 0;
					state->lasttgR = 0;
					state->lastsrcR = 0;
					state->payload_algid = 0; 
					state->payload_keyid = 0;
					state->payload_algidR = 0; 
					state->payload_keyidR = 0;
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
					state->lasttgR = 0;
					state->lastsrcR = 0;
					state->payload_algid = 0; 
					state->payload_keyid = 0;
					state->payload_algidR = 0; 
					state->payload_keyidR = 0;
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

		fprintf (stderr, "%s", KNRM);
	}
	if (opcode == 0x3 && err == 0)
	{
		//what else should we zero out here?
		//disable any of the lines below if issues are observed
		if (state->currentslot == 0)
		{
			state->payload_algid = 0;
			state->payload_keyid = 0;
			state->dmrburstL = 24;
			state->fourv_counter[0] = 0;
			state->voice_counter[0] = 0;
			state->lastsrc = 0;
			state->lasttg = 0;

		}
		else
		{
			state->payload_algidR = 0;
			state->payload_keyidR = 0;
			state->dmrburstR = 24;
			state->fourv_counter[1] = 0;
			state->voice_counter[1] = 0;
			state->lastsrcR = 0;
			state->lasttgR = 0;

		}
		fprintf (stderr, " MAC_IDLE ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 0, FMAC);
		fprintf (stderr, "%s", KNRM);

		// if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1)
		// {
		// 	if (state->currentslot == 0)
		// 	{
		// 		opts->slot1_on = 0;
		// 		opts->slot2_on = 1;
		// 	}
		// 	else
		// 	{
		// 		opts->slot1_on = 1;
		// 		opts->slot2_on = 0;
		// 	}
		// }

		//blank the call string here 
		sprintf (state->call_string[slot], "%s", "                     "); //21 spaces
			
	}
	if (opcode == 0x4 && err == 0)
	{
		//disable to prevent blinking in ncurses terminal due to OSS preemption shim
		#ifdef AERO_BUILD
		if (opts->audio_out_type != 5)
		{
			if (state->currentslot == 0) state->dmrburstL = 21;
			else state->dmrburstR = 21;
		}
		#else
		if (state->currentslot == 0) state->dmrburstL = 21;
		else state->dmrburstR = 21;
		#endif
		
		fprintf (stderr, " MAC_ACTIVE ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 0, FMAC);
		fprintf (stderr, "%s", KNRM);
	}
	if (opcode == 0x6 && err == 0)
	{
		if (state->currentslot == 0)
		{
			state->dmrburstL = 22;
			//close any open MBEout files
 			if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
		} 
		else
		{
			state->dmrburstR = 22;
			//close any open MBEout files
 			if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);
		} 
		fprintf (stderr, " MAC_HANGTIME ");
		fprintf (stderr, "%s", KYEL);
		process_MAC_VPDU(opts, state, 0, FMAC);
		fprintf (stderr, "%s", KNRM);
	}

	END_FMAC:
	if (1 == 2)
	{
		//CRC Failure!
	}

}