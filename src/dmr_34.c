/*-------------------------------------------------------------------------------
 * dmr_34.c
 * DMR (and P25) 3/4 Rate Viterbi Trellis Decoder (Improved)
 *
 * Code Sourced From: Unknown
 * No License Provided
 *
 * LWVMOBILE
 * 2026-02 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#define INRANGE         8       // Serial input is grouped in 3 bits, each input value ranges 0~7
#define INSTATIC        8       // Number of possible states in the state machine, range 0~7
#define INDATA          49      // Input 144 bits, grouped into 3-bit units → outputs 48 values (0~15) + one final 0 to determine end state
#define INSYMBOL        49      // Number of input symbols
#define PATH            64      // For the i-th (48≥i≥2) input, there are 64 branch metrics → 64 possible paths
#define SELECTPATH      8       // Among the 64 paths, select the 8 with smallest metrics, then choose the smallest one as final output
#define BITELEN         9       //

/******************************************************************************
*   Header file includes
*******************************************************************************/
#include "dsd.h"
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
*   Global variable definitions
*******************************************************************************/

uint8_t interleave[98] = {
0, 1, 8,   9, 16, 17, 24, 25, 32, 33, 40, 41, 48, 49, 56, 57, 64, 65, 72, 73, 80, 81, 88, 89, 96, 97,
2, 3, 10, 11, 18, 19, 26, 27, 34, 35, 42, 43, 50, 51, 58, 59, 66, 67, 74, 75, 82, 83, 90, 91,
4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45, 52, 53, 60, 61, 68, 69, 76, 77, 84, 85, 92, 93,
6, 7, 14, 15, 22, 23, 30, 31, 38, 39, 46, 47, 54, 55, 62, 63, 70, 71, 78, 79, 86, 87, 94, 95};

static int16_t staticX[INSTATIC][INRANGE] =
{
	{ 1,-3,-3,1,3,-1,-1,3 },
	{ -3,1,3,-1,-1,3,1,-3 },
	{ -1,3,3,-1,-3,1,1,-3 },
	{ 3,-1,-3,1,1,-3,-1,3 },
	{ -3,1,1,-3,-1,3,3,-1 },
	{ 1,-3,-1,3,3,-1,-3,1 },
	{ 3,-1,-1,3,1,-3,-3,1 },
	{ -1,3,1,-3,-3,1,3,-1 }

};

static int16_t staticY[INSTATIC][INRANGE] =
{
	{ -1,3,-1,3,-3,1,-3,1 },
	{ -1,3,-3,1,-3,1,-1,3 },
	{ -1,3,-1,3,-3,1,-3,1 },
	{ -1,3,-3,1,-3,1,-1,3 },
	{ -3,1,-3,1,-1,3,-1,3 },
	{ -3,1,-1,3,-1,3,-3,1 },
	{ -3,1,-3,1,-1,3,-1,3 },
	{ -3,1,-1,3,-1,3,-3,1 }

};


/**
* @brief   Convert 0~7 symbols into 3-bit (0/1) bitstream
*
* @Param   [in]  psymbol     Input decimal symbol values
* @Param   [in]  num         Number of input symbols
* @Param   [out] pbit        Output 3-bit stream
*
*/
void TriBit(uint16_t *psymbol, uint16_t num, uint16_t *pbit)
{
	int16_t temp, i;
	for (i = 0; i < num; i++)
	{
		temp = psymbol[i];
		*pbit++ = (temp >> 2) & 0x01;
		*pbit++ = (temp >> 1) & 0x01;
		*pbit++ = temp & 0x01;
	}
}

/**
* @brief   Organize the decoded 3-bit stream into 16-bit word format for output
*
* @Param   [in]  ptriBitDecRslt   Input 3-bit decoded bitstream
* @Param   [out] poutdata         Output packed 16-bit words
*
*/
void TriBitToShort(uint16_t *ptriBitDecRslt, uint16_t *poutdata)
{
	int16_t i, j, k = 0;
	uint16_t  bitArray[144] = { 0 };
	memset(poutdata, 0, sizeof(uint16_t) * 9);

	TriBit(ptriBitDecRslt, 48, bitArray);
	for (i = 0; i < 9; i++)
	{
		for (j = 15; j >= 0; j--)
		{
			poutdata[i] += bitArray[k++] << j;
		}
	}
}

/**
* @brief   Among 8 paths, select the one with the smallest metric and return its index
*
* @Param   [in]  pdata       Array of 8 path metrics
* @return  Index of the path with minimum metric
*
*/
uint16_t MinPath(uint16_t *pdata)
{
	uint16_t i, j = 0;
	uint16_t min = pdata[0];				// Take first value as initial minimum

	for (i = 0; i < SELECTPATH; i++)
	{
		if (pdata[i] < min)				// If current value is smaller
		{
			min = pdata[i];				// Update minimum
			j = i;
		}
	}
	return j;							// Return index of minimum path
}


/**
* @brief   From 64 paths, select the 8 paths with smallest metrics,
*          and output corresponding state and input values
*
* @Param   [in]   parrary        Array of 64 branch metrics
* @Param   [out]  pipath_ed      Sorted 8 smallest path metrics
* @Param   [out]  pstate         States corresponding to the 8 best paths
* @Param   [out]  pdata          Input values corresponding to the 8 best paths
*
*/
void PathFunc(uint16_t *parrary, uint16_t *pipath_ed, uint16_t *pstate, uint16_t *pdata)
{
	int16_t i, j, locT = 0, ValT = 0;
	uint16_t dataT[PATH] = { 0 };

	memcpy(dataT, parrary, sizeof(uint16_t)*PATH);

	for (i = 0; i < INRANGE; i++)
	{
		ValT = dataT[0];
		locT = 0;
		for (j = 1; j < PATH; j++)
		{
			if (dataT[j] < ValT)
			{
				ValT = dataT[j];
				locT = j;
			}
		}
		dataT[locT] = 10000;
		pipath_ed[i] = ValT;
		pstate[i] = locT / 8;	// quotient  → state
		pdata[i]  = locT % 8;	// remainder → input symbol
	}
}




/**
* @brief   Calculate path metric (Manhattan distance)
*
* @Param   [in] dataAX		X-coordinate of received constellation point A
* @Param   [in] dataBX		X-coordinate of reference constellation point B
* @Param   [in] dataAY		Y-coordinate of received constellation point A
* @Param   [in] dataBY		Y-coordinate of reference constellation point B
* @return	Path metric result
*
*/
uint16_t DistanceCal(int16_t dataAX, int16_t dataBX, int16_t dataAY, int16_t dataBY)
{
	uint16_t DistanceRslt = 0;
	DistanceRslt = abs(dataAX - dataBX) + abs(dataAY - dataBY);
	//DistanceRslt = sqrt((dataAX - dataBX)*(dataAX - dataBX) + (dataAY - dataBY)*(dataAY - dataBY));
	return DistanceRslt;
}

/**
* @brief   Convert 2-bit value to constellation symbol (±1, ±3)
*
* @Param   [in]  InBit		Input 2-bit value (0~3)
* @return  Constellation symbol value
*
*/
int16_t BitToSymbol(int16_t InBit)
{
	switch (InBit)
	{
	case 0:
		return 1;
	case 1:
		return 3;
	case 2:
		return -1;
	case 3:
		return -3;
	default:
		return 1;
	}
}


/**
* @brief   Map input symbols (0~15) to constellation points (±1, ±3) in I/Q
*
* @Param   [in]  pinstar	Input symbol array (0~15)
* @Param   [in]  num		Number of symbols
* @Param   [out] pdataX		Output I-channel (X) coordinates
* @Param   [out] pdataY		Output Q-channel (Y) coordinates
*
*/
void MapStar(uint16_t *pinstar, uint16_t num, int16_t *pdataX, int16_t *pdataY)
{
	int16_t i, itemp;

	for (i = 0; i < num; i++)
	{
		itemp = (pinstar[i] >> 2) & 0x03;
		*pdataX++ = BitToSymbol(itemp);
		itemp = pinstar[i] & 0x03;
		*pdataY++ = BitToSymbol(itemp);
	}
}


/**
* @brief   Viterbi decoding function — selects the most likely sequence
*
* @Param   [in]  pindata	  Input symbol sequence (values 0~15)
* @Param   [out] outdata      Decoded output (packed 16-bit words)
*
*/
void Virterbi(uint16_t *pindata, uint16_t outdata[])
{
	int16_t i, p, j = 1;
	uint16_t lastDistnc[INRANGE] = { 0 };					// Branch metrics for final input (8 values)
	uint16_t crntDistnc[64] = { 0 };						// All 64 possible branch metrics for current step
	uint16_t DecodeOut[INDATA][INRANGE] = { {0 }};			// Possible input sequences up to current step (8 survivors)
	uint16_t DecodeOutEnd[INDATA][INRANGE] = { {0 }};		// Temporary buffer for path history
	uint16_t exInDistnc[INRANGE] = { 0 };					// 8 survivor path metrics
	uint16_t triBitDecRslt[INDATA] = { 0 };				// Final decoded 3-bit symbols
	int16_t starX[INDATA] = { 0 }, starY[INDATA] = { 0 };	// Constellation point coordinates (I/Q)

	uint16_t minDistnc = 0;
	uint16_t ConstelPData[INRANGE] = { 0 };				// Input symbols leading to next states
	uint16_t ConstelPState[INSTATIC] = { 0 }; 			// Survivor states
	 
	MapStar(pindata, INDATA, starX, starY);				// Map input symbols to constellation points

	// Branch metrics for the first input symbol (starting from state 0)
	for (i = 0; i < INRANGE; i++)
	{
		ConstelPData[i] = i;							// Next state value
		DecodeOut[0][i] = i;							// Possible input symbols from state 0
		exInDistnc[i] = DistanceCal(starX[0], staticX[0][i], starY[0], staticY[0][i]);
	}

	// Compute branch metrics for each subsequent input symbol
	for (j = 1; j < INDATA - 1; j++)					// for each input symbol
	{
		for (p = 0; p < INSTATIC; p++)					// for each current survivor state
		{
			for (i = 0; i < INRANGE; i++)				// for each possible input
			{
				crntDistnc[p * 8 + i] = DistanceCal(starX[j], staticX[ConstelPData[p]][i], starY[j], staticY[ConstelPData[p]][i]) + exInDistnc[p];
			}
		}

		PathFunc(crntDistnc, exInDistnc, ConstelPState, ConstelPData);	// Select 8 best paths

		for (p = 0; p <= j; p++)
		{
			for (i = 0; i < INRANGE; i++)
			{
				if (p < j)
				{
					DecodeOutEnd[p][i] = DecodeOut[p][ConstelPState[i]];
				}
				else
				{
					DecodeOut[p][i] = ConstelPData[i];
					DecodeOutEnd[p][i] = DecodeOut[p][i];
				}
			}
		}
		memcpy(DecodeOut, DecodeOutEnd, j * 8 * sizeof(int16_t));
	}

	// Final step — force ending in state 0
	for (i = 0; i < INRANGE; i++)
	{
		lastDistnc[i] = DistanceCal(starX[INDATA - 1], staticX[DecodeOut[INDATA - 2][i]][0], starY[INDATA - 1], staticY[DecodeOut[INDATA - 2][i]][0]) + exInDistnc[i];
	}

	minDistnc = MinPath(lastDistnc);						// Find best final path

	for (i = 0; i < INDATA - 1; i++)
	{
		triBitDecRslt[i] = DecodeOutEnd[i][minDistnc];
	}

	TriBitToShort(triBitDecRslt, outdata);
}

uint32_t viterbi_r34 (uint8_t * input, uint8_t * output)
{

  int i = 0;
  uint32_t irr_err = 0; //irrecoverable errors

  uint8_t deinterleaved_dibits[98];
  memset (deinterleaved_dibits, 0, sizeof(deinterleaved_dibits));

  //deinterleave our input dibits
  for (i = 0; i < 98; i++)
    deinterleaved_dibits[interleave[i]] = input[i];

  //pack the input into nibbles (dibit pairs)
  uint8_t nibs[49];
  memset (nibs, 0, sizeof(nibs));

  for (i = 0; i < 49; i++)
    nibs[i] = (deinterleaved_dibits[i*2+0] << 2) | (deinterleaved_dibits[i*2+1] << 0);

  uint16_t pindata[49]; memset (pindata, 0, sizeof(pindata));
  uint16_t outdata[18]; memset (outdata, 0, sizeof(outdata));

  for (int i = 0; i < 49; i++)
    pindata[i] = (uint16_t)nibs[i];

  Virterbi(pindata, outdata);

  int k = 0;
  for (int i = 0; i < 9; i++)
  {
    output[k++] = (outdata[i] >> 8) & 0xFF;
    output[k++] = (outdata[i] >> 0) & 0xFF;
  }

  return irr_err; //TODO: Need to pull viterbi metrics from upper functions
}
