/*
 *   Copyright (C) 2009-2016,2018 by Jonathan Naylor G4KLX
 *
 *   Copyright (C) 2018 by Edouard Griffiths F4EXB:
 *   - Cosmetic changes to integrate with DSDcc
 *
 *   Copyright (C) 2018 by Louis HERVE F4HUZ:
 *   - Transform C++ lib into C lib to integrate with DSD
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/* Include ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>


/* Define -------------------------------------------------------------------*/


/* Global variables ---------------------------------------------------------*/
static const uint8_t CNXDNConvolution_BIT_MASK_TABLE[8] = {0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U};

#define WRITE_BIT1(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | CNXDNConvolution_BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~CNXDNConvolution_BIT_MASK_TABLE[(i)&7])
#define READ_BIT1(p,i)    (p[(i)>>3] & CNXDNConvolution_BIT_MASK_TABLE[(i)&7])

static const uint8_t CNXDNConvolution_BRANCH_TABLE1[8] = {0U, 0U, 0U, 0U, 2U, 2U, 2U, 2U};
static const uint8_t CNXDNConvolution_BRANCH_TABLE2[8] = {0U, 2U, 2U, 0U, 0U, 2U, 2U, 0U};

static const unsigned int CNXDNConvolution_NUM_OF_STATES_D2 = 8U;
static const uint32_t     CNXDNConvolution_M = 4U;
static const unsigned int CNXDNConvolution_K = 5U;

//NOTE:
uint16_t   m_metrics1[16];
uint16_t   m_metrics2[16];
uint64_t   m_decisions[8*300];
uint16_t * m_oldMetrics = NULL;
uint16_t * m_newMetrics = NULL;
uint64_t * m_dp = NULL;

/* Functions ----------------------------------------------------------------*/

void CNXDNConvolution_decode(uint8_t s0, uint8_t s1)
{
  uint8_t    i = 0;
  uint8_t    j = 0;
  uint8_t    decision0 = 0;
  uint8_t    decision1 = 0;
  uint16_t   metric = 0;
  uint16_t   m0 = 0;
  uint16_t   m1 = 0;
  uint16_t * tmp = NULL;

  *m_dp = 0U;

  for (i = 0U; i < CNXDNConvolution_NUM_OF_STATES_D2; i++)
  {
    j = i * 2U;

    metric = abs(CNXDNConvolution_BRANCH_TABLE1[i] - s0) + abs(CNXDNConvolution_BRANCH_TABLE2[i] - s1);

    m0 = m_oldMetrics[i] + metric;
    m1 = m_oldMetrics[i + CNXDNConvolution_NUM_OF_STATES_D2] + (CNXDNConvolution_M - metric);
    decision0 = (m0 >= m1) ? 1U : 0U;
    m_newMetrics[j + 0U] = decision0 != 0U ? m1 : m0;

    m0 = m_oldMetrics[i] + (CNXDNConvolution_M - metric);
    m1 = m_oldMetrics[i + CNXDNConvolution_NUM_OF_STATES_D2] + metric;
    decision1 = (m0 >= m1) ? 1U : 0U;
    m_newMetrics[j + 1U] = decision1 != 0U ? m1 : m0;

    *m_dp |= ((uint64_t)(decision1) << (j + 1U)) | ((uint64_t)(decision0) << (j + 0U));
  }

  ++m_dp;

  //assert((m_dp - m_decisions) <= 300);

  tmp = m_oldMetrics;
  m_oldMetrics = m_newMetrics;
  m_newMetrics = tmp;
}

void CNXDNConvolution_chainback(unsigned char* out, unsigned int nBits)
{
  //assert(out != 0);

  uint32_t state = 0U;
  uint32_t i = 0;
  uint8_t  bit = 0;

  while (nBits-- > 0)
  {
    --m_dp;

    i = state >> (9 - CNXDNConvolution_K);
    bit = (uint8_t)(*m_dp >> i) & 1;
    state = (bit << 7) | (state >> 1);

    WRITE_BIT1(out, nBits, bit != 0U);
  }
}

void CNXDNConvolution_encode(const unsigned char* in, unsigned char* out, unsigned int nBits)
{
  //assert(in != 0);
  //assert(out != 0);
  //assert(nBits > 0U);

  uint8_t g1 = 0U;
  uint8_t g2 = 0U;
  uint8_t d = 0U;
  uint8_t d1 = 0U;
  uint8_t d2 = 0U;
  uint8_t d3 = 0U;
  uint8_t d4 = 0U;
  uint32_t k = 0U;
  unsigned int i = 0U;

  for (i = 0U; i < nBits; i++)
  {
    d = READ_BIT1(in, i) ? 1U : 0U;

    g1 = (d + d3 + d4) & 1;
    g2 = (d + d1 + d2 + d4) & 1;

    d4 = d3;
    d3 = d2;
    d2 = d1;
    d1 = d;

    WRITE_BIT1(out, k, g1 != 0U);
    k++;

    WRITE_BIT1(out, k, g2 != 0U);
    k++;
  }
}

void CNXDNConvolution_start(void)
{

  m_oldMetrics = m_metrics1;
  m_newMetrics = m_metrics2;
  m_dp = m_decisions;

}

void CNXDNConvolution_init()
{
  memset(m_metrics1, 0x0, 16*sizeof(uint16_t));
  memset(m_metrics2, 0x0, 16*sizeof(uint16_t));
  memset(m_decisions,0x0, 8*300*sizeof(uint64_t));
}

