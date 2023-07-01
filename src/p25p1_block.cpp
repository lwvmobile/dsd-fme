/*-------------------------------------------------------------------------------
 * p25p1_block.c
 * P25 Block Bridge for TSBK Deinterleaving, CRC12 and CRC16 checks
 *
 * original copyrights for portions used below (OP25, Ossman, and LEH)
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <vector>

typedef std::vector<bool> bit_vector;

/* adapted from wireshark/plugins/p25/packet-p25cai.c */
/* Copyright 2008, Michael Ossmann <mike@ossmann.com>  */
/* deinterleave and trellis1_2 decoding, count_bits, and find_min*/
/* buf is assumed to be a buffer of 12 bytes */

//typedef std::vector<bool> bit_vector;

//ugly copy and paste below
static int count_bits(unsigned int n) {
            int i = 0;
            for (i = 0; n != 0; i++)
                n &= n - 1;
            return i;
        }

static int find_min(uint8_t list[], int len) {
            int min = list[0];
            int index = 0;
            int unique = 1;
            int i;

            for (i = 1; i < len; i++) {
                if (list[i] < min) {
                    min = list[i];
                    index = i;
                    unique = 1;
                } else if (list[i] == min) {
                    //unique = 0;
                    ; //do nothing, still can be correct and pass CRC
                }
            }
            /* return -1 if a minimum can't be found */
            if (!unique)
                return -1;

            return index;
        }

static int block_deinterleave(bit_vector& bv, unsigned int start, uint8_t* buf)
{
  static const uint16_t deinterleave_tb[] = {
                0,  1,  2,  3,  52, 53, 54, 55, 100,101,102,103, 148,149,150,151,
                4,  5,  6,  7,  56, 57, 58, 59, 104,105,106,107, 152,153,154,155,
                8,  9, 10, 11,  60, 61, 62, 63, 108,109,110,111, 156,157,158,159,
                12, 13, 14, 15,  64, 65, 66, 67, 112,113,114,115, 160,161,162,163,
                16, 17, 18, 19,  68, 69, 70, 71, 116,117,118,119, 164,165,166,167,
                20, 21, 22, 23,  72, 73, 74, 75, 120,121,122,123, 168,169,170,171,
                24, 25, 26, 27,  76, 77, 78, 79, 124,125,126,127, 172,173,174,175,
                28, 29, 30, 31,  80, 81, 82, 83, 128,129,130,131, 176,177,178,179,
                32, 33, 34, 35,  84, 85, 86, 87, 132,133,134,135, 180,181,182,183,
                36, 37, 38, 39,  88, 89, 90, 91, 136,137,138,139, 184,185,186,187,
                40, 41, 42, 43,  92, 93, 94, 95, 140,141,142,143, 188,189,190,191,
                44, 45, 46, 47,  96, 97, 98, 99, 144,145,146,147, 192,193,194,195,
                48, 49, 50, 51 };

            uint8_t hd[4];
            int b, d, j;
            int state = 0;
            uint8_t codeword;

            static const uint8_t next_words[4][4] = {
                {0x2, 0xC, 0x1, 0xF},
                {0xE, 0x0, 0xD, 0x3},
                {0x9, 0x7, 0xA, 0x4},
                {0x5, 0xB, 0x6, 0x8}
            };

            memset(buf, 0, 12);

            for (b=0; b < 98*2; b += 4) {
                codeword = (bv[start+deinterleave_tb[b+0]] << 3) +
                    (bv[start+deinterleave_tb[b+1]] << 2) +
                    (bv[start+deinterleave_tb[b+2]] << 1) +
                    bv[start+deinterleave_tb[b+3]]     ;

                /* try each codeword in a row of the state transition table */
                for (j = 0; j < 4; j++) {
                    /* find Hamming distance for candidate */
                    hd[j] = count_bits(codeword ^ next_words[state][j]);
                }
                /* find the dibit that matches the most codeword bits (minimum Hamming distance) */
                state = find_min(hd, 4);
                /* error if minimum can't be found */
                if(state == -1)
                    return -1;	// decode error, return failure
                /* It also might be nice to report a condition where the minimum is
                 * non-zero, i.e. an error has been corrected.  It probably shouldn't
                 * be a permanent failure, though.
                 *
                 * DISSECTOR_ASSERT(hd[state] == 0);
                 */

                /* append dibit onto output buffer */
                d = b >> 2;	// dibit ctr
                if (d < 48) {
                    buf[d >> 2] |= state << (6 - ((d%4) * 2));
                }
            }
            return 0;
        }

int bd_bridge (int payload[196], uint8_t decoded[12])
{
  std::vector<bool> vc(196,0);
  int ec = 69; //initialize error value

  //gather payload from tsbk handler and pass it to the block_deinterleave function
  //and then return its payload back to the tsbk handler

  //initialize our decoded byte buffer with zeroes
  for (int i = 0; i < 12; i++)
  {
    decoded[i] = 0;
  }

  //convert/load payload into a vector vc to pass to block_deinterleave
  for (int i = 0; i < 196; i++)
  {
    vc[i] = payload[i];
  }
  int block_count = 0; //
  unsigned int start = block_count*196;

  ec = block_deinterleave(vc, start, decoded);

  return ec;
}

//modified from the LEH ComputeCrcCCITT to accept variable len buffer bits
uint16_t ComputeCrcCCITT16b(const uint8_t buf[], unsigned int len)
{
  uint32_t i;
  uint16_t CRC = 0x0000; /* Initialization value = 0x0000 */
  /* Polynomial x^16 + x^12 + x^5 + 1
   * Normal     = 0x1021
   * Reciprocal = 0x0811
   * Reversed   = 0x8408
   * Reversed reciprocal = 0x8810 */
  uint16_t Polynome = 0x1021;
  for(i = 0; i < len; i++)
  {
    if(((CRC >> 15) & 1) ^ (buf[i] & 1))
    {
      CRC = (CRC << 1) ^ Polynome;
    }
    else
    {
      CRC <<= 1;
    }
  }

  /* Invert the CRC */
  CRC ^= 0xFFFF;

  /* Return the CRC */
  return CRC;
} /* End ComputeCrcCCITT() */


//modified from crc12_ok to run a quickie on 16 instead
static uint16_t crc16_ok(const uint8_t bits[], unsigned int len) {
	uint16_t crc = 0;
  // fprintf (stderr, "\n  LEN = %d", len);
  for (int i = 0; i < 16; i++)
  {
    crc = crc << 1;
    crc = crc | bits[i+len];
  }
  // uint16_t check = crc16b(bits,len);
  uint16_t check = ComputeCrcCCITT16b(bits, len);

  if (crc == check)
  {
    //fprintf (stderr, " CRC = %04X %04X", crc, check);
    return (0);
  }
  else
  {
    //fprintf (stderr, " CRC = %04X %04X", crc, check);
    return (-1);
  }

}

//TSBK/LCCH CRC16 x-bit bridge to crc16b and crc16b_okay, wip, may need multi pdu format as well
int crc16_lb_bridge (int payload[190], int len)
{
  int err = -2;
  uint8_t buf[190] = {0};

  for (int i = 0; i < len+16; i++) //add +16 here so we load the entire frame but only run crc on the len portion
  {
    buf[i] = payload[i];
  }
  err = crc16_ok(buf, len);
  return (err);
}

//borrowing crc12 from OP25
static uint16_t crc12(const uint8_t bits[], unsigned int len) {
	uint16_t crc=0;
	static const unsigned int K = 12;
  //g12(x) = x12 + x11 + x7 + x4 + x2 + x + 1
	static const uint8_t poly[K+1] = {1,1,0,0,0,1,0,0,1,0,1,1,1}; // p25 p2 crc 12 poly
	uint8_t buf[256];
	if (len+K > sizeof(buf)) {
		//fprintf (stderr, "crc12: buffer length %u exceeds maximum %u\n", len+K, sizeof(buf));
		return 0;
	}
	memset (buf, 0, sizeof(buf));
	for (unsigned int i=0; i<len; i++){
		buf[i] = bits[i];
	}
	for (unsigned int i=0; i<len; i++)
		if (buf[i])
			for (unsigned int j=0; j<K+1; j++)
				buf[i+j] ^= poly[j];
	for (unsigned int i=0; i<K; i++){
		crc = (crc << 1) + buf[len + i];
	}
	return crc ^ 0xfff;
}

//borrowing crc12_ok from OP25
static uint16_t crc12_ok(const uint8_t bits[], unsigned int len) {
	uint16_t crc = 0;
  for (int i = 0; i < 12; i++)
  {
    crc = crc << 1;
    crc = crc | bits[i+len];
  }
  uint16_t check = crc12(bits,len);

  if (crc == check)
  {
    //fprintf (stderr, " CRC = %03X %03X", crc, check);
    return (0);
  }
  else
  {
    //fprintf (stderr, " CRC = %03X %03X", crc, check);
    return (-1);
  }

}

//xCCH CRC12 x-bit bridge to crc12b and crc12b_okay
int crc12_xb_bridge (int payload[190], int len)
{
  int err = -5;
  uint8_t buf[190] = {0};

  //need to load up the 12 crc bits as well, else it will fail on the check, but not on the crc calc
  for (int i = 0; i < len+12; i++)
  {
    buf[i] = payload[i];
  }
  err = crc12_ok(buf, len);
  return (err);
}
