/*-------------------------------------------------------------------------------
 * 3.c
 * Placeholder File 3 -- This file is for temporary code storage
 *
 * A 1/2 Rate Convolutional Decoder for NXDN/M17/YSF
 * Used as a secondary decoder in case convolutional decoder fails (second opinion)
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

static const int PARITY[] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1};

// trellis_1_2 encode: source is in bits, result in bits
void trellis_encode(uint8_t result[], const uint8_t source[], int result_len, int reg)
{
	for (int i=0; i<result_len; i+=2) {
		reg = (reg << 1) | source[i>>1];
		result[i] = PARITY[reg & 0x19];
		result[i+1] = PARITY[reg & 0x17];
	}
}

// simplified trellis 2:1 decode; source and result in bits
// assumes that encoding was done with NTEST trailing zero bits
// result_len should be set to the actual number of data bits
// in the original unencoded message (excl. these trailing bits)
void trellis_decode(uint8_t result[], const uint8_t source[], int result_len)
{
	int reg = 0;
	int min_d;
	int min_bt;
	static const int NTEST = 4;
	static const int NTESTC = 1 << NTEST;
	uint8_t bt[NTEST];
	uint8_t tt[NTEST*2];
	int dstats[4];
	int sum;
	for (int p=0; p < 4; p++)
		dstats[p] = 0;
	for (int p=0; p < result_len; p++) {
		for (int i=0; i<NTESTC; i++) {
			bt[0] = (i&8)>>3;
			bt[1] = (i&4)>>2;
			bt[2] = (i&2)>>1;
			bt[3] = (i&1);
			trellis_encode(tt, bt, NTEST*2, reg);
			sum=0;
			for (int j=0; j<NTEST*2; j++) {
				sum += tt[j] ^ source[p*2+j];
			}
			if (i == 0 || sum < min_d) {
				min_d = sum;
				min_bt = bt[0];
			}
		}
		result[p] = min_bt;
		reg = (reg << 1) | min_bt;
		dstats[(min_d > 3) ? 3 : min_d] += 1;
	}
	
	//debug output
	// fprintf (stderr, "\n stats\t%d %d %d %d\n", dstats[0], dstats[1], dstats[2], dstats[3]);
}

//Original Copyright/License

/* -*- c++ -*- */
/* 
 * NXDN Encoder/Decoder (C) Copyright 2019 Max H. Parke KA1RBI
 * 
 * This file is part of OP25
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
