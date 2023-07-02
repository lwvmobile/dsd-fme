/*
 *	Copyright (C) 2016 by Jonathan Naylor, G4KLX
 *
 *	Heavy Modificatins of Original Code to work for DSD-FME
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */
#include "dsd.h"

#include <cstdio>
#include <cassert>

const unsigned int INTERLEAVE_TABLE[] = {
	0U, 1U, 8U,   9U, 16U, 17U, 24U, 25U, 32U, 33U, 40U, 41U, 48U, 49U, 56U, 57U, 64U, 65U, 72U, 73U, 80U, 81U, 88U, 89U, 96U, 97U,
	2U, 3U, 10U, 11U, 18U, 19U, 26U, 27U, 34U, 35U, 42U, 43U, 50U, 51U, 58U, 59U, 66U, 67U, 74U, 75U, 82U, 83U, 90U, 91U,
	4U, 5U, 12U, 13U, 20U, 21U, 28U, 29U, 36U, 37U, 44U, 45U, 52U, 53U, 60U, 61U, 68U, 69U, 76U, 77U, 84U, 85U, 92U, 93U,
	6U, 7U, 14U, 15U, 22U, 23U, 30U, 31U, 38U, 39U, 46U, 47U, 54U, 55U, 62U, 63U, 70U, 71U, 78U, 79U, 86U, 87U, 94U, 95U};

const unsigned char ENCODE_TABLE[] = {
	0U,  8U, 4U, 12U, 2U, 10U, 6U, 14U,
	4U, 12U, 2U, 10U, 6U, 14U, 0U,  8U,
	1U,  9U, 5U, 13U, 3U, 11U, 7U, 15U,
	5U, 13U, 3U, 11U, 7U, 15U, 1U,  9U,
	3U, 11U, 7U, 15U, 1U,  9U, 5U, 13U,
	7U, 15U, 1U,  9U, 5U, 13U, 3U, 11U,
	2U, 10U, 6U, 14U, 0U,  8U, 4U, 12U,
	6U, 14U, 0U,  8U, 4U, 12U, 2U, 10U};

const unsigned char DECODE_TABLE[] = { //flip the rows and columns, right?
	0,4,1,5,3,7,2,6,
	8,12,9,13,11,15,10,14,
	4,2,5,3,7,1,6,0,
	12,10,13,11,15,9,14,8,
	2,6,3,7,1,5,0,4,
	10,14,11,15,9,13,8,12,
	6,0,7,1,5,3,4,2,
	14,8,15,9,13,11,12,10
};

const unsigned char BIT_MASK_TABLE[] = {0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U};

#define READ_BIT(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])
#define WRITE_BIT(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])

unsigned char state = 0U;
//rework quickHamming to accept unsigned intergers instead and go bit by bit
int quickHamming(unsigned int butt, unsigned int farts)
{
    unsigned int i = 0, count = 0, shit = 0, shart = 0;
		//fprintf (stderr, "Distance Compare %X - %X \n", butt, farts );
		for (i; i < 3; i++) //shift three places tops;
		{
			shit  = (butt  >> i)  & 0b1;
			shart = (farts >> i)  & 0b1;
			if (shit != shart)
			{
				count++;
			}
		}

    return count;
}

bool CDMRTrellisFixCode(unsigned char* points, unsigned int failPos, unsigned char* payload)
{
	//unsigned int bestPos = 0U;
	//unsigned int bestVal = 0U;

	for (unsigned j = 0U; j < 20U; j++) {
		unsigned int bestPos = 0U;
		unsigned int bestVal = 0U;
		//fprintf (stderr, "  fail%d-", failPos);

		for (unsigned int i = 0U; i < 16U; i++) {
			points[failPos] = i;

			//fprintf (stderr, "i%d", i);

			unsigned char tribits[49U];
			unsigned int pos = CDMRTrellisCheckCode(points, tribits);
			if (pos == 999U) {
				CDMRTrellisTribitsToBits(tribits, payload);
				return true;
			}

			if (pos > bestPos) {
				bestPos = pos;
				bestVal = i;
			}
		}

		points[failPos] = bestVal;
		failPos = bestPos;
	}

	return false;
}

void CDMRTrellisTribitsToBits(const unsigned char* tribits, unsigned char* payload)
{


	unsigned char bits[144];
	for (int i = 0; i < 48; i++) {
		bits[(i * 3) + 0] = (tribits[i] >> 2) & 1;
		bits[(i * 3) + 1] = (tribits[i] >> 1) & 1;
		bits[(i * 3) + 2] = tribits[i] & 1;
	}


	// convert bits to bytes
	//fprintf (stderr, "\n Tribit Binary Sequence\n  ");
	for (short i = 0; i < 144; i++)
	{
		//fprintf (stderr, "%b", bits[i]);
	}
	for(short int i = 0; i < 18; i++)
  {
		payload[i] = (bits[i * 8 + 0] << 7) | (bits[i * 8 + 1] << 6) | (bits[i * 8 + 2] << 5) | (bits[i * 8 + 3] << 4) |
								 (bits[i * 8 + 4] << 3) | (bits[i * 8 + 5] << 2) | (bits[i * 8 + 6] << 1) | (bits[i * 8 + 7] << 0);
	}

	//for (int i = 0; i < 144; i++)
		//payload[i/8] = (payload[i/8] << 1) | bits[i];
		//DmrDataBit[j + 0] = (TrellisReturn[17-i] >> 0) & 0x01; //example

}

unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* tribits)
{

	//unsigned char state = 0U; //move outside of function to preserve state for every iteration
	//let's try reversing the points to pass through the decode table instead of flipping them here,
	//it may mess up the fixcode and failpos POS
	state = 0U;
	for (unsigned int i = 0U; i < 49U;) //total number of points to process
	{
		tribits[i] = 9U;
		for (unsigned int j = 0U; j < 8U; j++)
		{
			//if (points[i] == DECODE_TABLE[state * 8U + j])
			//if (points[48-i] == DECODE_TABLE[state * 8U + j])
			if (points[i] == ENCODE_TABLE[state * 8U + j])
			{
				tribits[i] = j;
				state = tribits[i]; //carry over if found right away
				i++; //increment i by one value
				break;
			}
		}
		//try to find shortest distance for surviving paths and decide on one
		//seems to be that two paths will have distance value 1, and two will have distance value 2
		//need to keep totaling up a few rounds before we can eliminate a path and resume?
		if (tribits[i] == 9U)
		{
			int    distance  =  0;
			int maxdistance  = 99; //random metric for two position distance
			int pathdistance =  0;
			unsigned int match     = 0;
			unsigned int nextmatch = 0;
			unsigned int q = 0;
			//only 4 possible states for each point, but in 8 (0-7) FSM states
			char path[99][99] = {0}; //[i][pathcount] = distance
			int pathcount = 0;
			for (unsigned int o = 0; o < 64; o++) //check entire state transition table for matches
			{
				if (points[i] == ENCODE_TABLE[o]) //ENCODE TABLE
				{
					match = o / (unsigned int)8; //get dividend no remainder/decimal I hope, get fsm state 0-7
					//fprintf (stderr, "\n Point #%d [%d] Match State %d Position %d Tribit Value %d \n", i, points[i], match, o, o % (unsigned int)8);
					distance = quickHamming(match, state);
					//fprintf (stderr, "Raw Distance Current %d\n", distance);
					path[i][pathcount] = distance ; //save which table values (paths) match as possible values and distance
					for (q; q < 64; q++) //check entire state transition table for next point match
					{
						if (points[i+1] == ENCODE_TABLE[q]) //ENCODE TABLE
						{
							nextmatch = q / (unsigned int)8;
							path[i+1][pathcount] = quickHamming(nextmatch, q % (unsigned int)8);
							//fprintf (stderr, "Raw Distance Next %d\n", path[i+1][pathcount]);
							pathcount++;
							//fprintf (stderr, "State 0 Match %d Distance %d \n", o, distance);
						}
					}
					for (short int z = 0; z <= pathcount; z++) //add metrics on paths to find shortest distance
					{
						pathdistance = path[i][0] + path[i+1][z];
						//fprintf (stderr, "Total Path Distance %d\n", pathdistance);
						if (pathdistance <= maxdistance) //equal too in case of tie?
						{
							maxdistance = pathdistance;
							tribits[i]   = o % (unsigned int)8; //remainder value will be tribit value on table
							if (i < 49) //just in case we are on 49 already, don't want to crash or something.
							{
								tribits[i+1] = q % (unsigned int)8; //remainder value will be tribit value on table
							}

							//fprintf (stderr, "Point #%d [%d] - Tribit Candidate %d\n", i,   points[i],   tribits[i]   );
							//fprintf (stderr, "Point #%d [%d]- Tribit Candidate %d\n", i+1, points[i+1], tribits[i+1] );
						}
					}
				}
				//fprintf (stderr, "Selected Point #%d [%d] - Tribit Selection %d\n", i,   points[i],   tribits[i]   );
				//fprintf (stderr, "Selected Point #%d [%d]- Tribit Selection %d\n", i+1, points[i+1], tribits[i+1] );
			}
			state = tribits[i+1]; //not quite sure if this will be needed or not, if so, set state to last found value
			i += 2; //increment i by two since we already got the next tribit value as well;
			//state = tribits[i]; //not quite sure if this will be needed or not, if so, set state to last found value
			//i++; //increment i by two since we already got the next tribit value as well;
		}

	}
	return 999U;
}

void CDMRTrellisDeinterleave(const unsigned char* data, signed char* dibits)
{
	int n;
	//can only assume this is giving the correct dibits out, and not inverse or something.
	/*
	for (int i = 0; i < 98; i++) {
		n = i * 2;
		if (n >= 98) n += 68;
		bool b1 = data[n] != 0x0;

		n = i * 2 + 1;
		if (n >= 98) n += 68;
		bool b2 = data[n] != 0x0;

		if (!b1 && b2)
			dibit = +3;        // 1 - 01
		else if (!b1 && !b2)
			dibit = +1;        // 0 - 00
		else if (b1 && !b2)
			dibit = -1;        // 2 - 10
		else
			dibit = -3;        // 3 - 11

		*/
		//Keep it simple!
		for (int i = 0; i < 98; i++)
		{
			n = INTERLEAVE_TABLE[97-i];
			//n = i;
			if (data[i] == 0)
			{
				dibits[n] =  1; //1
			}
			if (data[i] == 1)
			{
				dibits[n] =  3; //3
			}
			if (data[i] == 2)
			{
				dibits[n] =  -1; //-1
			}
			if (data[i] == 3)
			{
				dibits[n] = -3; //-3
			}
		}

		//n = INTERLEAVE_TABLE[i]; //should this go in reverse?
		//n = INTERLEAVE_TABLE[97-i]; //should this go in reverse?
		//dibits[n] = dibit; //should this go in reverse too? not sure on this one, but definitely the INTERLEAVE_TABLE
		//n = INTERLEAVE_TABLE[97-i]; //should this go in reverse?
		//dibits[97-n] = dibit;
	//}
}

void CDMRTrellisDibitsToPoints(const signed char* dibits, unsigned char* points)
{
	for (unsigned int i = 0U; i < 49U; i++) {
		if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == -1)
			points[i] = 0U;
		else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == -1)
			points[i] = 1U;
		else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == -3)
			points[i] = 2U;
		else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == -3)
			points[i] = 3U;
		else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == -1)
			points[i] = 4U;
		else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == -1)
			points[i] = 5U;
		else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == -3)
			points[i] = 6U;
		else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == -3)
			points[i] = 7U;
		else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == +3)
			points[i] = 8U;
		else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == +3)
			points[i] = 9U;
		else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == +1)
			points[i] = 10U;
		else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == +1)
			points[i] = 11U;
		else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == +3)
			points[i] = 12U;
		else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == +3)
			points[i] = 13U;
		else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == +1)
			points[i] = 14U;
		else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == +1)
			points[i] = 15U;
	}
}

void CDMRTrellisPointsToDibits(const unsigned char* points, signed char* dibits)
{
	for (unsigned int i = 0U; i < 49U; i++) {
		switch (points[i]) {
		case 0U:
			dibits[i * 2U + 0U] = +1;
			dibits[i * 2U + 1U] = -1;
			break;
		case 1U:
			dibits[i * 2U + 0U] = -1;
			dibits[i * 2U + 1U] = -1;
			break;
		case 2U:
			dibits[i * 2U + 0U] = +3;
			dibits[i * 2U + 1U] = -3;
			break;
		case 3U:
			dibits[i * 2U + 0U] = -3;
			dibits[i * 2U + 1U] = -3;
			break;
		case 4U:
			dibits[i * 2U + 0U] = -3;
			dibits[i * 2U + 1U] = -1;
			break;
		case 5U:
			dibits[i * 2U + 0U] = +3;
			dibits[i * 2U + 1U] = -1;
			break;
		case 6U:
			dibits[i * 2U + 0U] = -1;
			dibits[i * 2U + 1U] = -3;
			break;
		case 7U:
			dibits[i * 2U + 0U] = +1;
			dibits[i * 2U + 1U] = -3;
			break;
		case 8U:
			dibits[i * 2U + 0U] = -3;
			dibits[i * 2U + 1U] = +3;
			break;
		case 9U:
			dibits[i * 2U + 0U] = +3;
			dibits[i * 2U + 1U] = +3;
			break;
		case 10U:
			dibits[i * 2U + 0U] = -1;
			dibits[i * 2U + 1U] = +1;
			break;
		case 11U:
			dibits[i * 2U + 0U] = +1;
			dibits[i * 2U + 1U] = +1;
			break;
		case 12U:
			dibits[i * 2U + 0U] = +1;
			dibits[i * 2U + 1U] = +3;
			break;
		case 13U:
			dibits[i * 2U + 0U] = -1;
			dibits[i * 2U + 1U] = +3;
			break;
		case 14U:
			dibits[i * 2U + 0U] = +3;
			dibits[i * 2U + 1U] = +1;
			break;
		default:
			dibits[i * 2U + 0U] = -3;
			dibits[i * 2U + 1U] = +1;
			break;
		}
	}
}

void CDMRTrellisBitsToTribits(const unsigned char* payload, unsigned char* tribits)
{
	for (unsigned int i = 0U; i < 48U; i++) {
		unsigned int n = 143U - i * 3U;

		bool b1 = READ_BIT(payload, n) != 0x00U;
		n--;
		bool b2 = READ_BIT(payload, n) != 0x00U;
		n--;
		bool b3 = READ_BIT(payload, n) != 0x00U;

		unsigned char tribit = 0U;
		tribit |= b1 ? 4U : 0U;
		tribit |= b2 ? 2U : 0U;
		tribit |= b3 ? 1U : 0U;

		//tribits[i] = tribit;
	}

	tribits[48U] = 0U;
}

bool CDMRTrellisDecode(const unsigned char* data, unsigned char* payload)
{
	assert(data != NULL);
	assert(payload != NULL);
	unsigned char tribits[49U];
	for (short i = 0; i < 49; i++)
	{
		tribits[i] = 0; //initialize with 0 values so we know later on if these values are actually being assigned or not
	}

	if (1 == 2)
  {
    fprintf (stderr, "\n Raw Trellis Dibits inside Decoder\n  ");
    for (short i = 0; i < 98; i++)
    {
      fprintf (stderr, "#%d [%0d] ", i, data[i]);
    }
  }
	signed char dibits[98U];
	CDMRTrellisDeinterleave(data, dibits);
	if (1 == 2)
  {
    fprintf (stderr, "\n Trellis Deinterleaved Dibits inside Decoder (converted to signed)\n  ");
    for (short i = 0; i < 98; i++)
    {
      fprintf (stderr, "#%d [%d] ", i, dibits[i]); //signed char, values should be -3, -1, 1, 3 only
    }
  }
	unsigned char points[49U];
	// unsigned char reverse_points[49U];
	CDMRTrellisDibitsToPoints(dibits, points);
	//maybe keep point 49 at 49, since its the flush?
	//fprintf (stderr, "\n Trellis Reverse Points inside Decoder\n  ");
	// for (short i = 0; i < 49; i++)
	// {
	// 	reverse_points[i] = points[48-i];
	// 	//fprintf (stderr, "#%d [%02u] ", i, reverse_points[i]);
	// }
	//reverse_points[48] = points[48];
	//fprintf (stderr, "#%d [%02u] ", 48, reverse_points[48]);

	if (1 == 2)
  {
    fprintf (stderr, "\n Trellis Points inside Decoder\n  ");
    for (short i = 0; i < 49; i++)
    {
      fprintf (stderr, "#%d [%02u] ", i, points[i]);
    }
  }
	// Check the original code

	unsigned int failPos = CDMRTrellisCheckCode(points, tribits);
	//unsigned int failPos = CDMRTrellisCheckCode(reverse_points, tribits); //stack smashing detected terminated

	if (1 == 2)
	{
		fprintf (stderr, "\n Check Code Trellis Tribits inside Decoder\n  ");
		for (short i = 0; i < 49; i++)
		{
			fprintf (stderr, "#%d [%0u] ", i, tribits[i]); //shouldn't have any values greater that 0x7?
		}
	}

	if (failPos == 999U)
	{
		CDMRTrellisTribitsToBits(tribits, payload);
		if (1 == 2)
		{
		 fprintf (stderr, "\nTrellis Payload Tribits\n %s ", KCYN);
		 for (short i = 0; i < 49; i++)
		 {
		   fprintf (stderr, "#%d [%0u] ", i, tribits[i]); //shouldn't have any values greater that 0x7?
		 }
		}
		if (1 == 2)
		{
		 fprintf (stderr, "\nTrellis Payload Tribits to Bytes inside Decoder\n %s ", KCYN);
		 for (short i = 0; i < 18; i++)
		 {
			 fprintf (stderr, "[%02X]", payload[i]);
		 }
		 fprintf (stderr,"%s", KNRM); //change back to normal
		}
		return true;
	}

	unsigned char savePoints[49U];
	for (unsigned int i = 0U; i < 49U; i++)
		savePoints[i] = points[i];
		//savePoints[i] = reverse_points[i];

	bool ret = CDMRTrellisFixCode(points, failPos, payload);
	//bool ret = CDMRTrellisFixCode(reverse_points, failPos, payload);
	if (ret){

		if (1 == 2)
	  {
	    fprintf (stderr, "\n if return Trellis Tribits inside Decoder\n  ");
	    for (short i = 0; i < 49; i++)
	    {
	      fprintf (stderr, "#%d [%0u] ", i, tribits[i]); //shouldn't have any values greater that 0x7?
	    }
	  }

		if (1 == 2)
		{
		 fprintf (stderr, "\nTrellis Payload Tribits to Bits fix return true\n  "); //this one seems to be happening
		 for (short i = 0; i < 18; i++)
		 {
		   fprintf (stderr, "[%02X]", payload[i]);
		 }
		}
		return true; }

	if (failPos == 0U)
	{
	if (1 == 2)
	{
	 fprintf (stderr, "\nTrellis Payload Tribits to Bytes failPos == 0U\n  ");
	 for (short i = 0; i < 18; i++)
	 {
		 fprintf (stderr, "[%02X]", payload[i]);
	 }
	}
		return false; }

	// Backtrack one place for a last go
	return CDMRTrellisFixCode(savePoints, failPos - 1U, payload);
}
