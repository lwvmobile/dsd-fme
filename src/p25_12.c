/*-------------------------------------------------------------------------------
 * p25_12.c
 * P25p1 1/2 Rate Viterbi Trellis Decoder (Improved)
 *
 * LWVMOBILE
 * 2026-02 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <limits.h>  // For UINT8_MAX as infinity proxy

uint8_t p25_interleave[98] = {
  0, 1, 8,   9, 16, 17, 24, 25, 32, 33, 40, 41, 48, 49, 56, 57, 64, 65, 72, 73, 80, 81, 88, 89, 96, 97,
  2, 3, 10, 11, 18, 19, 26, 27, 34, 35, 42, 43, 50, 51, 58, 59, 66, 67, 74, 75, 82, 83, 90, 91,
  4, 5, 12, 13, 20, 21, 28, 29, 36, 37, 44, 45, 52, 53, 60, 61, 68, 69, 76, 77, 84, 85, 92, 93,
  6, 7, 14, 15, 22, 23, 30, 31, 38, 39, 46, 47, 54, 55, 62, 63, 70, 71, 78, 79, 86, 87, 94, 95
};

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
  9,6,5,10
};

//this is a dibit-pair to trellis dibit transition matrix (SDRTrunk and Ossmann)
//when evaluating hamming distance, we want to use this xor the dibit-pair nib,
//and not the fsm xor the constellation point
uint8_t p25_dtm[16] = {
  2,12,1,15,
  14,0,13,3,
  9,7,10,4,
  5,11,6,8
};

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

int p25_12(uint8_t * input, uint8_t treturn[12])
{
  int i, t;
  int total_err = 0;

  uint8_t deinterleaved_dibits[98];
  memset(deinterleaved_dibits, 0, sizeof(deinterleaved_dibits));

  //deinterleave our input dibits
  for (i = 0; i < 98; i++)
    deinterleaved_dibits[p25_interleave[i]] = input[i];

  //pack the input into nibbles (dibit pairs)
  uint8_t nibs[49];
  memset(nibs, 0, sizeof(nibs));

  for (i = 0; i < 49; i++)
    nibs[i] = (deinterleaved_dibits[i*2+0] << 2) | (deinterleaved_dibits[i*2+1] << 0);

  //convert our dibit pairs into constellation point values
  uint8_t point[49];
  memset(point, 0xFF, sizeof(point));

  for (i = 0; i < 49; i++)
    point[i] = p25_constellation_map[nibs[i]];

  // Viterbi setup: 4 states, 49 stages
  // Use UINT8_MAX / 2 as proxy for infinity (max possible errors per path ~196)
  const uint8_t INF = UINT8_MAX / 2;
  uint8_t metric[2][4];  // Ping-pong: [0] prev, [1] curr
  uint8_t path[49][4];   // path[t][s] = best prev_state to reach state s at stage t
  uint8_t tdibits[49];   // Decoded trellis dibits (states along best path)

  // Initialize: Start from state 0 before first symbol, metric 0
  int curr = 0, prev = 1;
  memset(metric, 0xFF, sizeof(metric));  // Set to high value
  memset(path, 0, sizeof(path));         // Zero-init paths

  // Stage 0 (first symbol): From initial state 0 to each possible next_s
  memset(metric[curr], INF, sizeof(metric[0]));
  for (int next_s = 0; next_s < 4; next_s++) {
    int branch = count_bits(nibs[0] ^ p25_dtm[0 * 4 + next_s], 4);
    metric[curr][next_s] = (uint8_t)branch;
    path[0][next_s] = 0;  // Prev state is initial 0
    total_err += branch;  // Accumulate for return (total path errors)
  }

  // Stages 1 to 48
  for (t = 1; t < 49; t++) {
    // Swap buffers
    prev = curr;
    curr = 1 - curr;
    memset(metric[curr], INF, sizeof(metric[0]));

    for (int next_s = 0; next_s < 4; next_s++) {
      uint8_t min_dist = INF;
      uint8_t best_prev = 0;
      for (int prev_s = 0; prev_s < 4; prev_s++) {
        if (metric[prev][prev_s] == INF) continue;
        int branch = count_bits(nibs[t] ^ p25_dtm[prev_s * 4 + next_s], 4);
        uint8_t cand = (uint8_t)(metric[prev][prev_s] + branch);
        if (cand < min_dist) {
          min_dist = cand;
          best_prev = (uint8_t)prev_s;
        }
      }
      metric[curr][next_s] = min_dist;
      path[t][next_s] = best_prev;
    }
  }

  // Find final state with minimum metric
  uint8_t min_final = 0;
  uint8_t min_metric = metric[curr][0];
  for (int s = 1; s < 4; s++) {
    if (metric[curr][s] < min_metric) {
      min_metric = metric[curr][s];
      min_final = (uint8_t)s;
    }
  }

  // Traceback: Reconstruct state sequence (tdibits[t] = state after symbol t)
  tdibits[48] = min_final;
  for (t = 48; t >= 1; t--) {
    tdibits[t - 1] = path[t][tdibits[t]];
  }

  //pack tdibits into return payload bytes (use first 48 as original)
  for (i = 0; i < 12; i++)
    treturn[i] = (tdibits[(i*4)+0] << 6) | (tdibits[(i*4)+1] << 4) | (tdibits[(i*4)+2] << 2) | tdibits[(i*4)+3];

  //debug what was the cost metric on this
  // fprintf (stderr, " R12 Min Metric = %d; ", min_metric);

  // Return total Hamming errors in best path
  return (int)min_metric;
}