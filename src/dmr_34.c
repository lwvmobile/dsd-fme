/*-------------------------------------------------------------------------------
 * dmr_34.c
 * DMR (and P25) 3/4 Rate Simple Trellis Decoder
 *
 * LWVMOBILE
 * 2023-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

uint8_t interleave[98] = {
0, 1, 8,   9, 16, 17, 24, 25, 32, 33, 40, 41, 48, 49, 56, 57, 64, 65, 72, 73, 80, 81, 88, 89, 96, 97,
2, 3, 10, 11, 18, 19, 26, 27, 34, 35, 42, 43, 50, 51, 58, 59, 66, 67, 74, 75, 82, 83, 90, 91,
4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45, 52, 53, 60, 61, 68, 69, 76, 77, 84, 85, 92, 93,
6, 7, 14, 15, 22, 23, 30, 31, 38, 39, 46, 47, 54, 55, 62, 63, 70, 71, 78, 79, 86, 87, 94, 95};

//this is a convertion table for converting the dibit pairs into constellation points
uint8_t constellation_map[16] = {
11, 12, 0, 7, 14, 9, 5, 2, 10, 13, 1, 6, 15, 8, 4, 3
};

//digitized dibit to OTA symbol conversion for reference
//0 = +1; 1 = +3; 
//2 = -1; 3 = -3; 

//finite state machine values
uint8_t fsm[64] = {
0,  8, 4, 12, 2, 10, 6, 14,
4, 12, 2, 10, 6, 14, 0,  8,
1,  9, 5, 13, 3, 11, 7, 15,
5, 13, 3, 11, 7, 15, 1,  9,
3, 11, 7, 15, 1,  9, 5, 13,
7, 15, 1,  9, 5, 13, 3, 11,
2, 10, 6, 14, 0,  8, 4, 12,
6, 14, 0,  8, 4, 12, 2, 10};

//attempt to find the surviving path, or the 'best' path available (most positions gained)
uint8_t fix_34(uint8_t * p, uint8_t state, int position)
{
  int i, j, k, counter, best_p, best_v, survivors;
  uint8_t temp_s, tri, t;

  //status of surviving paths -- debug
  int s[8];
  memset (s, 0, 8*sizeof(int));

  //assign all potentially correct points to temporary storage
  uint8_t temp_p[8];
  temp_p[0] = (p[position] ^  1) & 0xF;
  temp_p[1] = (p[position] ^  3) & 0xF;
  temp_p[2] = (p[position] ^  5) & 0xF;
  temp_p[3] = (p[position] ^  7) & 0xF;
  temp_p[4] = (p[position] ^  9) & 0xF;
  temp_p[5] = (p[position] ^ 11) & 0xF;
  temp_p[6] = (p[position] ^ 13) & 0xF;
  temp_p[7] = (p[position] ^ 15) & 0xF;

  best_p = 0; //best position
  best_v = 0; //best path value

  for (k = 0; k < 8; k++)
  {
    temp_s = state;
    counter = 0;
    tri = 0;
    for (i = position; i < 49; i++)
    {
      //assign temp t either as our temp_p, or next point of i
      if (i == position) t = temp_p[k];
      else t = p[i];

      if (tri != 0xFF) //while the path survives
      {
        tri = 0xFF;
        for (j = 0; j < 8; j++)
        {
          if ( fsm[(temp_s*8)+j] == t )
          {
            //return our tribit value and state for the next point
            tri = temp_s = (uint8_t)j;
            counter++;
            break;
          }
          
        }

        if (counter > best_p)
        {
          best_p = counter;
          best_v = k; //if we make it further on current path, assign as best path
        }

        // surviving path -- made it to 49
        if (i == 48) s[k] = 1;

      }
    }

  }

  //NOTE: If there are no surviving paths, then that means there is another bit/point err
  //Ideally, at the end of a single or multiple fixes, there should only be one survivor, but in practice,
  //I've seen up to 4 surviving paths and somehow, get a good CRC and a good LRRP or LOCN decode
  //multiple survivors seems common when the starting err position is very close to 49

  //tally number of surviving paths
  survivors = 0;
  for (k = 0; k < 8; k++)
  {
    if (s[k] == 1) survivors++;
  }

  //debug
  // fprintf (stderr, "START: %d; BEST_P: %d; BEST_V: %d; Survivors: %d; Point: %d; ", position, best_p, best_v, survivors, temp_p[best_v]);

  return temp_p[best_v]; //return the point value of the best path's starting point value
}

uint32_t dmr_34(uint8_t * input, uint8_t treturn[18])
{
  int i, j;
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

  //convert our dibit pairs into constellation point values
  uint8_t point[49];
  memset (point, 0xFF, sizeof(point));

  for (i = 0; i < 49; i++) 
    point[i] = constellation_map[nibs[i]];

  //debug view points
  // fprintf (stderr, "\n P =");
  // for (i = 0; i < 49; i++) 
  //   fprintf (stderr, " %02d", point[i]);

  //convert constellation points into tribit values using the FSM
  uint8_t state = 0;
  uint32_t tribits[49];
  memset (tribits, 0xF, sizeof(tribits));

  for (i = 0; i < 49; i++)
  {

    for (j = 0; j < 8; j++)
    {
      if ( fsm[(state*8)+j] == point[i] )
      {
        //return our tribit value and state for the next point
        tribits[i] = state = (uint8_t)j;
        break;
      }
    }

    //if tribit value is greater than 7, then we have a decoding error
    if (tribits[i] > 7)
    {
      irr_err++; //tally number of errors

      //debug point, position of error, and state value
      // fprintf (stderr, "\n P: %d, %d:%d; ", point[i], i, state);

      //attempt to fix point by looking for the best path from here
      point[i] = fix_34(point, state, i);

      //Make a hard decision and flip point to fit in current state
      // point[i] ^= 7; //lucky number 7 (0111) //fallback

      //decrement one and resume decoding
      i--;
     
    }

  }

  //debug view tribits/states
  // fprintf (stderr, "\n T =");
  // for (i = 0; i < 49; i++) 
  //   fprintf (stderr, " %02d", tribits[i]);

  //convert tribits into a return payload
  uint32_t temp = 0;

  //break into chunks of 24 bit values and shuffle into 8-bit (byte) treturn values
  for (i = 0; i < 6; i++)
  {
    temp = (tribits[(i*8)+0] << 21) + (tribits[(i*8)+1] << 18) + (tribits[(i*8)+2] << 15) + (tribits[(i*8)+3] << 12) + 
            (tribits[(i*8)+4] << 9) + (tribits[(i*8)+5] << 6)  + (tribits[(i*8)+6] << 3)  + (tribits[(i*8)+7] << 0);

    treturn[(i*3)+0] = (temp >> 16) & 0xFF;
    treturn[(i*3)+1] = (temp >>  8) & 0xFF;
    treturn[(i*3)+2] = (temp >>  0) & 0xFF;
  }

  //trellis point/state err tally
  // if (irr_err != 0)
  //   fprintf (stderr, " P_ERR = %d", irr_err);

  return (0);
}

