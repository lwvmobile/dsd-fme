/*-------------------------------------------------------------------------------
 * p25_12.c
 * P25p1 1/2 Rate Simple Trellis Decoder
 *
 * LWVMOBILE
 * 2023-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

uint8_t p25_interleave[98] = {
0, 1, 8,   9, 16, 17, 24, 25, 32, 33, 40, 41, 48, 49, 56, 57, 64, 65, 72, 73, 80, 81, 88, 89, 96, 97,
2, 3, 10, 11, 18, 19, 26, 27, 34, 35, 42, 43, 50, 51, 58, 59, 66, 67, 74, 75, 82, 83, 90, 91,
4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45, 52, 53, 60, 61, 68, 69, 76, 77, 84, 85, 92, 93,
6, 7, 14, 15, 22, 23, 30, 31, 38, 39, 46, 47, 54, 55, 62, 63, 70, 71, 78, 79, 86, 87, 94, 95};

//this is a convertion table for converting the dibit pairs into constellation points
uint8_t p25_constellation_map[16] = {
11, 12, 0, 7, 14, 9, 5, 2, 10, 13, 1, 6, 15, 8, 4, 3
};

//digitized dibit to OTA symbol conversion for reference
//0 = +1; 1 = +3; 
//2 = -1; 3 = -3; 

//finite state machine values
uint8_t p25_fsm[16] = {
0,15,12,3,
4,11,8,7,
13,2,1,14,
9,6,5,10 };

//this is a dibit-pair to trellis dibit transition matrix (SDRTrunk and Ossmann)
//when evaluating hamming distance, we want to use this xor the dibit-pair nib,
//and not the fsm xor the constellation point
uint8_t p25_dtm[16] = {
2,12,1,15,
14,0,13,3,
9,7,10,4,
5,11,6,8 };

int count_bits(uint8_t b, int slen)
{
  int i = 0; int j = 0;
  for (j = 0; j < slen; j++)
  {
    if ( (b & 1) == 1) i++;
    b = b >> 1;
  }
  return i;
}

uint8_t find_min(uint8_t list[4], int len)
{
  int min = list[0];
  uint8_t index = 0;
  int i;

  for (i = 1; i < len; i++)
  {
    if (list[i] < min)
    {
      min = list[i];
      index = (uint8_t)i;
    }

    //NOTE: Disqualifying result on uniqueness can impact decoding
    //let the CRC determine if the result is good or bad
    //its not uncommon for two values of the same min
    //distance to emerge, so its 50/50 each time
  }

  return index;
}


int p25_12(uint8_t * input, uint8_t treturn[12])
{
  int i, j;
  int irr_err = 0;

  uint8_t deinterleaved_dibits[98];
  memset (deinterleaved_dibits, 0, sizeof(deinterleaved_dibits));

  //deinterleave our input dibits
  for (i = 0; i < 98; i++)
    deinterleaved_dibits[p25_interleave[i]] = input[i];

  //pack the input into nibbles (dibit pairs)
  uint8_t nibs[49];
  memset (nibs, 0, sizeof(nibs));

  for (i = 0; i < 49; i++)
    nibs[i] = (deinterleaved_dibits[i*2+0] << 2) | (deinterleaved_dibits[i*2+1] << 0);

  //convert our dibit pairs into constellation point values
  uint8_t point[49];
  memset (point, 0xFF, sizeof(point));

  for (i = 0; i < 49; i++) 
    point[i] = p25_constellation_map[nibs[i]];

  //debug view points
  // fprintf (stderr, "\n P =");
  // for (i = 0; i < 49; i++) 
  //   fprintf (stderr, " %02d", point[i]);

  //convert constellation points into tdibit values using the FSM
  uint8_t state = 0;
  uint8_t tdibits[49]; //trellis dibits 1/2 rate
  memset (tdibits, 0xF, sizeof(tdibits));
  uint8_t hd[4]; //array of the four fsm words hamming distance
  memset (hd, 0, sizeof(hd));
  uint8_t min = 0;

  for (i = 0; i < 49; i++)
  {

    for (j = 0; j < 4; j++)
    {
      if ( p25_fsm[(state*4)+j] == point[i] )
      {
        //return our tdibit value and state for the next point
        tdibits[i] = state = (uint8_t)j;
        break;
      }
    }

    //if tdibit value is greater than 3, then we have a decoding error
    if (tdibits[i] == 0xF)
    {
      irr_err++; //increment number of errors

      //debug position of error and state value
      // fprintf (stderr, " %d:%d;", i, state);

      //this method only seems to be reliable up to 1-2 bit errors...sometimes
      for (j = 0; j < 4; j++)
        hd[j] = count_bits (  ((nibs[i] ^ p25_dtm[(state*4)+j]) & 0xF ), 4 );
      min = find_min (hd, 4);
      tdibits[i] = state = min;

      memset (hd, 0, sizeof(hd));
      min = 0;

    }

  }

  //debug view tdibits/states
  // fprintf (stderr, "\n T =");
  // for (i = 0; i < 49; i++) 
  //   fprintf (stderr, " %02d", tdibits[i]);

  //pack tdibits into return payload bytes
  for (i = 0; i < 12; i++)
    treturn[i] = (tdibits[(i*4)+0] << 6) | (tdibits[(i*4)+1] << 4) | (tdibits[(i*4)+2] << 2) | tdibits[(i*4)+3];

  //trellis point/state err tally
  // if (irr_err != 0)
  //   fprintf (stderr, " P_ERR = %d", irr_err);

  return (irr_err);
}



