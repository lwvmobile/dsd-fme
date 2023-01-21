/*
 ============================================================================
 Name        : bptc.c
 Author      : MotoTRBO_Encrypted_DSD_Author
 Version     : 1.0
 Date        : 2017 - December - 11
 Copyright   : None
 Description : BPTC library source
 ============================================================================
 */


/* Include ------------------------------------------------------------------*/
#include "dsd.h"


/* Define -------------------------------------------------------------------*/


/* Global variables ---------------------------------------------------------*/
const uint8_t BPTCInterleavingIndex[196] =
{
    0, 181, 166, 151,
  136, 121, 106,  91,
   76,  61,  46,  31,
   16,   1, 182, 167,
  152, 137, 122, 107,
   92,  77,  62,  47,
   32,  17,   2, 183,
  168, 153, 138, 123,
  108,  93,  78,  63,
   48,  33,  18,   3,
  184, 169, 154, 139,
  124, 109,  94,  79,
   64,  49,  34,  19,
    4, 185, 170, 155,
  140, 125, 110,  95,
   80,  65,  50,  35,
   20,   5, 186, 171,
  156, 141, 126, 111,
   96,  81,  66,  51,
   36,  21,   6, 187,
  172, 157, 142, 127,
  112,  97,  82,  67,
   52,  37,  22,   7,
  188, 173, 158, 143,
  128, 113,  98,  83,
   68,  53,  38,  23,
    8, 189, 174, 159,
  144, 129, 114,  99,
   84,  69,  54,  39,
   24,   9, 190, 175,
  160, 145, 130, 115,
  100,  85,  70,  55,
   40,  25,  10, 191,
  176, 161, 146, 131,
  116, 101,  86,  71,
   56,  41,  26,  11,
  192, 177, 162, 147,
  132, 117, 102,  87,
   72,  57,  42,  27,
   12, 193, 178, 163,
  148, 133, 118, 103,
   88,  73,  58,  43,
   28,  13, 194, 179,
  164, 149, 134, 119,
  104,  89,  74,  59,
   44,  29,  14, 195,
  180, 165, 150, 135,
  120, 105,  90,  75,
   60,  45,  30,  15
};


const uint8_t BPTCDeInterleavingIndex[196] =
{
    0,  13,  26,  39,
   52,  65,  78,  91,
  104, 117, 130, 143,
  156, 169, 182, 195,
   12,  25,  38,  51,
   64,  77,  90, 103,
  116, 129, 142, 155,
  168, 181, 194,  11,
   24,  37,  50,  63,
   76,  89, 102, 115,
  128, 141, 154, 167,
  180, 193,  10,  23,
   36,  49,  62,  75,
   88, 101, 114, 127,
  140, 153, 166, 179,
  192,   9,  22,  35,
   48,  61,  74,  87,
  100, 113, 126, 139,
  152, 165, 178, 191,
    8,  21,  34,  47,
   60,  73,  86,  99,
  112, 125, 138, 151,
  164, 177, 190,   7,
   20,  33,  46,  59,
   72,  85,  98, 111,
  124, 137, 150, 163,
  176, 189,   6,  19,
   32,  45,  58,  71,
   84,  97, 110, 123,
  136, 149, 162, 175,
  188,   5,  18,  31,
   44,  57,  70,  83,
   96, 109, 122, 135,
  148, 161, 174, 187,
    4,  17,  30,  43,
   56,  69,  82,  95,
  108, 121, 134, 147,
  160, 173, 186,   3,
   16,  29,  42,  55,
   68,  81,  94, 107,
  120, 133, 146, 159,
  172, 185,   2,  15,
   28,  41,  54,  67,
   80,  93, 106, 119,
  132, 145, 158, 171,
  184,   1,  14,  27,
   40,  53,  66,  79,
   92, 105, 118, 131,
  144, 157, 170, 183
};


const uint8_t DeInterleaveReverseChannelBptc[32] =
{
   0, 17,  2, 19,  4, 21,  6, 23,
   8, 25, 10, 27, 12, 29, 14, 31,
  16,  1, 18,  3, 20,  5, 22,  7,
  24,  9, 26, 11, 28, 13, 30, 15
};

const uint8_t DeInterleaveReverseChannelBptcPlacement[32] =
{
   0, 16,  1, 17,  2, 18,  3, 19,
   4, 20,  5, 21,  6, 22,  7, 23,
   8, 24,  9, 25, 10, 26, 11, 27,
  12, 28, 13, 29, 14, 30, 15, 31
};

/* Functions ----------------------------------------------------------------*/


/*
 * @brief : This function deinterleave the DMR data by using BPTC (196,96)
 *
 * @param Input : Pointer of DMR input data interleaved (196 bytes)
 *
 * @param Output : Pointer where DMR deinterleaved data will be written (196 bytes)
 *
 * @return None
 */
void BPTCDeInterleaveDMRData(uint8_t * Input, uint8_t * Output)
{
  uint32_t i, DeInterleaveIndex;

  for(i = 0; i < 196; i++)
  {
    DeInterleaveIndex = BPTCDeInterleavingIndex[i];
    Output[DeInterleaveIndex] = (Input[i] & 1);
  }
} /* End BPTCDeInterleaveDMRData() */


/*
 * @brief : This function extract the 96 bits of a deinteleaved 196 bits
 *          buffer using BPTC (196,96)
 *
 * @param InputDeInteleavedData : Pointer of DMR input data deinterleaved (196 bytes)
 *
 * @param DMRDataExtracted : Pointer where the DMR data will be written (96 bytes)
 *
 * @return The total number of irrecoverable Hamming check errors
 */
uint32_t BPTC_196x96_Extract_Data(uint8_t InputDeInteleavedData[196], uint8_t DMRDataExtracted[96], uint8_t R[3])
{
  uint32_t i, j, k;
  uint8_t  DataMatrix[13][15];
  uint8_t  LineUncorrected[15];
  uint8_t  LineCorrected[11];
  uint8_t  ColumnUncorrected[13];
  uint8_t  ColumnCorrected[9];
  uint32_t HammingIrrecoverableErrorNb = 0;

  /* First step : Reconstitute the BPTC 15x11 matrix
   * Note : Input data shall be deinterleaved */
  k = 1; /* Discard R(3) bit - See DMR standard chapter B1.1 BPTC (196,96) */
  for(i = 0; i < 13; i++)
  {
    for(j = 0; j < 15; j++)
    {
      /* Only the LSBit of the byte is stored */
      DataMatrix[i][j] = (InputDeInteleavedData[k] & 1);
      k++;
    }
  }

  /* Set to 0 R(0) to R(2) - See DMR standard chapter B1.1 BPTC (196,96) */
  // DataMatrix[0][0] = 0; /* R(2) */
  // DataMatrix[0][1] = 0; /* R(1) */
  // DataMatrix[0][2] = 0; /* R(0) */

  /* Init the Hamming (15,11,3) library
   * Not needed because it has already been done
   * in the "InitAllFecFunction()" function */
  //Hamming_15_11_init();

  /* Init the Hamming (13,9,3) library
   * Not needed because it has already been done
   * in the "InitAllFecFunction()" function */
  //Hamming_13_9_init();

  /* Init the Hamming error counter */
  HammingIrrecoverableErrorNb = 0;

  /* Process the the Hamming (15,11,3) code
   * check on each line (first time).
   * Do not check the 4th last lines */
  for(i = 0; i < 9; i++)
  {
    /* Get a full line */
    for(j = 0; j < 15; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
    }

    /* Apply Hamming (15,11,3) code correction */
    if(Hamming_15_11_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
    }

    /* Re-inject the line in the matrix (only the util data [11 bit],
     * not the Hamming part [4 bit]) */
    for(j = 0; j < 11; j++)
    {
      DataMatrix[i][j] = LineCorrected[j];
    }
  }


  /* Process the the Hamming (15,11,3) code
   * check on each column/row (first time) */
  for(i = 0; i < 15; i++)
  {
    /* Get a full column */
    for(j = 0; j < 13; j++)
    {
      ColumnUncorrected[j] = DataMatrix[j][i];
    }

    /* Apply Hamming (15,11,3) code correction */
    if(Hamming_13_9_decode(ColumnUncorrected, ColumnCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
    }

    /* Re-inject the column in the matrix (only the util data [11 bit],
     * not the Hamming part [4 bit]) */
    for(j = 0; j < 9; j++)
    {
      DataMatrix[j][i] = ColumnCorrected[j];
    }
  }

  /* The first Hamming code check has maybe corrected bit
   * witch can be use to correct other bit, so make the
   * same operation twice */

  /* Reset the number of irrecoverable errors */
  HammingIrrecoverableErrorNb = 0;


  /* Process the the Hamming (15,11,3) code
   * check on each line (second time).
   * Do not check the 4th last lines */
  for(i = 0; i < 9; i++)
  {
    /* Get a full line */
    for(j = 0; j < 15; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
    }

    /* Apply Hamming (15,11,3) code correction */
    if(Hamming_15_11_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      /* Row/Line contains irrecoverable errors */
      HammingIrrecoverableErrorNb++;
    }
    else
    {
      /* Row/Line contains correct data */
    }

    /* Re-inject the line in the matrix (only the util data [11 bit],
     * not the Hamming part [4 bit]) */
    for(j = 0; j < 11; j++)
    {
      DataMatrix[i][j] = LineCorrected[j];
    }
  }


  /* Process the the Hamming (15,11,3) code
   * check on each column/row (second time) */
  for(i = 0; i < 15; i++)
  {
    /* Get a full column */
    for(j = 0; j < 13; j++)
    {
      ColumnUncorrected[j] = DataMatrix[j][i];
    }

    /* Apply Hamming (15,11,3) code correction */
    if(Hamming_13_9_decode(ColumnUncorrected, ColumnCorrected, 1) == false)
    {
      /* Row/Line contains irrecoverable errors */
      HammingIrrecoverableErrorNb++;
    }
    else
    {
      /* Row/Line contains correct data */
    }

    /* Re-inject the column in the matrix (only the util data [11 bit],
     * not the Hamming part [4 bit]) */
    for(j = 0; j < 9; j++)
    {
      DataMatrix[j][i] = ColumnCorrected[j];
    }
  }

  /* Extract the DMR data (96 bit) from the matrix */
  /* First line : Do not take R(2), R(1) and R(0) */
  k = 0;

  for(i = 3; i < 11; i++)
  {
    DMRDataExtracted[k] = DataMatrix[0][i];
    k++;
  }

  /* Next lines */
  for(i = 1; i < 9; i++)
  {
    for(j = 0; j < 11; j++)
    {
      DMRDataExtracted[k] = DataMatrix[i][j];
      k++;
    }
  }

  /* R(0) to R(2) may be used to transport some
   * Restricted Access System (RAS) information,
   * So save these three bits after hamming correction
   * See patent US 2013/0288643 A1 */
  R[0] = DataMatrix[0][2]; /* Save R(0) */
  R[1] = DataMatrix[0][1]; /* Save R(1) */
  R[2] = DataMatrix[0][0]; /* Save R(2) */

  /* Return the number of irrecoverable Hamming errors */
  return HammingIrrecoverableErrorNb;
} /* End BPTC_196x96_Extract_Data() */


/*
 * @brief : This function extract the 77 bits of a deinteleaved 128 bits
 *          buffer using BPTC (128,77).
 *
 * @note : See DMR standard ETSI TS 102 361-1 chapter B.2.1 for details
 *
 * @param InputDeInteleavedData : Pointer of DMR input data deinterleaved (128 bytes
                                  act as a 16 x 8 byte data matrix)
 *
 * @param DMRDataExtracted : Pointer where the DMR data will be written (77 bytes)
 *                           DMRDataExtracted[0..71]  = Util data (hamming corrected)
 *                           DMRDataExtracted[72..76] = 5 bit of CRC extracted (to be checked)
 *
 * @return 0 = All is OK
 *         Other = Error - Irrecoverable Hamming parity check error number
 */
uint32_t BPTC_128x77_Extract_Data(uint8_t InputDataMatrix[8][16], uint8_t DMRDataExtracted[77])
{
  uint32_t i, j, k;
  uint8_t  DataMatrix[8][16];
  uint8_t  LineUncorrected[16];
  uint8_t  LineCorrected[11];
  uint32_t HammingIrrecoverableErrorNb = 0;
  uint32_t ParityCheckErrorNb = 0;
  uint32_t NbOfOne;

  /* First step : Reconstitute the BPTC 16x8 matrix */
  for(i = 0; i < 8; i++)
  {
    for(j = 0; j < 16; j++)
    {
      /* Only the LSBit of the byte is stored */
      DataMatrix[i][j] = (InputDataMatrix[i][j] & 1);
    }
  }

  /* Init the Hamming (16,11,4) library
   * Not needed because it has already been done
   * in the "InitAllFecFunction()" function */
  //Hamming_16_11_4_init();

  /* Process the the Hamming (16,11,4) code
   * check on each line.
   * Do not check the last line (line nb 8) */
  for(i = 0; i < 7; i++)
  {
    /* Get a full line */
    for(j = 0; j < 16; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
    }

    /* Apply Hamming (16,11,4) code correction */
    if(Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
    }

    /* Re-inject the line in the matrix (only the util data [11 bit],
     * not the Hamming part [4 bit]) */
    for(j = 0; j < 11; j++)
    {
      DataMatrix[i][j] = LineCorrected[j];
    }
  }

  /* Extract the DMR data (77 bit) from the matrix */
  k = 0;

  /* 2 first lines */
  for(i = 0; i < 2; i++)
  {
    for(j = 0; j < 11; j++)
    {
      DMRDataExtracted[k] = DataMatrix[i][j];
      k++;
    }
  }

  /* 5 Next lines */
  for(i = 2; i < 7; i++)
  {
    for(j = 0; j < 10; j++)
    {
      DMRDataExtracted[k] = DataMatrix[i][j];
      k++;
    }
  }

  /* 5 bit of CRC */
  for(i = 2; i < 7; i++)
  {
    DMRDataExtracted[k] = DataMatrix[i][10];
    k++;
  }

  /* Verify the data integrity by checking
   * all column parity bit */
  ParityCheckErrorNb = 0;
  for(i = 0; i < 16; i++)
  {
    NbOfOne = 0;

    /* Get a full column */
    for(j = 0; j < 7; j++)
    {
      NbOfOne += DataMatrix[j][i];
    }

    /* Check the parity bit (number of "1"
     * must be even) */
    if((NbOfOne % 2) != DataMatrix[7][i])
    {
      ParityCheckErrorNb++;
    }
  }

  /* Return the number of irrecoverable Hamming errors +
   * the number of parity check error */
  return (HammingIrrecoverableErrorNb + ParityCheckErrorNb);
} /* End BPTC_128x77_Extract_Data() */


/*
 * @brief : This function extract the 32 bits of a deinteleaved 32 bits
 *          buffer using Reverse Channel Single Burst BPTC.
 *
 * @note : See DMR standard ETSI TS 102 361-1 chapter B.2.2.2 for details
 *
 * @param InputInterleavedData : Pointer of DMR input data interleaved (32 bytes
                                 act as a 16 x 2 bits data matrix)
 *
 * @param DMRDataExtracted : Pointer where the DMR data will be written (77 bytes)
 *                           DMRDataExtracted[0..10]  = Util data (Hamming corrected)
 *                           DMRDataExtracted[11..15] = Hamming data
 *                           DMRDataExtracted[16..31] = 16 bits of odd parity (to be checked)
 *
 * @param ParityCheckTypeOdd : 0 = Parity to check is even (used in Non-Reverse Channel Single Burst BPTC)
 *                             1 = Parity to check is odd (used in Reverse Channel Single Burst BPTC)
 *
 * @return 0 = All is OK
 *         Other = Error - Irrecoverable Hamming parity check error number
 */
uint32_t BPTC_16x2_Extract_Data(uint8_t InputInterleavedData[32], uint8_t DMRDataExtracted[32], uint32_t ParityCheckTypeOdd)
{
  uint32_t i;
  uint8_t  DataMatrix[32];
  uint8_t  LineUncorrected[16];
  uint8_t  LineCorrected[11];
  uint32_t HammingIrrecoverableErrorNb = 0;
  uint32_t ParityCheckOddErrorNb = 0;
  uint32_t ParityCheckEvenErrorNb = 0;

  //TODO: make this so we can load either rc interleave, or single burst interleave
  for(i = 0; i < 32; i++)
  {
    DataMatrix[DeInterleaveReverseChannelBptcPlacement[DeInterleaveReverseChannelBptc[i]]] = InputInterleavedData[i] & 1;
  }

  /* Copy the entire input buffer to the output buffer */
  for(i = 0; i < 32; i++)
  {
    DMRDataExtracted[i] = DataMatrix[i] & 1;
  }

  /* Prepare the line to be corrected */
  for(i = 0; i < 16; i++)
  {
    LineUncorrected[i] = DataMatrix[i] & 1;
  }

  /* Apply Hamming (16,11,4) code correction */
  if(Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
  {
    HammingIrrecoverableErrorNb++;
  }

  /* Copy the corrected content to the output buffer */
  for(i = 0; i < 11; i++)
  {
    DMRDataExtracted[i] = LineCorrected[i] & 1;
  }

  /* Check Parity bits */
  for(i = 0; i < 16; i++)
  {
    /* Odd parity ==> If data = 1 then parity = 0
     *                If data = 0 then parity = 1 */
    if((DMRDataExtracted[i] & 1) == (DMRDataExtracted[i + 16] & 1)) ParityCheckOddErrorNb++;
    if((DMRDataExtracted[i] & 1) != (DMRDataExtracted[i + 16] & 1)) ParityCheckEvenErrorNb++;
  }

  //fprintf(stderr, "Hamming ERR=%u ; Parity odd ERR=%u, Parity even ERR=%u, ", HammingIrrecoverableErrorNb, ParityCheckOddErrorNb, ParityCheckEvenErrorNb);

  //fprintf(stderr, "Content =\n");
  //for(i = 0; i < 32; i++)
  //{
  //  fprintf(stderr, "%u", DMRDataExtracted[i] & 1);
  //  if((i == 15) || (i == 31)) fprintf(stderr, "\n");
  //  else fprintf(stderr, "-");
  //}

  /* Return the number of irrecoverable Hamming errors +
   * the number of parity check error */
  if(ParityCheckTypeOdd) return (HammingIrrecoverableErrorNb + ParityCheckOddErrorNb);
  else return (HammingIrrecoverableErrorNb + ParityCheckEvenErrorNb);

} /* End BPTC_16x2_Extract_Data() */



/* End of file */
