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
	int answer_len=0;
	uint8_t answer[32];
	uint8_t sacch_answer[32];
	int lich_parity_received;
	int lich_parity_computed;
	int voice=0;
	int facch=0;
	int facch2=0;
	int sacch=0;
	int cac=0;
	int sr_structure;
	int sr_ran;

	//new, and even more confusing NXDN Type-D / "IDAS" acronyms
	int idas = 0;
	int scch = 0;
	int facch3 = 0;
	int udch2 = 0;

	uint8_t lich_dibits[8]; 
	uint8_t sacch_bits[60];
	uint8_t facch_bits_a[144];
	uint8_t facch_bits_b[144];
	uint8_t cac_bits[300];
	uint8_t facch2_bits[348]; //facch2 or udch, same amount of bits

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
		// state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly 
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
		// state->lastsynctype = -1; //set to -1 so we don't jump back here too quickly 
		goto END;
	}

	switch(lich) { //cases without breaks continue to flow downwards until they hit the break
	case 0x01:	// CAC types
	case 0x05:
		cac = 1;
		break;
	case 0x28:  //facch2 types
	case 0x29:
	case 0x2e:
	case 0x2f:
	case 0x48:
	case 0x49:
	case 0x4e:
	case 0x4f:
		facch2 = 1;
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

	//NXDN "Type-D" or "IDAS" Specific Lich Codes
	case 0x76: //normal voice (in one and two)
	case 0x77: 
		idas = 1;
		scch = 1;
		voice = 3;
		break;
	case 0x74: //vch in 1, scch in 2 (scch 2 steal)
	case 0x75:
		idas = 1;
		voice = 1;
		scch = 2;
		break;
	case 0x72: //scch in 1, vch in 2 (scch 1 steal)
	case 0x73:
		idas = 1;
		voice = 2;
		scch = 2;
		break;
	case 0x70: //scch in 1 and 2 
	case 0x71:
		idas = 1;
		scch = 4; //total of three scch positions
		break;
	case 0x6E: //udch2
	case 0x6F:
		idas = 1;
		udch2 = 1;
		break;
	case 0x68:
	case 0x69: //facch3
		idas = 1;
		facch3 = 1;
		break;
	case 0x62:
	case 0x63: //facch1 in 1, n/post in 2
		idas = 1;
		scch = 1;
		facch = 1;
		break;
	case 0x60:
	case 0x61: //facch1 in both
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

	//printframesync after determining we have a good lich and it has something in it
	if (idas)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "IDAS D ", 0, "-");
		}
		if (opts->payload == 1) fprintf (stderr, "L%02X -", lich);
	}
	else if (voice || facch || sacch || facch2 || cac)
	{
		if (opts->frame_nxdn48 == 1)
		{
			printFrameSync (opts, state, "NXDN48 ", 0, "-");
		}
		else printFrameSync (opts, state, "NXDN96 ", 0, "-");
		if (opts->payload == 1) fprintf (stderr, "L%02X -", lich);
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

	//sacch
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

	//vch frames stay inside dbuf, easier to assign that to ambe_fr frames
	//sacch needs extra handling depending on superframe or non-superframe variety

	if (voice && !facch && scch < 2) //voice only, no facch/scch steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " Voice ");
		fprintf (stderr, "%s", KNRM);
	}
	else if (voice && facch) //voice with facch steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " V%d+F%d ", 3 - facch, facch); //print which position on each
		fprintf (stderr, "%s", KNRM);
	}
	else if (voice && scch > 1) //voice with scch steal
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " V%d+S%d ", 4 - scch, scch); //print which position on each
		fprintf (stderr, "%s", KNRM);
	}
	else
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, " Data  ");
		fprintf (stderr, "%s", KNRM);

		//roll the voice scrambler LFSR here if key available to advance seed (usually just needed on NXDN96)
		if (state->nxdn_cipher_type == 0x1 && state->R != 0) 
		{
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

	//TODO Later: Add Direction to all decoding functions
	uint8_t direction;
	if (lich % 2 == 0) direction = 0;
	else direction = 1;

	//TODO: SCCH handling -- direction will be needed
	if (scch)   fprintf (stderr, " SCCH ");

	//TODO Much Later: FACCH3 and UDCH2 handling
	if (facch3) fprintf (stderr, " FACCH3 ");
	if (udch2)  fprintf (stderr, " UDCH2 ");

	if (sacch)  nxdn_deperm_sacch(opts, state, sacch_bits);
	if (cac)    nxdn_deperm_cac(opts, state, cac_bits); 
	if (facch2) nxdn_deperm_facch2_udch(opts, state, facch2_bits);

	//testing purposes -- don't run facch steals on enc -- causing issues with key loader (VCALL_ASSGN_DUP)
	if (state->nxdn_cipher_type == 0 || state->R == 0) //!voice
	{
		if (facch & 1) nxdn_deperm_facch(opts, state, facch_bits_a);
		if (facch & 2) nxdn_deperm_facch(opts, state, facch_bits_b);
	}
	
	if (voice)
	{
		//restore MBE file open here
		if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
		//update last voice sync time
		state->last_vc_sync_time = time(NULL);
		//process voice frame
		nxdn_voice (opts, state, voice, dbuf); 
	}
	//close MBE file if no voice and its open
	if (!voice)
	{
		if (opts->mbe_out_f != NULL)
		{
			if (opts->frame_nxdn96 == 1) //nxdn96 has voice and data mixed together, so we will need to do a time check first
			{
				if ( (time(NULL) - state->last_vc_sync_time) > 1) //test for optimal time, 1 sec should be okay
				{
					closeMbeOutFile (opts, state);
				} 
			}
			if (opts->frame_nxdn48 == 1) closeMbeOutFile (opts, state); //okay to close right away if nxdn48, no data/voice mixing
		} 
	}

	if (voice && facch == 2) //facch steal 2 -- after voice
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
	

	fprintf (stderr, "\n");
	END: ; //do nothing
}
