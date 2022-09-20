//remove below ifdef if problems arise
#if __cplusplus > 199711L
#define register      // Deprecated in C++11.
#endif  // #if __cplusplus > 199711L
//end ifdef to fix compiler warnings

#include "p25p1_check_ldu.h"

#include "Hamming.hpp"
#include "ReedSolomon.hpp"

// Uncomment for very verbose trace messages
//#define CHECK_LDU_DEBUG

// The following methods are just a C bridge for the C++ implementations of the Golay and ReedSolomon
// algorithms.

static Hamming_10_6_3_TableImpl hamming;
static DSDReedSolomon_24_12_13 reed_solomon_24_12_13;
static DSDReedSolomon_24_16_9 reed_solomon_24_16_9;

int check_and_fix_hamming_10_6_3(char* hex, char* parity)
{
    return hamming.decode(hex, parity);
}

void encode_hamming_10_6_3(char* hex, char* out_parity)
{
    hamming.encode(hex, out_parity);
}

int check_and_fix_reedsolomon_24_12_13(char* data, char* parity)
{
#ifdef CHECK_LDU_DEBUG
    char original[12][6];
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 6; j++) {
            original[i][j] = data[i*6+j];
        }
    }
#endif

    int irrecoverable_error = reed_solomon_24_12_13.decode(data, parity);

#ifdef CHECK_LDU_DEBUG
    fprintf (stderr, "Results for Reed-Solomon code (24,12,13)\n\n");
    if (irrecoverable_error == 0) {
        fprintf (stderr, "  i  original fixed\n");
        for (int i = 0; i < 12; i++) {
            fprintf (stderr, "%3d  [", i);
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (original[i][j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "] [");
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (data[i*6+j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "]\n");
        }
    } else {
        fprintf (stderr, "Irrecoverable errors found\n");
        fprintf (stderr, "  i  original fixed\n");
        for (int i = 0; i < 12; i++) {
            fprintf (stderr, "%3d  [", i);
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (original[i][j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "]\n");
        }
    }
    fprintf (stderr, "\n");
#endif

    return irrecoverable_error;
}

void encode_reedsolomon_24_12_13(char* hex_data, char* fixed_parity)
{
    reed_solomon_24_12_13.encode(hex_data, fixed_parity);
}

int check_and_fix_reedsolomon_24_16_9(char* data, char* parity)
{
#ifdef CHECK_LDU_DEBUG
    char original[16][6];
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 6; j++) {
            original[i][j] = data[i*6+j];
        }
    }
#endif

    int irrecoverable_error = reed_solomon_24_16_9.decode(data, parity);

#ifdef CHECK_LDU_DEBUG
    fprintf (stderr, "Results for Reed-Solomon code (24,16,9)\n\n");
    if (irrecoverable_error == 0) {
        fprintf (stderr, "  i  original fixed\n");
        for (int i = 0; i < 16; i++) {
            fprintf (stderr, "%3d  [", i);
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (original[i][j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "] [");
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (data[i*6+j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "]\n");
        }
    } else {
        fprintf (stderr, "Irrecoverable errors found\n");
        fprintf (stderr, "  i  original fixed\n");
        for (int i = 0; i < 16; i++) {
            fprintf (stderr, "%3d  [", i);
            for (int j = 0; j < 6; j++) {
                fprintf (stderr, "%c", (original[i][j] == 1)? 'X' : ' ');
            }
            fprintf (stderr, "]\n");
        }
    }
    fprintf (stderr, "\n");
#endif

    return irrecoverable_error;
}

void encode_reedsolomon_24_16_9(char* hex_data, char* fixed_parity)
{
    reed_solomon_24_16_9.encode(hex_data, fixed_parity);
}
