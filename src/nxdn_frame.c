//NXDN frame handler
//Reworked portions from Osmocom OP25 rx_sync.cc

/* -*- c++ -*- */
/* 
 * NXDN Encoder/Decoder (C) Copyright 2019 Max H. Parke KA1RBI
 * 
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "dsd.h"
#include "nxdn_const.h"

#include <assert.h>

void nxdn_frame (dsd_opts * opts, dsd_state * state)
{
  // length is implicitly 192, with frame sync in first 10 dibits
	uint8_t dbuf[182]; 
	uint8_t lich;
	uint8_t answer[32];
	uint8_t sacch_answer[32];
	int lich_parity_received;
	int lich_parity_computed;
	int voice = 0;
	int facch = 0;
	int facch2 = 0;
	int udch = 0;
	int sacch = 0;
	int cac = 0;

	//new, and even more confusing NXDN Type-D / "IDAS" acronyms
	int idas = 0;
	int scch = 0;
	int facch3 = 0;
	int udch2 = 0;

	//new breakdown of lich codes
	uint8_t lich_rf = 0; //RF Channel Type
	uint8_t lich_fc = 0; //Functional Channel Type
	uint8_t lich_op = 0; //Options
	uint8_t direction; //inbound or outbound direction
	UNUSED2(lich_fc, lich_op);

	uint8_t lich_dibits[8]; 
	uint8_t sacch_bits[60];
	uint8_t facch_bits_a[144];
	uint8_t facch_bits_b[144];
	uint8_t cac_bits[300];
	uint8_t facch2_bits[348]; //facch2 or udch, same amount of bits
	uint8_t facch3_bits[288]; //facch3 or udch2, same amount of bits

	//nxdn bit buffer, for easy assignment handling
	int nxdn_bit_buffer[364]; 
	int nxdn_dibit_buffer[182];

	//init all arrays
	memset (dbuf, 0, sizeof(dbuf));
	memset (answer, 0, sizeof(answer));
	memset (sacch_answer, 0, sizeof(sacch_answer));
	memset (lich_dibits, 0, sizeof(lich_dibits));
	memset (sacch_bits, 0, sizeof(sacch_bits));
	memset (facch_bits_b, 0, sizeof(facch_bits_b));
	memset (facch_bits_a, 0, sizeof(facch_bits_a));
	memset (cac_bits, 0, sizeof(cac_bits));
	memset (facch2_bits, 0, sizeof(facch2_bits));

	memset (nxdn_bit_buffer, 0, sizeof(nxdn_bit_buffer));
	memset (nxdn_dibit_buffer, 0, sizeof(nxdn_dibit_buffer));

	//collect lich bits first, if they are good, then we can collect the rest of them
	for (int i = 0; i < 8; i++) lich_dibits[i] = dbuf[i] = getDibit(opts, state);

	nxdn_descramble (lich_dibits, 8);
	
	lich = 0;
	for (int i=0; i<8; i++) lich |= (lich_dibits[i] >> 1) << (7-i);
		
	lich_parity_received = lich & 1;
	lich_parity_computed = ((lich >> 7) + (lich >> 6) + (lich >> 5) + (lich >> 4)) & 1;
	lich = lich >> 1;
	if (lich_parity_received != lich_parity_computed)
	{
		if (opts->payload == 1) fprintf(stderr, "  Lich Parity Error %02X\n", lich);
		state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly 
		goto END;
	}
  
	voice = 0;
	facch = 0;
	facch2 = 0;
	sacch = 0;
	cac = 0;

	//test for inbound direction lich when trunking (false positive) and skip
	//all inbound lich are even value (lsb is set to 0 for inbound direction)
	if (lich % 2 == 0 && opts->p25_trunk == 1)
	{
		if (opts->payload == 1) fprintf(stderr, "  Simplex/Inbound NXDN lich on trunking system - type 0x%02X\n", lich);
		state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly 
		goto END;
	}

	switch(lich) { //cases without breaks continue to flow downwards until they hit the break
	case 0x01:	// CAC types
	case 0x05:
		cac = 1;
		break;
	case 0x28:  //facch2 types
	case 0x29:
	case 0x48:
	case 0x49:
		facch2 = 1;
		break;
	case 0x2e: //udch types
	case 0x2f:
	case 0x4e:
	case 0x4f:
		udch = 1;
		break;
	case 0x32:  //facch in 1, vch in 2
	case 0x33:
	case 0x52:
	case 0x53:
		voice = 2;	
		facch = 1;
		sacch = 1;
		break;
	case 0x34:  //vch in 1, facch in 2
	case 0x35:
	case 0x54:
	case 0x55: //disabled for testing, IDAS system randomly triggers this one, probably due to poor signal
		voice = 1;	
		facch = 2;
		sacch = 1;
		break;
	case 0x36:  //vch in both
	case 0x37:
	case 0x56: 
	case 0x57: 
		voice = 3;	
		facch = 0;
		sacch = 1;
		break;
	case 0x20: //facch in both 
	case 0x21:
	case 0x30:
	case 0x31:
	case 0x40:
	case 0x41:
	case 0x50:
	case 0x51:
		voice = 0;
		facch = 3;	
		sacch = 1;
		break;
	case 0x38: //sacch only (NULL?)
	case 0x39:
		sacch = 1;
		break;

	//NXDN "Type-D" or "IDAS" Specific Lich Codes
	case 0x76: //normal vch voice (in one and two)
	case 0x77: 
		idas = 1;
		scch = 1;
		voice = 3;
		break;
	case 0x74: //vch in 1, facch1 in 2 (facch 2 steal)
	case 0x75:
		idas = 1;
		scch = 1;
		voice = 1;
		facch = 2;
		break;
	case 0x72: //facch in 1, vch in 2 (facch 1 steal)
	case 0x73:
		idas = 1;
		scch = 1;
		voice = 2;
		facch = 1;
		break;
	case 0x70: //facch steal in vch 1 and vch 2 (during voice only)
	case 0x71:
		idas = 1;
		scch = 1;
		facch = 3;
		break;
	case 0x6E: //udch2
	case 0x6F:
		idas = 1;
		scch = 1;
		udch2 = 1;
		break;
	case 0x68:
	case 0x69: //facch3
		idas = 1;
		scch = 1;
		facch3 = 1;
		break;
	case 0x62:
	case 0x63: //facch1 in 1, null data and post field in 2
		idas = 1;
		scch = 1;
		facch = 1;
		break;
	case 0x60:
	case 0x61: //facch1 in both (non vch)
		idas = 1;
		scch = 1;
		facch = 3;
		break;

	default:
    if (opts->payload == 1) fprintf(stderr, "  false sync or unsupported NXDN lich type 0x%02X\n", lich);
		//reset the sacch field, we probably got a false sync and need to wipe or give a bad crc
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly 
		voice = 0;
		goto END;
		break;
	} // end of switch(lich)

	//enable these after good lich parity and known lich value
	state->carrier = 1;
	state->last_cc_sync_time = time(NULL);

	//printframesync after determining we have a good lich and it has something in it
	if (idas)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "IDAS D ", 0, "-");
		}
		if (opts->payload == 1) 
			fprintf (stderr, "L%02X - ", lich);
	}
	else if (voice || facch || sacch || facch2 || udch || cac)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "NXDN48 ", 0, "-");
		}
		else printFrameSync (opts, state, "NXDN96 ", 0, "-");
		if (opts->payload == 1)
			fprintf (stderr, "L%02X - ", lich);
	}

	//now that we have a good LICH, we can collect all of our dibits
	//and push them to the proper places for decoding (if LICH calls for that type)
	for (int i = 0; i < 174; i++) //192total-10FSW-8lich = 174
	{
	 	dbuf[i+8] = getDibit(opts, state);
	}

	nxdn_descramble (dbuf, 182); //sizeof(dbuf)

	//seperate our dbuf (dibit_buffer) into individual bit array
	for (int i = 0; i < 182; i++)
	{
		nxdn_bit_buffer[i*2]   = dbuf[i] >> 1;
		nxdn_bit_buffer[i*2+1] = dbuf[i] & 1;
	}

	//sacch or scch bits
	for (int i = 0; i < 60; i++)
	{
		sacch_bits[i] = nxdn_bit_buffer[i+16];
	}
	
	//facch
	for (int i = 0; i < 144; i++)
	{
		facch_bits_a[i] = nxdn_bit_buffer[i+16+60];
		facch_bits_b[i] = nxdn_bit_buffer[i+16+60+144]; 
	}

	//cac
	for (int i = 0; i < 300; i++)
	{
		cac_bits[i] = nxdn_bit_buffer[i+16];
	}

	//udch or facch2
	for (int i = 0; i < 348; i++)
	{
		facch2_bits[i] = nxdn_bit_buffer[i+16];
	}

	//udch2 or facch3
	for (int i = 0; i < 288; i++)
	{
		facch3_bits[i] = nxdn_bit_buffer[i+16+60];
	}

	//vch frames stay inside dbuf, easier to assign that to ambe_fr frames
	//sacch needs extra handling depending on superframe or non-superframe variety

	//Add advanced decoding of LICH (RF, FC, OPT, and Direction
	lich_rf = (lich >> 5) & 0x3;
	lich_fc = (lich >> 3) & 0x3;
	lich_op = (lich >> 1) & 0x3;
	if (lich % 2 == 0) direction = 0;
	else direction = 1;

	// RF Channel Type
	if (lich_rf == 0) fprintf (stderr, "RCCH ");
	else if (lich_rf == 1) fprintf (stderr, "RTCH ");
	else if (lich_rf == 2) fprintf (stderr, "RDCH ");
	else
	{
		if (lich < 0x60) fprintf (stderr, "RTCH_C ");
		else fprintf (stderr, "RTCH2 ");
	}

	// Functional Channel Type -- things start to get really convoluted here
	// These will echo when handled, either with the decoded message type, or relevant crc err
	// if (lich_rf == 0) //CAC Type
	// {
	// 	//Technically, we should be checking direction as well, but the fc never has split meaning on CAC
	// 	if (lich_fc == 0) fprintf (stderr, "CAC ");
	// 	else if (lich_fc == 1) fprintf (stderr, "Long CAC ");
	// 	else if (lich_fc == 3) fprintf (stderr, "Short CAC ");
	// 	else fprintf (stderr, "Reserved ");
	// }
	// else //USC Type
	// {
	// 	if (lich_fc == 0) fprintf (stderr, "NSF SACCH ");
	// 	else if (lich_fc == 1) fprintf (stderr, "UDCH ");
	// 	else if (lich_fc == 2) fprintf (stderr, "SF SACCH ");
	// 	else if (lich_fc == 3) fprintf (stderr, "SF SACCH/IDLE ");
	// }

#ifdef LIMAZULUTWEAKS

  //LimaZulu specific tweak, load keys from frequency value, if avalable -- test before VCALL
	//needs to be loaded here, if superframe data pair, then we need to run the LFSR on it as well

	if (voice) //can this run TOO frequently?
	{
		long int freq = 0;
		uint8_t hash_bits[24];
		memset (hash_bits, 0, sizeof(hash_bits));
		uint16_t limazulu = 0;
			
		//if not available, then poll rigctl if its available
		if (opts->use_rigctl == 1)
			freq = GetCurrentFreq (opts->rigctl_sockfd);

		//if using rtl input, we can ask for the current frequency tuned
		else if (opts->audio_in_type == 3)
			freq = (long int)opts->rtlsdr_center_freq;

		// freq = 167831250; //hardset for  testing

		//since a frequency value will be larger than the 16-bit max, we need to hash it first
		//the hash has to be run the same way as the import, so at a 24-bit depth, which hopefully
		//will not lead to any duplicate key loads due to multiple CRC16 collisions on a larger value?
		for (int i = 0; i < 24; i++)
			hash_bits[i] = ((freq << i) & 0x800000) >> 23; //load into array for CRC16 

		if (freq) limazulu = ComputeCrcCCITT16d (hash_bits, 24);
		limazulu = limazulu & 0xFFFF; //make sure no larger than 16-bits

		fprintf (stderr, "%s", KYEL);
		if (freq) fprintf (stderr, "\n Freq: %ld - Freq Hash: %d", freq, limazulu);
		if (state->rkey_array[limazulu] != 0) fprintf (stderr, " - Key Loaded: %lld", state->rkey_array[limazulu]);
		fprintf (stderr, "%s", KNRM);

		if (state->rkey_array[limazulu] != 0) 
			state->R = state->rkey_array[limazulu];

		if (state->R != 0 && state->M == 1) state->nxdn_cipher_type = 0x1;

		//add additional time to last_sync_time for LimaZulu to hold on current frequency
		//a little longer without affecting normal scan time on trunk_hangtime variable
		state->last_cc_sync_time = time(NULL) + 2; //ask him for an ideal wait timer
	}

#endif //end LIMAZULUTWEAKS

	if (opts->scanner_mode == 1)
		state->last_cc_sync_time = time(NULL) + 2; //add a little extra hangtime between resuming scan

	//Option/Steal Flags echoed in Voice, V+F, or Data 
	if (voice && !facch) //voice only, no facch steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " Voice ");
		fprintf (stderr, "%s", KNRM);
	}
	else if (voice && facch) //voice with facch1 steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " V%d+F%d ", 3 - facch, facch); //print which position on each
		fprintf (stderr, "%s", KNRM);
	}
	else //Covers FACCH1 in both, FACCH2, UDCH, UDCH2, CAC
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, " Data  ");
		fprintf (stderr, "%s", KNRM);

		//roll the voice scrambler LFSR here if key available to advance seed (usually just needed on NXDN96)
		if (state->nxdn_cipher_type == 0x1 && state->R != 0) 
		{
			if (state->payload_miN == 0)
			{
				state->payload_miN = state->R;
			}

			char ambe_temp[49] = {0};
			char ambe_d[49] = {0};
			for (int i = 0; i < 4; i++)
			{
				LFSRN(ambe_temp, ambe_d, state);
			}
		}

	}

	if (voice && facch == 1) //facch steal 1 -- before voice
	{
		//force scrambler here, but with unspecified key (just use what's loaded)
		if (state->M == 1 && state->R != 0) state->nxdn_cipher_type = 0x1; 
		//roll the voice scrambler LFSR here if key available to advance seed -- half rotation on a facch steal
		if (state->nxdn_cipher_type == 0x1 && state->R != 0)
		{
			if (state->payload_miN == 0)
			{
				state->payload_miN = state->R;
			}

			char ambe_temp[49] = {0};
			char ambe_d[49] = {0};
			for (int i = 0; i < 2; i++)
			{
				LFSRN(ambe_temp, ambe_d, state);
			}
		}  
	}

	if (lich == 0x20 || lich == 0x21 || lich == 0x61 || lich == 0x40 || lich == 0x41) state->nxdn_sacch_non_superframe = TRUE;
	else state->nxdn_sacch_non_superframe = FALSE;

	//TODO Later: Add Direction and/or LICH to all decoding functions
	if (scch) nxdn_deperm_scch (opts, state, sacch_bits, direction);

	if (udch2)  nxdn_deperm_facch3_udch2(opts, state, facch3_bits, 0);
	if (facch3) nxdn_deperm_facch3_udch2(opts, state, facch3_bits, 1);

	if (sacch)  nxdn_deperm_sacch(opts, state, sacch_bits);
	if (cac)    nxdn_deperm_cac(opts, state, cac_bits);

	//Seperated UDCH user data from facch2 data 
	if (udch)   nxdn_deperm_facch2_udch(opts, state, facch2_bits, 0);
	if (facch2) nxdn_deperm_facch2_udch(opts, state, facch2_bits, 1);

	//SHOULD be okay to run facch1's again on steal frames, will need testing
	// if (facch & 1) nxdn_deperm_facch(opts, state, facch_bits_a);
	// if (facch & 2) nxdn_deperm_facch(opts, state, facch_bits_b);

	//only run facch in second slot if its not equal to the first one
	//ideally, this would work better AFTER decoding/FEC
	if (facch & 1) nxdn_deperm_facch(opts, state, facch_bits_a);
	if (facch & 2)
	{
		if (memcmp (facch_bits_a, facch_bits_b, 144) != 0)
			nxdn_deperm_facch(opts, state, facch_bits_b);
	}

	if (voice)
	{
		//restore MBE file open here
		if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
		//update last voice sync time
		state->last_vc_sync_time = time(NULL);
		//turn on scrambler if forced by user option
		if (state->M == 1 && state->R != 0) state->nxdn_cipher_type = 0x1;
		//process voice frame
		nxdn_voice (opts, state, voice, dbuf); 
	}

	//close MBE file if no voice and its open
	if (!voice)
	{
		if (opts->mbe_out_f != NULL)
		{
			if (opts->frame_nxdn96 == 1) //nxdn96 has pure voice and data frames mixed together, so we will need to do a time check first
			{
				if ( (time(NULL) - state->last_vc_sync_time) > 1) //test for optimal time, 1 sec should be okay
				{
					closeMbeOutFile (opts, state);
				} 
			}
			//may need to reconsider this, due to double FACCH1 steals on some Type-C (ASSGN_DUP, etc) and Conventional Systems (random IDLE FACCH1 steal for no reason)
			if (opts->frame_nxdn48 == 1) closeMbeOutFile (opts, state); //okay to close right away if nxdn48, no data/voice frames mixing
		} 
	}

	if (voice && facch == 2) //facch steal 2 -- after voice 1
	{
		//roll the voice scrambler LFSR here if key available to advance seed -- half rotation on a facch steal
		if (state->nxdn_cipher_type == 0x1 && state->R != 0)
		{
			char ambe_temp[49] = {0};
			char ambe_d[49] = {0};
			for (int i = 0; i < 2; i++)
			{
				LFSRN(ambe_temp, ambe_d, state);
			}
		}  
	}
	
	if (opts->payload == 1 && !voice) fprintf (stderr, "\n");
	else if (opts->payload == 0) fprintf (stderr, "\n");

	END: ; //do nothing
}
