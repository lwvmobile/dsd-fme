/*-------------------------------------------------------------------------------
 * 3.c
 * Placeholder File 3 -- This file is for temporary code storage
 *
 * Multiple 1/2 Rate Convolutional Decoders for NXDN/M17/YSF
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

//Ripped from libM17
#define K	5                       //constraint length
#define NUM_STATES (1 << (K - 1)) //number of states

static uint32_t prevMetrics[NUM_STATES];
static uint32_t currMetrics[NUM_STATES];
static uint32_t prevMetricsData[NUM_STATES];
static uint32_t currMetricsData[NUM_STATES];
static uint16_t viterbi_history[244];

/**
* @brief Decode unpunctured convolutionally encoded data.
*
* @param out Destination array where decoded data is written.
* @param in Input data.
* @param len Input length in bits.
* @return Number of bit errors corrected.
*/
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, const uint16_t len)
{
	if(len > 244*2)
		fprintf(stderr, "Input size exceeds max history\n");

	viterbi_reset();

	size_t pos = 0;
	for(size_t i = 0; i < len; i += 2)
	{
		uint16_t s0 = in[i];
		uint16_t s1 = in[i + 1];

		viterbi_decode_bit(s0, s1, pos);
		pos++;
	}
	uint32_t err = viterbi_chainback(out, pos, len/2);

	//debug
	// fprintf (stderr, "\n vcb: \n");
	// for (size_t i = 0; i < len; i++)
	// 	fprintf (stderr, "%02X", out[i]);

	return err;
}

/**
* @brief Decode punctured convolutionally encoded data.
*
* @param out Destination array where decoded data is written.
* @param in Input data.
* @param punct Puncturing matrix.
* @param in_len Input data length.
* @param p_len Puncturing matrix length (entries).
* @return Number of bit errors corrected.
*/
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, const uint16_t in_len, const uint16_t p_len)
{
	if(in_len > 244*2)
	fprintf(stderr, "Input size exceeds max history\n");

	uint16_t umsg[244*2]; //unpunctured message
	uint8_t p=0;		      //puncturer matrix entry
	uint16_t u=0;		      //bits count - unpunctured message
	uint16_t i=0;         //bits read from the input message

	while(i<in_len)
	{
		if(punct[p])
		{
			umsg[u]=in[i];
			i++;
		}
		else
		{
			umsg[u]=0x7FFF;
		}

		u++;
		p++;
		p%=p_len;
	}

	//debug
	// fprintf (stderr, " p: %d, u: %d; p_len: %d; len: %d;", p, u, p_len, (u-in_len)*0x7FFF);

	return viterbi_decode(out, umsg, u) - (u-in_len)*0x7FFF;
}

/**
* @brief Decode one bit and update trellis.
*
* @param s0 Cost of the first symbol.
* @param s1 Cost of the second symbol.
* @param pos Bit position in history.
*/
void viterbi_decode_bit(uint16_t s0, uint16_t s1, const size_t pos)
{
	static const uint16_t COST_TABLE_0[] = {0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	static const uint16_t COST_TABLE_1[] = {0, 0xFFFF, 0xFFFF, 0, 0, 0xFFFF, 0xFFFF, 0};

	for(uint8_t i = 0; i < NUM_STATES/2; i++)
	{
		uint32_t metric = q_abs_diff(COST_TABLE_0[i], s0)
										+ q_abs_diff(COST_TABLE_1[i], s1);


		uint32_t m0 = prevMetrics[i] + metric;
		uint32_t m1 = prevMetrics[i + NUM_STATES/2] + (0x1FFFE - metric);

		uint32_t m2 = prevMetrics[i] + (0x1FFFE - metric);
		uint32_t m3 = prevMetrics[i + NUM_STATES/2] + metric;

		uint8_t i0 = 2 * i;
		uint8_t i1 = i0 + 1;

		if(m0 >= m1)
		{
				viterbi_history[pos]|=(1<<i0);
				currMetrics[i0] = m1;
		}
		else
		{
				viterbi_history[pos]&=~(1<<i0);
				currMetrics[i0] = m0;
		}

		if(m2 >= m3)
		{
				viterbi_history[pos]|=(1<<i1);
				currMetrics[i1] = m3;
		}
		else
		{
				viterbi_history[pos]&=~(1<<i1);
				currMetrics[i1] = m2;
		}
	}

	//swap
	uint32_t tmp[NUM_STATES];
	for(uint8_t i=0; i<NUM_STATES; i++)
	{
		tmp[i]=currMetrics[i];
	}
	for(uint8_t i=0; i<NUM_STATES; i++)
	{
		currMetrics[i]=prevMetrics[i];
		prevMetrics[i]=tmp[i];
	}
}

/**
* @brief History chainback to obtain final byte array.
*
* @param out Destination byte array for decoded data.
* @param pos Starting position for the chainback.
* @param len Length of the output in bits.
* @return Minimum Viterbi cost at the end of the decode sequence.
*/
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len)
{
	uint8_t state = 0;
	size_t bitPos = len+4;

	memset(out, 0, (len-1)/8+1);

	while(pos > 0)
	{
		bitPos--;
		pos--;
		uint16_t bit = viterbi_history[pos]&((1<<(state>>4)));
		state >>= 1;
		if(bit)
		{
			state |= 0x80;
			out[bitPos/8]|=1<<(7-(bitPos%8));
		}
	}

	uint32_t cost = prevMetrics[0];

	for(size_t i = 0; i < NUM_STATES; i++)
	{
		uint32_t m = prevMetrics[i];
		if(m < cost) cost = m;
	}

	//debug
	// fprintf (stderr, "\n cb: \n");
	// for (size_t i = 0; i < len; i++)
	// 	fprintf (stderr, "%02X", out[i]);

	return cost;
}

/**
 * @brief Reset the decoder state. No args.
 *
 */
void viterbi_reset(void)
{
	memset((uint8_t*)viterbi_history, 0, 2*244);
	memset((uint8_t*)currMetrics, 0, 4*NUM_STATES);
	memset((uint8_t*)prevMetrics, 0, 4*NUM_STATES);
	memset((uint8_t*)currMetricsData, 0, 4*NUM_STATES);
	memset((uint8_t*)prevMetricsData, 0, 4*NUM_STATES);
}

uint16_t q_abs_diff(const uint16_t v1, const uint16_t v2)
{
	if(v2 > v1) return v2 - v1;
	return v1 - v2;
}

//original sources
//--------------------------------------------------------------------
// M17 C library - decode/viterbi.c
//
// This file contains:
// - the Viterbi decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------

//--------------------------------------------------------------------
// M17 C library - math/math.c
//
// This file contains:
// - absolute difference value
// - Euclidean norm (L2) calculation for n-dimensional vectors (float)
// - soft-valued arrays to integer conversion (and vice-versa)
// - fixed-valued multiplication and division
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------