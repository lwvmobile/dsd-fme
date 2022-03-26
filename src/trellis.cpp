/*
 *	Copyright (C) 2016 by Jonathan Naylor, G4KLX
 *
 *	Modifications of original code to work with OP25
 *	Copyright (C) 2019 by Graham J. Norbury
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
//#include "trellis.h"

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

const unsigned char BIT_MASK_TABLE[] = {0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U};

#define READ_BIT(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])
#define WRITE_BIT(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])

//unsigned char state = 0U;
//unsigned int superposition = 0;
//CDMRTrellis::CDMRTrellis()
//{
//}

//CDMRTrellis::~CDMRTrellis()
//{
//}

//void CDMRTrellis::tribitsToBits(const unsigned char* tribits, unsigned char* payload) const
//void CDMRTrellisTribitsToBits(const unsigned char* tribits, unsigned char* payload) const
//bool CDMRTrellis::fixCode(unsigned char* points, unsigned int failPos, unsigned char* payload) const
//bool CDMRTrellisFixCode(unsigned char* points, unsigned int failPos, unsigned char* payload) const
bool CDMRTrellisFixCode(unsigned char* points, unsigned int failPos, unsigned char* payload)

{
	//unsigned int bestPos = 0U;
	//unsigned int bestVal = 0U;

	for (unsigned j = 0U; j < 20U; j++) {
		unsigned int bestPos = 0U;
		unsigned int bestVal = 0U;
		fprintf (stderr, "  fail%d-", failPos);

		for (unsigned int i = 0U; i < 16U; i++) {
			points[failPos] = i;

			fprintf (stderr, "i%d", i);

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
	//g4klx version below
	/*
	for (unsigned int i = 0U; i < 48U; i++) {
		unsigned char tribit = tribits[i];

	  bool b1 = (tribit & 0x04U) == 0x04U;
		bool b2 = (tribit & 0x02U) == 0x02U;
		bool b3 = (tribit & 0x01U) == 0x01U;

		unsigned int n = 143U - i * 3U;
		/*
		WRITE_BIT(payload, n, b1);
			n--;
		WRITE_BIT(payload, n, b2);
			n--;
		WRITE_BIT(payload, n, b3);
	}
	*/

	// boatbod version below...
	// convert tribits to bits

	unsigned char bits[144];
	for (int i = 0; i < 48; i++) {
		bits[(i * 3) + 0] = (tribits[i] >> 2) & 1;
		bits[(i * 3) + 1] = (tribits[i] >> 1) & 1;
		bits[(i * 3) + 2] = tribits[i] & 1;
	}


	// convert bits to bytes

	for (int i = 0; i < 144; i++)
		payload[i/8] = (payload[i/8] << 1) | bits[i];

}

//unsigned int CDMRTrellis::checkCode(const unsigned char* points, unsigned char* tribits) const
//unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* tribits) const
/*
unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* tribits)

{
	//replace i with superposition; revert back if totally borked
	//unsigned char state = 0U;

	//for (unsigned int i = 0U; i < 49U; i++) {
		//tribits[i] = 9U;
		for (superposition; superposition < 49U; superposition++) {
			tribits[superposition] = 9U;
		for (unsigned int j = 0U; j < 8U; j++) {
			if (points[superposition] == ENCODE_TABLE[state * 8U + j]) {
				tribits[superposition] = j;
				break;
			}
		}

		if (tribits[superposition] == 9) {
			return superposition;
			//experiment placing fixcode right in here instead
			//or we need to find a way to update state outside of this function
			}
		state = tribits[superposition];

	}
	//if (tribits[48U] != 0U)
		//return 48U;

	return 999U;
}
*/
unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* tribits)
{
	unsigned char state = 0U;

	for (unsigned int i = 0U; i < 49U; i++) {
		tribits[i] = 9U;

		for (unsigned int j = 0U; j < 8U; j++) {
			if (points[i] == ENCODE_TABLE[state * 8U + j]) {
				tribits[i] = j;
				break;
			}
		}

		if (tribits[i] == 9U)
			return i;

		state = tribits[i];
	}

	if (tribits[48U] != 0U)
		return 48U;

	return 999U;
}

//void CDMRTrellis::deinterleave(const unsigned char* data, signed char* dibits) const
//void CDMRTrellisDeinterleave(const unsigned char* data, signed char* dibits) const
void CDMRTrellisDeinterleave(const unsigned char* data, signed char* dibits)
{
	int n;
	signed char dibit;

	for (int i = 0; i < 98; i++) {
		n = i * 2;
		if (n >= 98) n += 68;
		bool b1 = data[n] != 0x0;

		n = i * 2 + 1;
		if (n >= 98) n += 68;
		bool b2 = data[n] != 0x0;

		if (!b1 && b2)
			dibit = +3;
		else if (!b1 && !b2)
			dibit = +1;
		else if (b1 && !b2)
			dibit = -1;
		else
			dibit = -3;

		n = INTERLEAVE_TABLE[i];
		dibits[n] = dibit;
	}
}

//void CDMRTrellis::dibitsToPoints(const signed char* dibits, unsigned char* points) const
//void CDMRTrellisDibitsToPoints(const signed char* dibits, unsigned char* points) const
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

//void CDMRTrellis::pointsToDibits(const unsigned char* points, signed char* dibits) const
//void CDMRTrellisPointsToDibits(const unsigned char* points, signed char* dibits) const
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

//void CDMRTrellis::bitsToTribits(const unsigned char* payload, unsigned char* tribits) const
//void CDMRTrellisBitsToTribits(const unsigned char* payload, unsigned char* tribits) const
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

//bool CDMRTrellis::decode(const unsigned char* data, unsigned char* payload)
bool CDMRTrellisDecode(const unsigned char* data, unsigned char* payload)
{
	assert(data != NULL);
	assert(payload != NULL);
	if (1 == 1)
  {
    fprintf (stderr, "\n Raw Trellis Dibits inside Decoder\n  ");
    for (short i = 0; i < 98; i++)
    {
      fprintf (stderr, "[%0d]", data[i]);
    }
  }
	signed char dibits[98U];
	CDMRTrellisDeinterleave(data, dibits);
	if (1 == 1)
  {
    fprintf (stderr, "\n Trellis Deinterleaved Dibits inside Decoder (converted to signed)\n  ");
    for (short i = 0; i < 98; i++)
    {
      fprintf (stderr, "[%d]", dibits[i]); //signed char, values should be -3, -1, 1, 3 only
    }
  }
	unsigned char points[49U];
	CDMRTrellisDibitsToPoints(dibits, points);
	if (1 == 1)
  {
    fprintf (stderr, "\n Trellis Points inside Decoder (no higher than 15U?)\n  ");
    for (short i = 0; i < 49; i++)
    {
      fprintf (stderr, "[%02u]", points[i]);
    }
  }
	// Check the original code
	unsigned char tribits[49U];
	unsigned int failPos = CDMRTrellisCheckCode(points, tribits);




	if (failPos == 999U) {
		CDMRTrellisTribitsToBits(tribits, payload);
		if (1 == 1)
		{
		 fprintf (stderr, "\nTrellis Payload Tribits to Bits failPos == 999U\n  ");
		 for (short i = 0; i < 18; i++)
		 {
		   fprintf (stderr, "[%02u]", payload[i]);
		 }
		}
		return true;
	}

	unsigned char savePoints[49U];
	for (unsigned int i = 0U; i < 49U; i++)
		savePoints[i] = points[i];

	bool ret = CDMRTrellisFixCode(points, failPos, payload);
	if (ret){

		if (1 == 1)
	  {
	    fprintf (stderr, "\n Trellis Tribits inside Decoder\n  ");
	    for (short i = 0; i < 49; i++)
	    {
	      fprintf (stderr, "[%0u]", tribits[i]); //shouldn't have any values greater that 0x7?
	    }
	  }

		if (1 == 1)
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
	if (1 == 1)
	{
	 fprintf (stderr, "\nTrellis Payload Tribits to Bits failPos == 0U\n  ");
	 for (short i = 0; i < 18; i++)
	 {
		 fprintf (stderr, "[%02X]", payload[i]);
	 }
	}
		return false; }

	// Backtrack one place for a last go
	return CDMRTrellisFixCode(savePoints, failPos - 1U, payload);
}
