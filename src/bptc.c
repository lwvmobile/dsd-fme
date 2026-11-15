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
 * BPTC (196,96) extract — ETSI TS 102 361-1 B.1.1
 * Two-pass row Hamming(15,11) + column Hamming(13,9).
 * Debug: define BPTC_196x96_DEBUG 1 for row/col correction chatter.
 */

// #define BPTC_196x96_DEBUG 1

uint32_t BPTC_196x96_Extract_Data(uint8_t InputDeInteleavedData[196],
                                  uint8_t DMRDataExtracted[96],
                                  uint8_t R[3],
                                  uint8_t * is_ras)
{
  uint32_t i, j, k;
  uint8_t  DataMatrix[13][15];
  uint8_t  LineUncorrected[15];
  uint8_t  LineBefore[15];
  uint8_t  LineCorrected[11];
  uint8_t  ColumnUncorrected[13];
  uint8_t  ColumnBefore[13];
  uint8_t  ColumnCorrected[9];
  uint32_t HammingIrrecoverableErrorNb = 0;
  uint32_t row_corr_p1 = 0, col_corr_p1 = 0;
  uint32_t row_fail_p1 = 0, col_fail_p1 = 0;
  uint32_t row_corr_p2 = 0, col_corr_p2 = 0;
  uint32_t row_fail_p2 = 0, col_fail_p2 = 0;

  UNUSED(row_corr_p1); UNUSED(col_corr_p1);
  UNUSED(row_fail_p1); UNUSED(col_fail_p1);
  UNUSED(row_corr_p2); UNUSED(col_corr_p2);
  UNUSED(row_fail_p2); UNUSED(col_fail_p2);

#if BPTC_196x96_DEBUG
  fprintf (stderr, "\n");
#endif

  /* Rebuild 13×15 matrix; discard R(3) at index 0 — DMR B.1.1 */
  k = 1;
  for (i = 0; i < 13; i++)
  {
    for (j = 0; j < 15; j++)
    {
      DataMatrix[i][j] = (InputDeInteleavedData[k] & 1);
      k++;
    }
  }

  /* ========== Pass 1: row Hamming(15,11) on rows 0..12 ========== */
  for (i = 0; i < 13; i++)
  {
    for (j = 0; j < 15; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
      LineBefore[j] = LineUncorrected[j];
    }

    if (Hamming_15_11_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
      row_fail_p1++;
#if BPTC_196x96_DEBUG
      fprintf(stderr, " BPTC196 P1 ROW FAIL row=%u  rx=", (unsigned)i);
      for (j = 0; j < 15; j++)
        fprintf(stderr, "%u", LineBefore[j] & 1);
      fprintf(stderr, "\n");
#endif
      /* Keep original bits on failure */
      for (j = 0; j < 15; j++)
        DataMatrix[i][j] = LineBefore[j];
    }
    else
    {
      /* rxBits were corrected in place; note any flip */
      for (j = 0; j < 15; j++)
      {
        if ((LineBefore[j] & 1) != (LineUncorrected[j] & 1))
        {
          row_corr_p1++;
#if BPTC_196x96_DEBUG
          fprintf(stderr,
                  " BPTC196 P1 ROW CORRECTED row=%u bit=%u  %u->%u\n",
                  (unsigned)i, (unsigned)j,
                  (unsigned)(LineBefore[j] & 1),
                  (unsigned)(LineUncorrected[j] & 1));
#endif
        }
      }
      /* Re-inject the full line (including corrected parity bits) in the matrix */
      for (j = 0; j < 15; j++)
        DataMatrix[i][j] = LineUncorrected[j];
    }
  }
#if BPTC_196x96_DEBUG
  if (row_fail_p1 || row_corr_p1)
    fprintf(stderr, " BPTC196 P1 ROW summary: fail=%u corrected_bits=%u\n",
            (unsigned)row_fail_p1, (unsigned)row_corr_p1);
#endif

  /* ========== Pass 1: column Hamming(13,9) on cols 0..14 ========== */
  for (i = 0; i < 15; i++)
  {
    for (j = 0; j < 13; j++)
    {
      ColumnUncorrected[j] = DataMatrix[j][i];
      ColumnBefore[j] = ColumnUncorrected[j];
    }

    if (Hamming_13_9_decode(ColumnUncorrected, ColumnCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
      col_fail_p1++;
#if BPTC_196x96_DEBUG
      fprintf(stderr, " BPTC196 P1 COL FAIL col=%u  rx=", (unsigned)i);
      for (j = 0; j < 13; j++)
        fprintf(stderr, "%u", ColumnBefore[j] & 1);
      fprintf(stderr, "\n");
#endif
      for (j = 0; j < 13; j++)
        DataMatrix[j][i] = ColumnBefore[j];
    }
    else
    {
      for (j = 0; j < 13; j++)
      {
        if ((ColumnBefore[j] & 1) != (ColumnUncorrected[j] & 1))
        {
          col_corr_p1++;
#if BPTC_196x96_DEBUG
          fprintf(stderr,
                  " BPTC196 P1 COL CORRECTED col=%u bit=%u  %u->%u\n",
                  (unsigned)i, (unsigned)j,
                  (unsigned)(ColumnBefore[j] & 1),
                  (unsigned)(ColumnUncorrected[j] & 1));
#endif
        }
      }
      /* Re-inject the full column (including corrected parity bits) in the matrix */
      for (j = 0; j < 13; j++)
        DataMatrix[j][i] = ColumnUncorrected[j];
    }
  }
#if BPTC_196x96_DEBUG
  if (col_fail_p1 || col_corr_p1)
    fprintf(stderr, " BPTC196 P1 COL summary: fail=%u corrected_bits=%u\n",
            (unsigned)col_fail_p1, (unsigned)col_corr_p1);
#endif

  /* Second pass can fix more after the first cascade */
  HammingIrrecoverableErrorNb = 0;

  /* ========== Pass 2: rows ========== */
  for (i = 0; i < 13; i++)
  {
    for (j = 0; j < 15; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
      LineBefore[j] = LineUncorrected[j];
    }

    if (Hamming_15_11_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
      row_fail_p2++;
#if BPTC_196x96_DEBUG
      fprintf(stderr, " BPTC196 P2 ROW FAIL row=%u  rx=", (unsigned)i);
      for (j = 0; j < 15; j++)
        fprintf(stderr, "%u", LineBefore[j] & 1);
      fprintf(stderr, "\n");
#endif
      for (j = 0; j < 15; j++)
        DataMatrix[i][j] = LineBefore[j];
    }
    else
    {
      for (j = 0; j < 15; j++)
      {
        if ((LineBefore[j] & 1) != (LineUncorrected[j] & 1))
        {
          row_corr_p2++;
#if BPTC_196x96_DEBUG
          fprintf(stderr,
                  " BPTC196 P2 ROW CORRECTED row=%u bit=%u  %u->%u\n",
                  (unsigned)i, (unsigned)j,
                  (unsigned)(LineBefore[j] & 1),
                  (unsigned)(LineUncorrected[j] & 1));
#endif
        }
      }
      /* Re-inject the full line (including corrected parity bits) in the matrix */
      for (j = 0; j < 15; j++)
        DataMatrix[i][j] = LineUncorrected[j];
    }
  }
#if BPTC_196x96_DEBUG
  if (row_fail_p2 || row_corr_p2)
    fprintf(stderr, " BPTC196 P2 ROW summary: fail=%u corrected_bits=%u\n",
            (unsigned)row_fail_p2, (unsigned)row_corr_p2);
#endif

  /* ========== Pass 2: columns ========== */
  for (i = 0; i < 15; i++)
  {
    for (j = 0; j < 13; j++)
    {
      ColumnUncorrected[j] = DataMatrix[j][i];
      ColumnBefore[j] = ColumnUncorrected[j];
    }

    if (Hamming_13_9_decode(ColumnUncorrected, ColumnCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
      col_fail_p2++;
#if BPTC_196x96_DEBUG
      fprintf(stderr, " BPTC196 P2 COL FAIL col=%u  rx=", (unsigned)i);
      for (j = 0; j < 13; j++)
        fprintf(stderr, "%u", ColumnBefore[j] & 1);
      fprintf(stderr, "\n");
#endif
      for (j = 0; j < 13; j++)
        DataMatrix[j][i] = ColumnBefore[j];
    }
    else
    {
      for (j = 0; j < 13; j++)
      {
        if ((ColumnBefore[j] & 1) != (ColumnUncorrected[j] & 1))
        {
          col_corr_p2++;
#if BPTC_196x96_DEBUG
          fprintf(stderr,
                  " BPTC196 P2 COL CORRECTED col=%u bit=%u  %u->%u\n",
                  (unsigned)i, (unsigned)j,
                  (unsigned)(ColumnBefore[j] & 1),
                  (unsigned)(ColumnUncorrected[j] & 1));
#endif
        }
      }
      /* Re-inject the full column (including corrected parity bits) in the matrix */
      for (j = 0; j < 13; j++)
        DataMatrix[j][i] = ColumnUncorrected[j];
    }
  }

  /* Verify that all rows (0..12) and columns (0..14) have converged to zero syndrome.
   * Hamming_15_11_decode never returns false because all 15 non-zero 4-bit syndromes
   * map to a bit flip, so without a post-pass consistency check a noisy burst can
   * mis-correct data bits in Pass 2 Columns while still reporting 0 errors. */
  for (i = 0; i < 13; i++)
  {
    for (j = 0; j < 15; j++)
    {
      LineUncorrected[j] = DataMatrix[i][j];
    }
    if (Hamming_15_11_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
    }
    else
    {
      for (j = 0; j < 15; j++)
      {
        if (LineUncorrected[j] != DataMatrix[i][j])
        {
          HammingIrrecoverableErrorNb++;
          break;
        }
      }
    }
  }
  for (i = 0; i < 15; i++)
  {
    for (j = 0; j < 13; j++)
    {
      ColumnUncorrected[j] = DataMatrix[j][i];
    }
    if (Hamming_13_9_decode(ColumnUncorrected, ColumnCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
    }
    else
    {
      for (j = 0; j < 13; j++)
      {
        if (ColumnUncorrected[j] != DataMatrix[j][i])
        {
          HammingIrrecoverableErrorNb++;
          break;
        }
      }
    }
  }
#if BPTC_196x96_DEBUG
  if (col_fail_p2 || col_corr_p2)
    fprintf(stderr, " BPTC196 P2 COL summary: fail=%u corrected_bits=%u\n",
            (unsigned)col_fail_p2, (unsigned)col_corr_p2);
  fprintf(stderr,
          " BPTC196 FINAL irrecoverable=%u  P1(row_f=%u col_f=%u row_c=%u col_c=%u)  "
          "P2(row_f=%u col_f=%u row_c=%u col_c=%u)\n",
          (unsigned)HammingIrrecoverableErrorNb,
          (unsigned)row_fail_p1, (unsigned)col_fail_p1,
          (unsigned)row_corr_p1, (unsigned)col_corr_p1,
          (unsigned)row_fail_p2, (unsigned)col_fail_p2,
          (unsigned)row_corr_p2, (unsigned)col_corr_p2);
#endif

  /* Extract 96 data bits */
  k = 0;
  for (i = 3; i < 11; i++)
  {
    DMRDataExtracted[k++] = DataMatrix[0][i];
  }
  for (i = 1; i < 9; i++)
  {
    for (j = 0; j < 11; j++)
      DMRDataExtracted[k++] = DataMatrix[i][j];
  }

  uint32_t total_corr = row_corr_p1 + col_corr_p1 + row_corr_p2 + col_corr_p2;
  uint32_t p2_corr = row_corr_p2 + col_corr_p2;

  /* In DMR BPTC(196,96), R(0) and R(1) must be 0. Also reject R(2)==1 when the
   * received R(2) bit was 0 or the matrix required heavy multi-pass corrections,
   * because is_ras==1 bypasses downstream CRC checks when no RAS key is loaded. */
  if (DataMatrix[0][2] != 0 || DataMatrix[0][1] != 0)
  {
    HammingIrrecoverableErrorNb++;
  }
  else if (DataMatrix[0][0] == 1 && ((InputDeInteleavedData[1] & 1) == 0 || total_corr > 8 || p2_corr > 2))
  {
    HammingIrrecoverableErrorNb++;
  }

  if (HammingIrrecoverableErrorNb == 0)
  {
    R[0] = DataMatrix[0][2]; /* R(0) */
    R[1] = DataMatrix[0][1]; /* R(1) */
    R[2] = DataMatrix[0][0]; /* R(2) */
  }
  else
  {
    R[0] = 0;
    R[1] = 0;
    R[2] = 0;
  }

  if (R[0] == 0 && R[1] == 0 && R[2] == 1)
    *is_ras = 1;
  else
    *is_ras = 0;

  return HammingIrrecoverableErrorNb;
}


/*
 * BPTC (128,77) extract with iterative Hamming + parity-guided repair
 * and diagnostic logging.
 * See ETSI TS 102 361-1 B.2.1
 *
 * Enable/disable chatter with BPTC_128x77_DEBUG (1 = on).
 */

// #define BPTC_128x77_DEBUG 1

uint32_t BPTC_128x77_Extract_Data(uint8_t InputDataMatrix[8][16], uint8_t DMRDataExtracted[77])
{
  uint32_t i, j, k;
  uint8_t  DataMatrix[8][16];
  uint8_t  LineUncorrected[16];
  uint8_t  LineCorrected[11];
  uint32_t HammingIrrecoverableErrorNb = 0;
  uint32_t ParityCheckErrorNb = 0;
  uint32_t NbOfOne;

  uint8_t row_bad[8];
  uint8_t col_bad[16];

#if BPTC_128x77_DEBUG
  fprintf (stderr, "\n");
#endif

  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 16; j++)
      DataMatrix[i][j] = (InputDataMatrix[i][j] & 1);
  }

  /* ---------- Pass 1: Hamming (16,11,4) on rows 0..7 ---------- */
  memset(row_bad, 0, sizeof(row_bad));
  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 16; j++)
      LineUncorrected[j] = DataMatrix[i][j];

    if (Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
      row_bad[i] = 1;
#if BPTC_128x77_DEBUG
      fprintf(stderr, " BPTC128 P1 Hamming FAIL row=%u  rx=", (unsigned)i);
      for (j = 0; j < 16; j++)
        fprintf(stderr, "%u", LineUncorrected[j] & 1);
      fprintf(stderr, "\n");
#endif
    }
    else
    {
      for (j = 0; j < 16; j++)
        DataMatrix[i][j] = LineUncorrected[j];
    }
  }
#if BPTC_128x77_DEBUG
  if (HammingIrrecoverableErrorNb)
    fprintf(stderr, " BPTC128 P1 Hamming irrecoverable rows=%u\n",
            (unsigned)HammingIrrecoverableErrorNb);
#endif

  /* ---------- Column even-parity check (row 7) ---------- */
  memset(col_bad, 0, sizeof(col_bad));
  ParityCheckErrorNb = 0;
  for (i = 0; i < 16; i++)
  {
    NbOfOne = 0;
    for (j = 0; j < 7; j++)
      NbOfOne += DataMatrix[j][i];

    if ((NbOfOne % 2) != DataMatrix[7][i])
    {
      ParityCheckErrorNb++;
      col_bad[i] = 1;
#if BPTC_128x77_DEBUG
      fprintf(stderr,
              " BPTC128 P1 parity FAIL col=%u  sum7=%u  parity_bit=%u  (expect even)\n",
              (unsigned)i, (unsigned)NbOfOne, (unsigned)DataMatrix[7][i]);
#endif
    }
  }
#if BPTC_128x77_DEBUG
  if (ParityCheckErrorNb)
    fprintf(stderr, " BPTC128 P1 parity bad columns=%u\n",
            (unsigned)ParityCheckErrorNb);
#endif

  /* ---------- Parity-guided bit repair ---------- */
  {
    uint32_t n_bad_rows = 0, n_bad_cols = 0;
    uint32_t br = 0, bc = 0;

    UNUSED(bc);

    for (i = 0; i < 8; i++)
      if (row_bad[i]) { n_bad_rows++; br = i; }
    for (i = 0; i < 16; i++)
      if (col_bad[i]) { n_bad_cols++; bc = i; }

#if BPTC_128x77_DEBUG
    fprintf(stderr,
            " BPTC128 repair candidates: bad_rows=%u bad_cols=%u",
            (unsigned)n_bad_rows, (unsigned)n_bad_cols);
    if (n_bad_rows == 1)
      fprintf(stderr, " row=%u", (unsigned)br);
    if (n_bad_cols == 1)
      fprintf(stderr, " col=%u", (unsigned)bc);
    fprintf(stderr, "\n");
#endif

    if (n_bad_rows == 1 && (n_bad_cols == 1 || n_bad_cols == 2))
    {
      for (i = 0; i < 16; i++)
      {
        if (col_bad[i])
        {
          DataMatrix[br][i] ^= 1;
#if BPTC_128x77_DEBUG
          fprintf(stderr,
                  " BPTC128 parity-guided FLIP at row=%u col=%u -> %u\n",
                  (unsigned)br, (unsigned)i, (unsigned)DataMatrix[br][i]);
#endif
        }
      }
      row_bad[br] = 0;
    }
#if BPTC_128x77_DEBUG
    else if (n_bad_rows || n_bad_cols)
    {
      fprintf(stderr,
              " BPTC128 no repair flip "
              "(need 1 bad row and 1 or 2 bad cols)\n");
    }
#endif
  }

  /* ---------- Pass 2: Hamming (16,11,4) again ---------- */
  HammingIrrecoverableErrorNb = 0;
  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 16; j++)
      LineUncorrected[j] = DataMatrix[i][j];

    if (Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
    {
      HammingIrrecoverableErrorNb++;
#if BPTC_128x77_DEBUG
      fprintf(stderr, " BPTC128 P2 Hamming FAIL row=%u  rx=", (unsigned)i);
      for (j = 0; j < 16; j++)
        fprintf(stderr, "%u", LineUncorrected[j] & 1);
      fprintf(stderr, "\n");
#endif
    }
    else
    {
      for (j = 0; j < 16; j++)
        DataMatrix[i][j] = LineUncorrected[j];
    }
  }
#if BPTC_128x77_DEBUG
  if (HammingIrrecoverableErrorNb)
    fprintf(stderr, " BPTC128 P2 Hamming irrecoverable rows=%u\n",
            (unsigned)HammingIrrecoverableErrorNb);
#endif

  /* ---------- Final column parity recount ---------- */
  ParityCheckErrorNb = 0;
  for (i = 0; i < 16; i++)
  {
    NbOfOne = 0;
    for (j = 0; j < 7; j++)
      NbOfOne += DataMatrix[j][i];

    if ((NbOfOne % 2) != DataMatrix[7][i])
    {
      ParityCheckErrorNb++;
#if BPTC_128x77_DEBUG
      fprintf(stderr,
              " BPTC128 P2 parity FAIL col=%u  sum7=%u  parity_bit=%u\n",
              (unsigned)i, (unsigned)NbOfOne, (unsigned)DataMatrix[7][i]);
#endif
    }
  }

#if BPTC_128x77_DEBUG
  fprintf(stderr,
          " BPTC128 result: Hamming_err=%u  parity_err=%u  total=%u\n",
          (unsigned)HammingIrrecoverableErrorNb,
          (unsigned)ParityCheckErrorNb,
          (unsigned)(HammingIrrecoverableErrorNb + ParityCheckErrorNb));
#endif

  /* ---------- Extract 77 bits ---------- */
  k = 0;
  for (i = 0; i < 2; i++)
  {
    for (j = 0; j < 11; j++)
      DMRDataExtracted[k++] = DataMatrix[i][j];
  }
  for (i = 2; i < 7; i++)
  {
    for (j = 0; j < 10; j++)
      DMRDataExtracted[k++] = DataMatrix[i][j];
  }
  for (i = 2; i < 7; i++)
    DMRDataExtracted[k++] = DataMatrix[i][10];

  return (HammingIrrecoverableErrorNb + ParityCheckErrorNb);
}

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

  /* Prepare the first row (bits 0..15) to be corrected */
  for(i = 0; i < 16; i++)
  {
    LineUncorrected[i] = DataMatrix[i] & 1;
  }

  /* Apply Hamming (16,11,4) code correction to row 0 */
  if(Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
  {
    HammingIrrecoverableErrorNb++;
  }
  else
  {
    /* Copy all 16 corrected bits (including parity bits 11..15) */
    for(i = 0; i < 16; i++)
    {
      DMRDataExtracted[i] = LineUncorrected[i] & 1;
    }
  }

  /* Prepare the second row (bits 16..31) to be corrected;
   * 1^16 is a codeword of Hamming(16,11,4), so both even and odd parity rows are valid codewords */
  for(i = 0; i < 16; i++)
  {
    LineUncorrected[i] = DataMatrix[i + 16] & 1;
  }

  /* Apply Hamming (16,11,4) code correction to row 1 */
  if(Hamming_16_11_4_decode(LineUncorrected, LineCorrected, 1) == false)
  {
    HammingIrrecoverableErrorNb++;
  }
  else
  {
    /* Copy all 16 corrected bits (including parity bits 11..15) */
    for(i = 0; i < 16; i++)
    {
      DMRDataExtracted[i + 16] = LineUncorrected[i] & 1;
    }
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

uint32_t BPTC_128x77_Check_Errors(uint8_t InputDataMatrix[8][16])
{
  uint32_t i, j;
  uint32_t ham_bad = 0, ham_fix = 0, par_err = 0;
  uint8_t  line[16], before[16], decoded[11];
  uint32_t ones;
  int changed;

  for (i = 0; i < 7; i++)
  {
    for (j = 0; j < 16; j++)
    {
      before[j] = (InputDataMatrix[i][j] & 1);
      line[j]   = before[j];
    }

    if (Hamming_16_11_4_decode(line, decoded, 1) == false)
    {
      ham_bad++;
    }
    else
    {
      changed = 0;
      for (j = 0; j < 16; j++)
      {
        if (line[j] != before[j])
        {
          changed = 1;
          break;
        }
      }
      if (changed)
        ham_fix++;
    }
  }

  for (i = 0; i < 16; i++)
  {
    ones = 0;
    for (j = 0; j < 7; j++)
      ones += (InputDataMatrix[j][i] & 1);
    if ((ones % 2) != (InputDataMatrix[7][i] & 1))
      par_err++;
  }

  return ham_bad + ham_fix + par_err;
}

/* End of file */
