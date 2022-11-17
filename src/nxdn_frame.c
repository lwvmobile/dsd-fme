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

	uint8_t lich_dibits[8] = {0}; 
	uint8_t sacch_bits[60] = {0};
	uint8_t facch_bits_a[144] = {0};
	uint8_t facch_bits_b[144] = {0};
	uint8_t cac_bits[300] = {0};
	uint8_t facch2_bits[348] = {0}; //facch2 or udch, same amount of bits

  //nxdn bit buffer, for easy assignment handling
  int nxdn_bit_buffer[364] = {0}; 
	int nxdn_dibit_buffer[182] = {0};

	memset (dbuf, 0, sizeof(dbuf));

	//collect lich bits first, if they are good, then we can collect the rest of them
  for (int i = 0; i < 8; i++) 
  {
		lich_dibits[i] = dbuf[i] = getDibit(opts, state);
  }

	nxdn_descramble (lich_dibits, 8);
	
	lich = 0;
	for (int i=0; i<8; i++)
  {
    lich |= (lich_dibits[i] >> 1) << (7-i);
  }
		
	lich_parity_received = lich & 1;
	lich_parity_computed = ((lich >> 7) + (lich >> 6) + (lich >> 5) + (lich >> 4)) & 1;
	lich = lich >> 1;
	if (lich_parity_received != lich_parity_computed)
  {
    //fprintf(stderr, "NXDN lich parity error, ignoring frame \n");
		return;
	}
  
	voice = 0;
	facch = 0;
	facch2 = 0;
	sacch = 0;
	cac = 0;
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
	case 0x69:
	case 0x6f:
		facch2 = 1;
		break;
	case 0x32:  //facch in 1, vch in 2
	case 0x33:
	case 0x52:
	case 0x53:
	case 0x73:
		voice = 2;	
		facch = 1;
		sacch = 1;
		break;
	case 0x34:  //vch in 1, facch in 2
	case 0x35:
	case 0x54:
	case 0x55:
	case 0x75:
		voice = 1;	
		facch = 2;
		sacch = 1;
		break;
	case 0x36:  //vch in both
	case 0x37:
	case 0x56:
	case 0x57: 
	case 0x77:
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
	case 0x61:
	case 0x71:
		voice = 0;
		facch = 3;	
		sacch = 1;
		break;
	case 0x38: //sacch only (NULL?)
	case 0x39:
		sacch = 1;
		break;
	default:
    if (opts->payload == 1) fprintf(stderr, "  false sync or unsupported NXDN lich type 0x%02X\n", lich);
		//reset the sacch field, we probably got a false sync and need to wipe or give a bad crc
		memset (state->nxdn_sacch_frame_segment, 0, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc)); 
		voice = 0;
		goto END;
		break;
	} // end of switch(lich)

	//printframesync after determining we have a good lich and it has something in it
	if (voice || facch || sacch || facch2 || cac)
	{
		// if (state->synctype == 28) fprintf (stderr, " +");
		// else fprintf (stderr, " -");
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

	if (voice)
	{
		fprintf (stderr, "%s", KGRN);
		fprintf (stderr, " Voice ");
		fprintf (stderr, "%s", KNRM);
	}
	else
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, " Data  ");
		fprintf (stderr, "%s", KNRM);

		//roll the voice scrambler LFSR here if key available to advance seed (usually just needed on NXDN96)
		if (state->nxdn_cipher_type == 0x1 && state->R != 0) //!voice, or voice == 0
		{
			char ambe_temp[49] = {0};
			char ambe_d[49] = {0};
			for (int i = 0; i < 4; i++)
			{
				LFSRN(ambe_temp, ambe_d, state);
			}
		}

	}  

	state->nxdn_sacch_non_superframe = (lich == 0x20 || lich == 0x21 || lich == 0x61 || lich == 0x40 || lich == 0x41) ? true : false;
	if (sacch)nxdn_deperm_sacch(opts, state, sacch_bits);
	if (cac) nxdn_deperm_cac(opts, state, cac_bits); 
	if (facch2) nxdn_deperm_facch2_udch(opts, state, facch2_bits);
	if (facch & 1) nxdn_deperm_facch(opts, state, facch_bits_a);
	if (facch & 2) nxdn_deperm_facch(opts, state, facch_bits_b);
	if (voice) nxdn_voice (opts, state, voice, dbuf); 

	fprintf (stderr, "\n");
	END: ; //do nothing
}
