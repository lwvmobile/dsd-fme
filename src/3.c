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


//audio filter stuff sourced from: https://github.com/NedSimao/FilteringLibrary
//no license / information provided in source code
#define PI 3.141592653

//do we need this for test input sthit?
// #define PLOT_POINTS     100
// #define SAMPLE_TIME_S   0.001
// #define CUTOFFFREQ_HZ   50
// #define sine_freq       40
// #define sine_freq_1     500
// #define HFC 50
// #define LFC 100


void LPFilter_Init(LPFilter *filter, float cutoffFreqHz, float sampleTimeS)
{

	float RC=0.0;
	RC=1.0/(2*PI*cutoffFreqHz);
	filter->coef[0]=sampleTimeS/(sampleTimeS+RC);
	filter->coef[1]=RC/(sampleTimeS+RC);

	filter->v_out[0]=0.0;
	filter->v_out[1]=0.0;

}


float LPFilter_Update(LPFilter *filter, float v_in)
{

	filter->v_out[1]=filter->v_out[0];
	filter->v_out[0]=(filter->coef[0]*v_in) + (filter->coef[1]*filter->v_out[1]);

	return (filter->v_out[0]);

}


/********************************************************************************************************
 *                              HIGH PASS FILTER
********************************************************************************************************/
void HPFilter_Init(HPFilter *filter, float cutoffFreqHz, float sampleTimeS)
{

	float RC=0.0;
	RC=1.0/(2*PI*cutoffFreqHz);

	filter->coef=RC/(sampleTimeS+RC);

	filter->v_in[0]=0.0;
	filter->v_in[1]=0.0;

	filter->v_out[0]=0.0;
	filter->v_out[1]=0.0;

}


float HPFilter_Update(HPFilter *filter, float v_in)
{
    
	filter->v_in[1]=filter->v_in[0];
	filter->v_in[0]=v_in;

	filter->v_out[1]=filter->v_out[0];
	filter->v_out[0]=filter->coef * (filter->v_in[0] - filter->v_in[1]+filter->v_out[1]);

	return (filter->v_out[0]);

}

/********************************************************************************************************
 *                              BAND PASS FILTER
********************************************************************************************************/


void PBFilter_Init(PBFilter *filter, float HPF_cutoffFreqHz, float LPF_cutoffFreqHz, float sampleTimeS)
{

	LPFilter_Init(&filter->lpf, LPF_cutoffFreqHz, sampleTimeS);
	HPFilter_Init(&filter->hpf, HPF_cutoffFreqHz, sampleTimeS);

	filter->out_in=0.0;

}

float PBFilter_Update(PBFilter *filter, float v_in)
{

	filter->out_in=HPFilter_Update(&filter->hpf, v_in);

	filter->out_in=LPFilter_Update(&filter->lpf, filter->out_in);

	return (filter->out_in);

}

/********************************************************************************************************
 *                              NOTCH FILTER
********************************************************************************************************/


void NOTCHFilter_Init(NOTCHFilter *filter, float centerFreqHz, float notchWidthHz, float sampleTimeS)
{

	//filter frequency to angular (rad/s)
	float w0_rps=2.0 * PI *centerFreqHz;
	float ww_rps=2.0 * PI *notchWidthHz;

	//pre warp center frequency
	float w0_pw_rps=(2.0/sampleTimeS) * tanf(0.5 * w0_rps * sampleTimeS);

	//computing filter coefficients

	filter->alpha=4.0 + w0_rps*w0_pw_rps*sampleTimeS*sampleTimeS;
	filter->beta=2.0*ww_rps*sampleTimeS;

	//clearing input and output  buffers

	for (uint8_t n=0; n<3; n++)
	{
		filter->vin[n]=0;
		filter->vout[n]=0;
	}

}

float NOTCHFilter_Update(NOTCHFilter *filter, float vin)
{

	//shifting samples
	filter->vin[2]=filter->vin[1];
	filter->vin[1]=filter->vin[0];

	filter->vout[2]=filter->vout[1];
	filter->vout[1]=filter->vout[0];

	filter->vin[0]=vin;

	//compute new output
	filter->vout[0]=(filter->alpha*filter->vin[0] + 2.0 *(filter->alpha -8.0)*filter->vin[1] + filter->alpha*filter->vin[2]
	-(2.0f*(filter->alpha-8.0)*filter->vout[1]+(filter->alpha-filter->beta)*filter->vout[2]))/(filter->alpha+filter->beta);


	return (filter->vout[0]);
}

void init_audio_filters (dsd_state * state)
{
	//still not sure if this is even correct or not, but 48k sounds good now
  LPFilter_Init(&state->RCFilter, 960, (float)1/(float)48000);
  HPFilter_Init(&state->HRCFilter, 960, (float)1/(float)48000);

	//left and right variants for stereo output testing on digital voice samples
  LPFilter_Init(&state->RCFilterL, 960, (float)1/(float)16000);
  HPFilter_Init(&state->HRCFilterL, 960, (float)1/(float)16000);
  LPFilter_Init(&state->RCFilterR, 960, (float)1/(float)16000);
  HPFilter_Init(&state->HRCFilterR, 960, (float)1/(float)16000);

	//PBFilter_Init(PBFilter *filter, float HPF_cutoffFreqHz, float LPF_cutoffFreqHz, float sampleTimeS);
	//NOTCHFilter_Init(NOTCHFilter *filter, float centerFreqHz, float notchWidthHz, float sampleTimeS);

	//NOTE: PBFilter_init also inits a LPF and HPF, but on another set of filters, might be worth
	//testing just using the PBFilter by itself and see how it does without the hpf used

	//passband filter working (seems to be), notch filter unsure which values to use, doesn't have any appreciable affect when used as is
	PBFilter_Init(&state->PBF, 8000, 12000, (float)1/(float)1536000); //RTL Sampling at 1536000 S/s.
	NOTCHFilter_Init(&state->NF, 1000, 4000, (float)1/(float)1536000);
	
}

//FUNCTIONS for handing use of above filters

//lpf
void lpf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = LPFilter_Update(&state->RCFilter, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf
void hpf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilter, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf digital left
void hpf_dL(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilterL, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf digital right
void hpf_dR(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilterR, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//nf
void nf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = NOTCHFilter_Update(&state->NF, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//pbf
void pbf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = PBFilter_Update(&state->PBF, input[i]);
	}
}

//Generic RMS function derived from RTL_FM (RTL_SDR) RMS code (doesnt' really work correctly outside of RTL)
long int raw_rms(int16_t *samples, int len, int step) //use samplespersymbol as len
{
  
  int i;
  long int rms;
  long p, t, s;
  double dc, err;

  p = t = 0L;
  for (i=0; i<len; i+=step) {
    s = (long)samples[i];
    t += s;
    p += s * s;
  }
  /* correct for dc offset in squares */
  dc = (double)(t*step) / (double)len;
  err = t * 2 * dc - dc * dc * len;

  rms = (long int)sqrt((p-err) / len);
  if (rms < 0) rms = 150;
  return rms;
}