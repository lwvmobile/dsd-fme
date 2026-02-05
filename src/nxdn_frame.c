
/*-------------------------------------------------------------------------------
 * nxdn_frame.c
 * NXDN Frame Handling
 *
 * Reworked portions from Osmocom OP25 rx_sync.cc
 * NXDN Encoder/Decoder (C) Copyright 2019 Max H. Parke KA1RBI
 *
 * LWVMOBILE
 * 2026-01 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "nxdn_const.h"

#define LICH_ERR_THRESHOLD 7 //Threshold for lich dibit "dividing" errors (8 is perfect, 7 is 1 bit error)
// #define NXDN_DEBUG_LICH      //print LICH debug info on err on payload == 1
#define NXDN_LICH_OFFBITS    //use the offbits to help determine sync status (disable if bad signal / bad sample)

//NXDN Type Enablement (handled by cmake -DTYPEC=OFF , -DTYPED=OFF, -DDCR=OFF)
// #define JPNDCR_DECODE  //Include Japanese DCR Decoding
// #define TYPE_D_DECODE  //Include Type-D Decoding
// #define TYPE_C_DECODE  //Include Type-C and Conventional Decoding (Basically the "normal" NXDN stuff)

void nxdn_frame (dsd_opts * opts, dsd_state * state)
{
  // length is implicitly 192, with frame sync in first 10 dibits
	uint8_t dbuf[182];
	uint8_t lich = 0;
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

	//DCR Mode Specific Things
	int sacch2 = 0;
	int pich_tch = 0;

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

	#ifdef NXDN_DEBUG_LICH
	/*
	//this is how many bad syncs with polarity flipping occurred when allowed both pos and inv polarity on sync test
	Sync: no sync
	[+] [+] [-] [+] [-] [+] [-] [-] [-] [+] [+] [-] [+] (+) 17:36:02 Sync: NXDN48  RDCH Voice  RAN 01 PF X/4
	*/
	//debug polarity (purely illustative, shows how frequently false sync can be attributed to wrong polarity)
	if (state->lastsynctype == 28)
		fprintf (stderr, "[+] ");
	else fprintf (stderr, "[-] ");
	#endif

	//init all arrays
	memset (dbuf, 0, sizeof(dbuf));
	memset (lich_dibits, 0, sizeof(lich_dibits));
	memset (sacch_bits, 0, sizeof(sacch_bits));
	memset (facch_bits_b, 0, sizeof(facch_bits_b));
	memset (facch_bits_a, 0, sizeof(facch_bits_a));
	memset (cac_bits, 0, sizeof(cac_bits));
	memset (facch2_bits, 0, sizeof(facch2_bits));

	memset (nxdn_bit_buffer, 0, sizeof(nxdn_bit_buffer));

	//collect lich bits first, if they are good, then we can collect the rest of them
	for (int i = 0; i < 8; i++) lich_dibits[i] = dbuf[i] = getDibit(opts, state);

	nxdn_pn95_dibit_scrambler (state, lich_dibits, 8);

	//divide lich_dibits to get the lich code w/ parity bit on LSB
	for (int i = 0; i < 8; i++)
	{
		lich <<= 1;
		lich |= (lich_dibits[i] >> 1) & 1;
	}

	//debug lich as a 16-bit value (with encoding "dividing")
	uint8_t lich_bits[16]; memset(lich_bits, 0, sizeof(lich_bits));
	for (int i=0; i<8; i++)
	{
		lich_bits[(i*2)+0] = (lich_dibits[i] >> 1) & 1;
		lich_bits[(i*2)+1] = (lich_dibits[i] >> 0) & 1;
	}
	uint16_t lich_bits_hex = (uint16_t)ConvertBitIntoBytes(lich_bits, 16);
	UNUSED(lich_bits_hex);

	//debug look at the "off bits" of the encoded lich, should be all 1's (8)
	//disble this code if sync issues arise, this may not be ideal of marginal signal
	uint8_t lich_off_hex = 0;
	for (int i=0; i<8; i++)
		lich_off_hex += lich_bits[(i*2)+1];
	#ifdef NXDN_LICH_OFFBITS
	if (lich_off_hex < LICH_ERR_THRESHOLD)
	{
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf(stderr, "  Lich Off Bit Fill Error: %d / 8; \n", lich_off_hex);
		#endif
		state->lastsynctype = -1;  //set to -1 so we don't jump back here too quickly
	}
	#endif

	uint8_t lich_full = lich;
	lich_parity_received = lich & 1;
	lich_parity_computed = ((lich_full >> 7) + (lich_full >> 6) + (lich_full >> 5) + (lich_full >> 4)) & 1;
	lich = lich_full >> 1;

	#ifdef JPNDCR_DECODE
	//special cases on DCR where parity is computed over 7 bits, and not 4 bits
	if (lich == 0x08 || lich == 0x4A || lich == 0x48 || lich == 0x46)
		lich_parity_computed = ((lich_full >> 7) + (lich_full >> 6) + (lich_full >> 5) + (lich_full >> 4) + (lich_full >> 3) + (lich_full >> 2) + (lich_full >> 1)) & 1;
	#endif

	if (lich_parity_received != lich_parity_computed)
	{
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf(stderr, "  Lich Parity Error %02X / %04X\n", lich_full, lich_bits_hex);
		#endif
		state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly
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
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf(stderr, "  Simplex/Inbound NXDN lich on trunking system - type 0x%02X\n", lich);
		#endif
		state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly
	}

	switch(lich)
	{

#ifdef TYPE_C_DECODE
		case 0x01:	// CAC types
		case 0x05:
			cac = 1;
			break;
		case 0x28:  //facch2 types
		case 0x29:
		// case 0x48: //removing from here, moving to DCR as pich_tch
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
		case 0x55:
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
#endif

#ifdef JPNDCR_DECODE
		//DCR Voice
		case 0x46:
			voice = 3;
			sacch2 = 1;
			break;

		//DCR SB0, Data or End Frame
		case 0x08: //SB0 w/ CSM 9 digit BCD found here
			sacch2 = 1;
			pich_tch = 1; //observed 2nd PICH is zero fill
			break;
		case 0x48:
			pich_tch = 3; //may be 1, or 2 TCH (observed possibly both)
		case 0x4A:
			sacch2 = 1;
			break;
#endif

#ifdef TYPE_D_DECODE
		//NXDN "Type-D" or "IDAS" Specific Lich Codes
		case 0x76: //normal vch voice (in one and two)
		case 0x77:
			idas = 1;
			scch = 1;
			voice = 3;
			break;
		// case 0x74: //False Positive on DCR, keep disabled, or revert this line
		case 0x75: //vch in 1, facch1 in 2 (facch 2 steal)
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
	#endif

		default:
			#ifdef NXDN_DEBUG_LICH
			if (opts->payload == 1)
				fprintf(stderr, "  false sync or unsupported NXDN lich type L: %02X / LH: %04X\n", lich, lich_bits_hex);
			#endif
			//reset the sacch field, we probably got a false sync and need to wipe or give a bad crc
			memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
			memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
			state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly
			voice = 0;
			break;
	} // end of switch(lich)

	// //collect remaining dibits at this point (before bad frame skip)
	// for (int i = 0; i < 174; i++) //192total-10FSW-8lich = 174
	//  	dbuf[i+8] = getDibit(opts, state);

	//go to end if bad returns from earlier
	if (state->lastsynctype == -1)
		goto END;

	//collect remaining dibits at this point (after bad frame skip)
	for (int i = 0; i < 174; i++) //192total-10FSW-8lich = 174
	 	dbuf[i+8] = getDibit(opts, state);

	//enable these after good lich parity and known lich value
	state->carrier = 1;
	state->last_cc_sync_time = time(NULL);

	//debug polarity (purely illustative, shows how frequently false sync can be attributed to wrong polarity)
	#ifdef NXDN_DEBUG_LICH
	if (state->lastsynctype == 28)
		fprintf (stderr, "(+) ");
	else fprintf (stderr, "(-) ");
	#endif

	//printframesync after determining we have a good lich and it has something in it
	if (idas)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "IDAS D ", 0, "-");
		}
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf (stderr, "L: %02X / LH: %04X; ", lich, lich_bits_hex);
		#endif
	}
	else if (sacch2)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "JPN DCR", 0, "-");
		}
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf (stderr, "L: %02X / LH: %04X; ", lich, lich_bits_hex);
		#endif
	}
	else if (voice || facch || sacch || facch2 || udch || cac)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "NXDN48 ", 0, "-");
		}
		else printFrameSync (opts, state, "NXDN96 ", 0, "-");
		#ifdef NXDN_DEBUG_LICH
		if (opts->payload == 1)
			fprintf (stderr, "L: %02X / LH: %04X; ", lich, lich_bits_hex);
		#endif
	}

	nxdn_pn95_dibit_scrambler (state, dbuf, 182);

	//seperate our dbuf (dibit_buffer) into individual bit array
	for (int i = 0; i < 182; i++)
	{
		nxdn_bit_buffer[i*2]   = dbuf[i] >> 1;
		nxdn_bit_buffer[i*2+1] = dbuf[i] & 1;
	}

	//sacch or scch bits
	for (int i = 0; i < 60; i++)
		sacch_bits[i] = nxdn_bit_buffer[i+16];

	//facch
	for (int i = 0; i < 144; i++)
	{
		facch_bits_a[i] = nxdn_bit_buffer[i+16+60];
		facch_bits_b[i] = nxdn_bit_buffer[i+16+60+144];
	}

	//cac
	for (int i = 0; i < 300; i++)
		cac_bits[i] = nxdn_bit_buffer[i+16];

	//udch or facch2
	for (int i = 0; i < 348; i++)
		facch2_bits[i] = nxdn_bit_buffer[i+16];

	//udch2 or facch3
	for (int i = 0; i < 288; i++)
		facch3_bits[i] = nxdn_bit_buffer[i+16+60];

	//vch frames stay inside dbuf, easier to assign that to ambe_fr frames
	//sacch needs extra handling depending on superframe or non-superframe variety

	//Add advanced decoding of LICH (RF, FC, OPT, and Direction
	lich_rf = (lich >> 5) & 0x3;
	lich_fc = (lich >> 3) & 0x3;
	lich_op = (lich >> 1) & 0x3;
	if (lich % 2 == 0) direction = 0;
	else direction = 1;

	// RF Channel Type
	if (sacch2 == 0)
	{

		if (lich_rf == 0) fprintf (stderr, "RCCH ");
		else if (lich_rf == 1) fprintf (stderr, "RTCH ");
		else if (lich_rf == 2) fprintf (stderr, "RDCH ");
		else
		{
			if (lich < 0x60) fprintf (stderr, "RTCH_C ");
			else fprintf (stderr, "RTCH2 ");
		}

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

		if (state->R != 0 && state->forced_alg_id == 1) state->nxdn_cipher_type = 0x1;

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
		fprintf (stderr, "Voice ");
		fprintf (stderr, "%s", KNRM);
	}
	else if (voice && facch) //voice with facch1 steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, "V%d+F%d ", 3 - facch, facch); //print which position on each
		fprintf (stderr, "%s", KNRM);
	}
	else //Covers FACCH1 in both, FACCH2, UDCH, UDCH2, CAC
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, "Data  ");
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

		//correct the bit counter if NXDN96 Data Frames (or double FACCH1 steal)
		if (state->nxdn_cipher_type == 0x2 || state->nxdn_cipher_type == 0x3)
			state->bit_counterL += (49*4);

	}

	if (voice && facch == 1) //facch steal 1 -- before voice
	{
		//force scrambler here, but with unspecified key (just use what's loaded)
		if (state->forced_alg_id == 1 && state->R != 0) state->nxdn_cipher_type = 0x1;
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

		//correct the bit counter if FACCH1 steal)
		if (state->nxdn_cipher_type == 0x2 || state->nxdn_cipher_type == 0x3)
			state->bit_counterL += 49*2;
	}

	if (lich == 0x20 || lich == 0x21 || lich == 0x61 || lich == 0x40 || lich == 0x41) state->nxdn_sacch_non_superframe = TRUE;
	else state->nxdn_sacch_non_superframe = FALSE;

	//TODO Later: Add Direction and/or LICH to all decoding functions

	//Type-D "IDAS"
	if (scch) 	idas_scch(opts, state, sacch_bits, direction);
	if (udch2)  idas_facch3_udch2(opts, state, facch3_bits, 0);
	if (facch3) idas_facch3_udch2(opts, state, facch3_bits, 1);

	//NXDN Conventional and Type-C
	if (sacch)	nxdn_sacch(opts, state, sacch_bits);
	if (cac)    nxdn_cac(opts, state, cac_bits);

	//Duplicate facch check inside decoder. store and compare for facch message, depending on passed frame value
	if (facch == 3)
	{
		if (facch & 1) nxdn_facch1(opts, state, facch_bits_a, 1);
		if (facch & 2) nxdn_facch1(opts, state, facch_bits_b, 2);
	}
	else //facch1 steals, or single facch only messages
	{
		if (facch & 1) nxdn_facch1(opts, state, facch_bits_a, 0);
		if (facch & 2) nxdn_facch1(opts, state, facch_bits_b, 0);
	}

	//Seperated UDCH user data from facch2 data
	if (udch)   nxdn_facch2_udch(opts, state, facch2_bits, 0);
	if (facch2) nxdn_facch2_udch(opts, state, facch2_bits, 1);

	//Japanese DCR
	if (sacch2)       dcr_sacch(opts, state, sacch_bits);
	if (pich_tch & 1) dcr_pich_tch(opts, state, facch_bits_a, lich);
	if (pich_tch & 2) dcr_pich_tch(opts, state, facch_bits_b, lich);

	//EHR only (TODO: Make an EFR mode? never observed)
	if (voice)
	{
		//restore MBE file open here
		if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
		//update last voice sync time
		state->last_vc_sync_time = time(NULL);
		//turn on scrambler if forced by user option
		if (state->forced_alg_id == 1 && state->R != 0) state->nxdn_cipher_type = 0x1;
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

		//correct the bit counter if FACCH1 steal)
		if (state->nxdn_cipher_type == 0x2 || state->nxdn_cipher_type == 0x3)
			state->bit_counterL += 49*2;
	}

	if (opts->payload == 1 && !voice) fprintf (stderr, "\n");
	else if (opts->payload == 0) fprintf (stderr, "\n");

	END:

	//if rejected sync, reset carrier and synctype as well
	if (state->lastsynctype == -1)
	{
		state->carrier = 0; //this is why the "Sync: no sync" doesn't show in terminal if we have a bad sync (not an issue, just not consistent)
		state->synctype = -1;
	}
}
