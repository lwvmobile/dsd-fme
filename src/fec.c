///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "dsd.h"

unsigned char Hamming_7_4_m_corr[8]; //!< single bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Hamming_7_4_m_G[7*4] = {
        1, 0, 0, 0,   1, 0, 1,
        0, 1, 0, 0,   1, 1, 1,
        0, 0, 1, 0,   1, 1, 0,
        0, 0, 0, 1,   0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char Hamming_7_4_m_H[7*3] = {
        1, 1, 1, 0,   1, 0, 0,
        0, 1, 1, 1,   0, 1, 0,
        1, 1, 0, 1,   0, 0, 1
//      0  1  2  3 <- correctable bit positions
};


// ========================================================================================


unsigned char Hamming_12_8_m_corr[16]; //!< single bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Hamming_12_8_m_G[12*8] = {
        1, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1,
        0, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0,
        0, 0, 0, 1, 0, 0, 0, 0,   0, 1, 0, 1,
        0, 0, 0, 0, 1, 0, 0, 0,   1, 0, 1, 1,
        0, 0, 0, 0, 0, 1, 0, 0,   1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,   0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char Hamming_12_8_m_H[12*4] = {
        1, 0, 1, 0, 1, 1, 0, 0,   1, 0, 0, 0,
        1, 1, 0, 1, 0, 1, 1, 0,   0, 1, 0, 0,
        1, 1, 1, 0, 1, 0, 1, 1,   0, 0, 1, 0,
        0, 1, 0, 1, 1, 0, 0, 1,   0, 0, 0, 1
//      0  1  2  3  4  5  6  7 <- correctable bit positions
};


// ========================================================================================


unsigned char Hamming_13_9_m_corr[16]; //!< single bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Hamming_13_9_m_G[13*9] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 1,
        0, 1, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 0,
        0, 0, 1, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1,
        0, 0, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0,   0, 1, 0, 1,
        0, 0, 0, 0, 0, 1, 0, 0, 0,   1, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 1, 0, 0,   1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0,   0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 1, 1
};

//!< Parity check matrix of bits
const unsigned char Hamming_13_9_m_H[13*4] = {
        1, 1, 0, 1, 0, 1, 1, 0, 0,   1, 0, 0, 0,
        1, 1, 1, 0, 1, 0, 1, 1, 0,   0, 1, 0, 0,
        1, 1, 1, 1, 0, 1, 0, 1, 1,   0, 0, 1, 0,
        1, 0, 1, 0, 1, 1, 0, 0, 1,   0, 0, 0, 1,
//      0  1  2  3  4  5  6  7  8  <- correctable bit positions
};


// ========================================================================================


unsigned char Hamming_15_11_m_corr[16]; //!< single bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Hamming_15_11_m_G[15*11] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 0, 0, 1,
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 0, 1,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 1,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,   0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   1, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,   1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,   0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char Hamming_15_11_m_H[15*4] = {
        1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0,   1, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0,   0, 1, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1,   0, 0, 1, 0,
        1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1,   0, 0, 0, 1,
//      0  1  2  3  4  5  6  7  8  9 10  <- correctable bit positions
};


// ========================================================================================


unsigned char Hamming_16_11_4_m_corr[32]; //!< single bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Hamming_16_11_4_m_G[16*11] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 0, 0, 1, 1,
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 0, 1, 0,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 1, 1,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,   0, 1, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   1, 0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,   1, 1, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,   0, 1, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 1, 1, 1
};

//!< Parity check matrix of bits
const unsigned char Hamming_16_11_4_m_H[16*5] = {
        1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0,   1, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0,   0, 1, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1,   0, 0, 1, 0, 0,
        1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1,   0, 0, 0, 1, 0,
        1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1,   0, 0, 0, 0, 1
};


// ========================================================================================


unsigned char Golay_20_8_m_corr[4096][3]; //!< up to 3 bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Golay_20_8_m_G[20*8] = {
        1, 0, 0, 0, 0, 0, 0, 0,    0, 0, 1, 1,  1, 1, 0, 1,  1, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,    1, 1, 0, 1,  1, 0, 0, 1,  1, 0, 0, 1,
        0, 0, 1, 0, 0, 0, 0, 0,    0, 1, 1, 0,  1, 1, 0, 0,  1, 1, 0, 1,
        0, 0, 0, 1, 0, 0, 0, 0,    0, 0, 1, 1,  0, 1, 1, 0,  0, 1, 1, 1,
        0, 0, 0, 0, 1, 0, 0, 0,    1, 1, 0, 1,  1, 1, 0, 0,  0, 1, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0,    1, 0, 1, 0,  1, 0, 0, 1,  0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 1, 0,    1, 0, 0, 1,  0, 0, 1, 1,  1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1,    1, 0, 0, 0,  1, 1, 1, 0,  1, 0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char Golay_20_8_m_H[20*12] = {
        0, 1, 0, 0, 1, 1, 1, 1,    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 0, 1, 0, 0, 0,    0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0,    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 1, 0, 1, 0,    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 0, 1, 1, 0, 1,    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 1, 1, 0, 0, 1,    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 1, 1,    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        1, 1, 0, 0, 0, 1, 1, 0,    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        1, 1, 1, 0, 0, 0, 1, 1,    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 1, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 1, 1, 1, 1,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 1, 0, 1, 0, 1,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
};


// ========================================================================================


unsigned char Golay_23_12_m_corr[2048][3]; //!< up to 3 bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Golay_23_12_m_G[23*12] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,   0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,   1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,   0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,   0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,   1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,   1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1,
};

//!< Parity check matrix of bits
const unsigned char Golay_23_12_m_H[23*11] = {
        1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1,   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0,   0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0,   0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0,   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1,   0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,   0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0,   0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1,   0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
};


// ========================================================================================


unsigned char Golay_24_12_m_corr[4096][3]; //!< up to 3 bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char Golay_24_12_m_G[24*12] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1,
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1,
        0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,   1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,   0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,   0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,   1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,   0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,   0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,   1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,   1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char Golay_24_12_m_H[24*12] = {
        1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1,   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0,   0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0,   0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0,   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1,   0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,   0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0,   0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1,   0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
};


// ========================================================================================


unsigned char QR_16_7_6_m_corr[512][2]; //!< up to 2 bit error correction by syndrome index

//!< Generator matrix of bits
const unsigned char QR_16_7_6_m_G[16*7] = {
        1, 0, 0, 0, 0, 0, 0,    0, 0, 1, 0, 0, 1, 1, 1, 1,
        0, 1, 0, 0, 0, 0, 0,    1, 0, 0, 0, 1, 1, 1, 1, 0,
        0, 0, 1, 0, 0, 0, 0,    1, 1, 0, 1, 1, 0, 1, 1, 1,
        0, 0, 0, 1, 0, 0, 0,    1, 1, 1, 1, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 1, 0, 0,    1, 1, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 0, 0, 0, 1, 0,    0, 1, 1, 1, 0, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1,    0, 0, 1, 1, 1, 0, 0, 1, 1,
};

//!< Parity check matrix of bits
const unsigned char QR_16_7_6_m_H[16*9] = {
        0, 1, 1,  1, 1, 0, 0,   1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1,  1, 1, 1, 0,   0, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0,  1, 1, 1, 1,   0, 0, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 1,  1, 0, 1, 1,   0, 0, 0, 1, 0, 0, 0, 0, 0,
        0, 1, 1,  0, 0, 0, 1,   0, 0, 0, 0, 1, 0, 0, 0, 0,
        1, 1, 0,  0, 1, 0, 0,   0, 0, 0, 0, 0, 1, 0, 0, 0,
        1, 1, 1,  0, 0, 1, 0,   0, 0, 0, 0, 0, 0, 1, 0, 0,
        1, 1, 1,  1, 0, 0, 1,   0, 0, 0, 0, 0, 0, 0, 1, 0,
        1, 0, 1,  0, 1, 1, 1,   0, 0, 0, 0, 0, 0, 0, 0, 1,
};


// ========================================================================================


void Hamming_7_4_init()
{
    // correctable bit positions given syndrome bits as index (see above)
    memset(Hamming_7_4_m_corr, 0xFF, 8); // initialize with all invalid positions
    Hamming_7_4_m_corr[0b101] = 0;
    Hamming_7_4_m_corr[0b111] = 1;
    Hamming_7_4_m_corr[0b110] = 2;
    Hamming_7_4_m_corr[0b011] = 3;
    Hamming_7_4_m_corr[0b100] = 4;
    Hamming_7_4_m_corr[0b010] = 5;
    Hamming_7_4_m_corr[0b001] = 6;
}

// Not very efficient but encode is used for unit testing only
void Hamming_7_4_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    memset(encodedBits, 0, 7);

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 7; j++)
        {
            encodedBits[j] += origBits[i] * Hamming_7_4_m_G[7*i + j];
        }
    }

    for (i = 0; i < 7; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool Hamming_7_4_decode(unsigned char *rxBits) // corrects in place
{
    unsigned int syndromeI = 0; // syndrome index
    int is = 0;
    int correction = 0;

    for (is = 0; is < 3; is++)
    {
        syndromeI += (((rxBits[0] * Hamming_7_4_m_H[7*is + 0])
                     + (rxBits[1] * Hamming_7_4_m_H[7*is + 1])
                     + (rxBits[2] * Hamming_7_4_m_H[7*is + 2])
                     + (rxBits[3] * Hamming_7_4_m_H[7*is + 3])
                     + (rxBits[4] * Hamming_7_4_m_H[7*is + 4])
                     + (rxBits[5] * Hamming_7_4_m_H[7*is + 5])
                     + (rxBits[6] * Hamming_7_4_m_H[7*is + 6])) % 2) << (2-is);
    }

    if (syndromeI > 0)
    {
        if (Hamming_7_4_m_corr[syndromeI] == 0xFF)
        {
            return false;
        }
        else
        {
            rxBits[Hamming_7_4_m_corr[syndromeI]] ^= 1; // flip bit
            correction++;
        }
        //not sure of upper limit on what hamming can correct (if any),
        //but will test with 0 and 1 to see how those perform
        if (correction > 1)
        {
            return false;
        }
    }

    return true;
}


// ========================================================================================


void Hamming_12_8_init()
{
    // correctable bit positions given syndrome bits as index (see above)
    memset(Hamming_12_8_m_corr, 0xFF, 16); // initialize with all invalid positions
    Hamming_12_8_m_corr[0b1110] = 0;
    Hamming_12_8_m_corr[0b0111] = 1;
    Hamming_12_8_m_corr[0b1010] = 2;
    Hamming_12_8_m_corr[0b0101] = 3;
    Hamming_12_8_m_corr[0b1011] = 4;
    Hamming_12_8_m_corr[0b1100] = 5;
    Hamming_12_8_m_corr[0b0110] = 6;
    Hamming_12_8_m_corr[0b0011] = 7;
    Hamming_12_8_m_corr[0b1000] = 8;
    Hamming_12_8_m_corr[0b0100] = 9;
    Hamming_12_8_m_corr[0b0010] = 10;
    Hamming_12_8_m_corr[0b0001] = 11;
}

// Not very efficient but encode is used for unit testing only
void Hamming_12_8_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    memset(encodedBits, 0, 12);

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 12; j++)
        {
            encodedBits[j] += origBits[i] * Hamming_12_8_m_G[12*i + j];
        }
    }

    for (i = 0; i < 12; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool Hamming_12_8_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords)
{
    bool correctable = true;
    int ic = 0;
    int is = 0;
    int syndromeI = 0; // syndrome index

    for (ic = 0; ic < nbCodewords; ic++)
    {
        // calculate syndrome

        syndromeI = 0; // syndrome index

        for (is = 0; is < 4; is++)
        {
            syndromeI += (((rxBits[12*ic +  0] * Hamming_12_8_m_H[12*is +  0])
                         + (rxBits[12*ic +  1] * Hamming_12_8_m_H[12*is +  1])
                         + (rxBits[12*ic +  2] * Hamming_12_8_m_H[12*is +  2])
                         + (rxBits[12*ic +  3] * Hamming_12_8_m_H[12*is +  3])
                         + (rxBits[12*ic +  4] * Hamming_12_8_m_H[12*is +  4])
                         + (rxBits[12*ic +  5] * Hamming_12_8_m_H[12*is +  5])
                         + (rxBits[12*ic +  6] * Hamming_12_8_m_H[12*is +  6])
                         + (rxBits[12*ic +  7] * Hamming_12_8_m_H[12*is +  7])
                         + (rxBits[12*ic +  8] * Hamming_12_8_m_H[12*is +  8])
                         + (rxBits[12*ic +  9] * Hamming_12_8_m_H[12*is +  9])
                         + (rxBits[12*ic + 10] * Hamming_12_8_m_H[12*is + 10])
                         + (rxBits[12*ic + 11] * Hamming_12_8_m_H[12*is + 11])) % 2) << (3-is);
        }

        // correct bit

        if (syndromeI > 0) // single bit error correction
        {
            if (Hamming_12_8_m_corr[syndromeI] == 0xFF) // uncorrectable error
            {
                correctable = false;
            }
            else
            {
                rxBits[Hamming_12_8_m_corr[syndromeI]] ^= 1; // flip bit
            }
        }

        // move information bits
        memcpy(&decodedBits[8*ic], &rxBits[12*ic], 8);
    }

    return correctable;
}


// ========================================================================================


void Hamming_13_9_init()
{
    // correctable bit positions given syndrome bits as index (see above)
    memset(Hamming_13_9_m_corr, 0xFF, 16); // initialize with all invalid positions
    Hamming_13_9_m_corr[0b1111] = 0;
    Hamming_13_9_m_corr[0b1110] = 1;
    Hamming_13_9_m_corr[0b0111] = 2;
    Hamming_13_9_m_corr[0b1010] = 3;
    Hamming_13_9_m_corr[0b0101] = 4;
    Hamming_13_9_m_corr[0b1011] = 5;
    Hamming_13_9_m_corr[0b1100] = 6;
    Hamming_13_9_m_corr[0b0110] = 7;
    Hamming_13_9_m_corr[0b0011] = 8;
    Hamming_13_9_m_corr[0b1000] = 9;
    Hamming_13_9_m_corr[0b0100] = 10;
    Hamming_13_9_m_corr[0b0010] = 11;
    Hamming_13_9_m_corr[0b0001] = 12;
}

// Not very efficient but encode is used for unit testing only
void Hamming_13_9_encode(unsigned char *origBits, unsigned char *encodedBits)
{
  int i = 0, j = 0;
  memset(encodedBits, 0, 13);

  for (i = 0; i < 9; i++)
  {
    for (j = 0; j < 13; j++)
    {
      encodedBits[j] += origBits[i] * Hamming_13_9_m_G[13*i + j];
    }
  }

  for (i = 0; i < 13; i++)
  {
    encodedBits[i] %= 2;
  }
}

bool Hamming_13_9_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords)
{
    bool correctable = true;
    int ic = 0, is = 0;
    int syndromeI = 0; // syndrome index

  for (ic = 0; ic < nbCodewords; ic++)
  {
    // calculate syndrome

    syndromeI = 0; // syndrome index

    for (is = 0; is < 4; is++)
    {
      syndromeI += (((rxBits[13*ic +  0] * Hamming_13_9_m_H[13*is +  0])
                   + (rxBits[13*ic +  1] * Hamming_13_9_m_H[13*is +  1])
                   + (rxBits[13*ic +  2] * Hamming_13_9_m_H[13*is +  2])
                   + (rxBits[13*ic +  3] * Hamming_13_9_m_H[13*is +  3])
                   + (rxBits[13*ic +  4] * Hamming_13_9_m_H[13*is +  4])
                   + (rxBits[13*ic +  5] * Hamming_13_9_m_H[13*is +  5])
                   + (rxBits[13*ic +  6] * Hamming_13_9_m_H[13*is +  6])
                   + (rxBits[13*ic +  7] * Hamming_13_9_m_H[13*is +  7])
                   + (rxBits[13*ic +  8] * Hamming_13_9_m_H[13*is +  8])
                   + (rxBits[13*ic +  9] * Hamming_13_9_m_H[13*is +  9])
                   + (rxBits[13*ic + 10] * Hamming_13_9_m_H[13*is + 10])
                   + (rxBits[13*ic + 11] * Hamming_13_9_m_H[13*is + 11])
                   + (rxBits[13*ic + 12] * Hamming_13_9_m_H[13*is + 12])) % 2) << (3-is);
    }

    // correct bit

    if (syndromeI > 0) // single bit error correction
    {
      if (Hamming_13_9_m_corr[syndromeI] == 0xFF) // uncorrectable error
      {
        correctable = false;
        break;
      }
      else
      {
        rxBits[Hamming_13_9_m_corr[syndromeI]] ^= 1; // flip bit
      }
    }

    // move information bits
    if (decodedBits)
    {
      memcpy(&decodedBits[9*ic], &rxBits[13*ic], 9);
    }
  }

  return correctable;
}


// ========================================================================================


void Hamming_15_11_init()
{
    // correctable bit positions given syndrome bits as index (see above)
    memset(Hamming_15_11_m_corr, 0xFF, 16); // initialize with all invalid positions
    Hamming_15_11_m_corr[0b1001] = 0;
    Hamming_15_11_m_corr[0b1101] = 1;
    Hamming_15_11_m_corr[0b1111] = 2;
    Hamming_15_11_m_corr[0b1110] = 3;
    Hamming_15_11_m_corr[0b0111] = 4;
    Hamming_15_11_m_corr[0b1010] = 5;
    Hamming_15_11_m_corr[0b0101] = 6;
    Hamming_15_11_m_corr[0b1011] = 7;
    Hamming_15_11_m_corr[0b1100] = 8;
    Hamming_15_11_m_corr[0b0110] = 9;
    Hamming_15_11_m_corr[0b0011] = 10;
    Hamming_15_11_m_corr[0b1000] = 11;
    Hamming_15_11_m_corr[0b0100] = 12;
    Hamming_15_11_m_corr[0b0010] = 13;
    Hamming_15_11_m_corr[0b0001] = 14;
}

// Not very efficient but encode is used for unit testing only
void Hamming_15_11_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;
    memset(encodedBits, 0, 15);

    for (i = 0; i < 11; i++)
    {
        for (j = 0; j < 15; j++)
        {
            encodedBits[j] += origBits[i] * Hamming_15_11_m_G[15*i + j];
        }
    }

    for (i = 0; i < 15; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool Hamming_15_11_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords)
{
    bool correctable = true;
    int ic = 0, is = 0;
    int syndromeI = 0; // syndrome index

    for (ic = 0; ic < nbCodewords; ic++)
    {
        // calculate syndrome

        syndromeI = 0; // syndrome index

        for (is = 0; is < 4; is++)
        {
            syndromeI += (((rxBits[15*ic +  0] * Hamming_15_11_m_H[15*is +  0])
                         + (rxBits[15*ic +  1] * Hamming_15_11_m_H[15*is +  1])
                         + (rxBits[15*ic +  2] * Hamming_15_11_m_H[15*is +  2])
                         + (rxBits[15*ic +  3] * Hamming_15_11_m_H[15*is +  3])
                         + (rxBits[15*ic +  4] * Hamming_15_11_m_H[15*is +  4])
                         + (rxBits[15*ic +  5] * Hamming_15_11_m_H[15*is +  5])
                         + (rxBits[15*ic +  6] * Hamming_15_11_m_H[15*is +  6])
                         + (rxBits[15*ic +  7] * Hamming_15_11_m_H[15*is +  7])
                         + (rxBits[15*ic +  8] * Hamming_15_11_m_H[15*is +  8])
                         + (rxBits[15*ic +  9] * Hamming_15_11_m_H[15*is +  9])
                         + (rxBits[15*ic + 10] * Hamming_15_11_m_H[15*is + 10])
                         + (rxBits[15*ic + 11] * Hamming_15_11_m_H[15*is + 11])
                         + (rxBits[15*ic + 12] * Hamming_15_11_m_H[15*is + 12])
                         + (rxBits[15*ic + 13] * Hamming_15_11_m_H[15*is + 13])
                         + (rxBits[15*ic + 14] * Hamming_15_11_m_H[15*is + 14])) % 2) << (3-is);
        }

        // correct bit

        if (syndromeI > 0) // single bit error correction
        {
            if (Hamming_15_11_m_corr[syndromeI] == 0xFF) // uncorrectable error
            {
                correctable = false;
                break;
            }
            else
            {
                rxBits[Hamming_15_11_m_corr[syndromeI]] ^= 1; // flip bit
            }
        }

        // move information bits
        if (decodedBits)
        {
            memcpy(&decodedBits[11*ic], &rxBits[15*ic], 11);
        }
    }

    return correctable;
}


// ========================================================================================


void Hamming_16_11_4_init()
{
    // correctable bit positions given syndrome bits as index (see above)
    memset(Hamming_16_11_4_m_corr, 0xFF, 32); // initialize with all invalid positions
    Hamming_16_11_4_m_corr[0b10011] = 0;
    Hamming_16_11_4_m_corr[0b11010] = 1;
    Hamming_16_11_4_m_corr[0b11111] = 2;
    Hamming_16_11_4_m_corr[0b11100] = 3;
    Hamming_16_11_4_m_corr[0b01110] = 4;
    Hamming_16_11_4_m_corr[0b10101] = 5;
    Hamming_16_11_4_m_corr[0b01011] = 6;
    Hamming_16_11_4_m_corr[0b10110] = 7;
    Hamming_16_11_4_m_corr[0b11001] = 8;
    Hamming_16_11_4_m_corr[0b01101] = 9;
    Hamming_16_11_4_m_corr[0b00111] = 10;
    Hamming_16_11_4_m_corr[0b10000] = 11;
    Hamming_16_11_4_m_corr[0b01000] = 12;
    Hamming_16_11_4_m_corr[0b00100] = 13;
    Hamming_16_11_4_m_corr[0b00010] = 14;
    Hamming_16_11_4_m_corr[0b00001] = 15;
}

// Not very efficient but encode is used for unit testing only
void Hamming_16_11_4_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;
    memset(encodedBits, 0, 16);

    for (i = 0; i < 11; i++)
    {
        for (j = 0; j < 16; j++)
        {
            encodedBits[j] += origBits[i] * Hamming_16_11_4_m_G[16*i + j];
        }
    }

    for (i = 0; i < 16; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool Hamming_16_11_4_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords)
{
    bool correctable = true;
    int ic = 0, is = 0;
    int syndromeI = 0; // syndrome index

    for (ic = 0; ic < nbCodewords; ic++)
    {
        // calculate syndrome
        syndromeI = 0; // syndrome index
        for (is = 0; is < 5; is++)
        {
            syndromeI += (((rxBits[16*ic +  0] * Hamming_16_11_4_m_H[16*is +  0])
                         + (rxBits[16*ic +  1] * Hamming_16_11_4_m_H[16*is +  1])
                         + (rxBits[16*ic +  2] * Hamming_16_11_4_m_H[16*is +  2])
                         + (rxBits[16*ic +  3] * Hamming_16_11_4_m_H[16*is +  3])
                         + (rxBits[16*ic +  4] * Hamming_16_11_4_m_H[16*is +  4])
                         + (rxBits[16*ic +  5] * Hamming_16_11_4_m_H[16*is +  5])
                         + (rxBits[16*ic +  6] * Hamming_16_11_4_m_H[16*is +  6])
                         + (rxBits[16*ic +  7] * Hamming_16_11_4_m_H[16*is +  7])
                         + (rxBits[16*ic +  8] * Hamming_16_11_4_m_H[16*is +  8])
                         + (rxBits[16*ic +  9] * Hamming_16_11_4_m_H[16*is +  9])
                         + (rxBits[16*ic + 10] * Hamming_16_11_4_m_H[16*is + 10])
                         + (rxBits[16*ic + 11] * Hamming_16_11_4_m_H[16*is + 11])
                         + (rxBits[16*ic + 12] * Hamming_16_11_4_m_H[16*is + 12])
                         + (rxBits[16*ic + 13] * Hamming_16_11_4_m_H[16*is + 13])
                         + (rxBits[16*ic + 14] * Hamming_16_11_4_m_H[16*is + 14])
                         + (rxBits[16*ic + 15] * Hamming_16_11_4_m_H[16*is + 15])) % 2) << (4-is);
        }

        // correct bit

        if (syndromeI > 0) // single bit error correction
        {
            if (Hamming_16_11_4_m_corr[syndromeI] == 0xFF) // uncorrectable error
            {
                correctable = false;
                break;
            }
            else
            {
                rxBits[Hamming_16_11_4_m_corr[syndromeI]] ^= 1; // flip bit
            }
        }

        // move information bits
        if (decodedBits)
        {
            memcpy(&decodedBits[11*ic], &rxBits[16*ic], 11);
        }
    }

    return correctable;
}


// ========================================================================================

void rand_test_20_8() //from dforsi as a pull request to DSDcc
{
    unsigned char msg[8];
    unsigned char codeword[20], xcodeword[20];
    int idx1, idx2, idx3 = 0;
    int dataIn, dataOut; UNUSED(dataIn); //don't know why its reporting dataIn as unused for
    int passCount = 0, failCount = 0, parityFailCount = 0;

    // Run multiple times, to randomly corrupt different bits
    // Takes about 10 seconds on a fast PC
    for (int repeat = 0; repeat < 100000/2; repeat++)
    {
        // Exhaustively test all 8-bit inputs
        for (int dataIn = 0; dataIn < 256; dataIn++)
        {
            // Convert to array of bits
            for (int j = 0; j < 8; j++) {
                msg[j] = (dataIn >> j) & 1;
            }

            // Encode
            Golay_20_8_encode(msg, codeword);

            // Save copy of uncorrupted codeword
            memcpy(xcodeword, codeword, 20*sizeof(unsigned char));

            //change the seed each time, using time(NULL) will only change the seed once every second, otherwise resets it to the same seed
            srand(dataIn+dataOut);

            // Randomly corrupt up to 3 bits
            idx1 = rand() % 20;
            idx2 = rand() % 20;
            idx3 = rand() % 20;
            codeword[idx1] ^= codeword[idx1];
            codeword[idx2] ^= codeword[idx2];
            codeword[idx3] ^= codeword[idx3];

            //flip the parity bit
            // codeword[19] ^= 1;

            bool fail = false;
            // Decode and correct errors
            dataOut = 0;
            if (Golay_20_8_decode(codeword))
            {
                // Check data is corrected
                for (int j = 0; j < 8; j++) {
                    dataOut |= codeword[j] << j;
                }
                if (dataIn != dataOut) {
                    fail = true;
                }

                // Check also that parity has been corrected, as we previously had a bug with this
                if (memcmp(codeword, xcodeword, 20)) {
                    parityFailCount++;
                }
            }
            else
            {
                fail = true;
            }
            if (fail)
            {
                fprintf (stderr, "\n Decode Failed: IN:%05X; OUT:%05X; IDX: %d, %d, %d; ", dataIn, dataOut, idx1, idx2, idx3);
                failCount++;
            }
            else
            {
                passCount++;
                // fprintf (stderr, "\n Decode Passed: IN:%05X; OUT:%05X; IDX: %d, %d, %d; ", dataIn, dataOut, idx1, idx2, idx3);
            }
        }
    }
    fprintf (stderr, "\n Passcount: %d; Failcount: %d; Parity Fail Count: %d \n", passCount, failCount, parityFailCount);
}

void Golay_20_8_init()
{
    int i1 = 0, i2 = 0, i3 = 0, ir = 0, ip = 0;
    int syndromeI = 0, syndromeIP = 0;
    int ip1 = 0, ip2 = 0, ip3 = 0;
    int syndromeIP1 = 0, syndromeIP2 = 0, syndromeIP3 = 0;

    memset(Golay_20_8_m_corr, 0xFF, 3*4096);

    for (i1 = 0; i1 < 8; i1++)
    {
        for (i2 = i1+1; i2 < 8; i2++)
        {
            for (i3 = i2+1; i3 < 8; i3++)
            {
                // 3 bit patterns
                syndromeI = 0;

                for (ir = 0; ir < 12; ir++)
                {
                    syndromeI += ((Golay_20_8_m_H[20*ir + i1] +  Golay_20_8_m_H[20*ir + i2] +  Golay_20_8_m_H[20*ir + i3]) % 2) << (11-ir);
                }

                Golay_20_8_m_corr[syndromeI][0] = i1;
                Golay_20_8_m_corr[syndromeI][1] = i2;
                Golay_20_8_m_corr[syndromeI][2] = i3;
            }

            // 2 bit patterns
            syndromeI = 0;

            for (ir = 0; ir < 12; ir++)
            {
                syndromeI += ((Golay_20_8_m_H[20*ir + i1] +  Golay_20_8_m_H[20*ir + i2]) % 2) << (11-ir);
            }

            Golay_20_8_m_corr[syndromeI][0] = i1;
            Golay_20_8_m_corr[syndromeI][1] = i2;

            // 1 possible bit flip left in the parity part
            for (ip = 0; ip < 12; ip++)
            {
                syndromeIP = syndromeI ^ (1 << (11-ip));
                Golay_20_8_m_corr[syndromeIP][0] = i1;
                Golay_20_8_m_corr[syndromeIP][1] = i2;
                Golay_20_8_m_corr[syndromeIP][2] = 8 + ip;
            }
        }

        // single bit patterns
        syndromeI = 0;

        for (ir = 0; ir < 12; ir++)
        {
            syndromeI += Golay_20_8_m_H[20*ir + i1] << (11-ir);
        }

        Golay_20_8_m_corr[syndromeI][0] = i1;

        for (ip1 = 0; ip1 < 12; ip1++) // 1 more bit flip in parity
        {
            syndromeIP1 = syndromeI ^ (1 << (11-ip1));
            Golay_20_8_m_corr[syndromeIP1][0] = i1;
            Golay_20_8_m_corr[syndromeIP1][1] = 8 + ip1;

            for (ip2 = ip1+1; ip2 < 12; ip2++) // 1 more bit flip in parity
            {
                syndromeIP2 = syndromeIP1 ^ (1 << (11-ip2));
                Golay_20_8_m_corr[syndromeIP2][0] = i1;
                Golay_20_8_m_corr[syndromeIP2][1] = 8 + ip1;
                Golay_20_8_m_corr[syndromeIP2][2] = 8 + ip2;
            }
        }
    }

    // no bit patterns (in message) -> all in parity
    for (ip1 = 0; ip1 < 12; ip1++) // 1 bit flip in parity
    {
        syndromeIP1 =  (1 << (11-ip1));
        Golay_20_8_m_corr[syndromeIP1][0] = 8 + ip1;

        for (ip2 = ip1+1; ip2 < 12; ip2++) // 1 more bit flip in parity
        {
            syndromeIP2 = syndromeIP1 ^ (1 << (11-ip2));
            Golay_20_8_m_corr[syndromeIP2][0] = 8 + ip1;
            Golay_20_8_m_corr[syndromeIP2][1] = 8 + ip2;

            for (ip3 = ip2+1; ip3 < 12; ip3++) // 1 more bit flip in parity
            {
                syndromeIP3 = syndromeIP2 ^ (1 << (11-ip3));
                Golay_20_8_m_corr[syndromeIP3][0] = 8 + ip1;
                Golay_20_8_m_corr[syndromeIP3][1] = 8 + ip2;
                Golay_20_8_m_corr[syndromeIP3][2] = 8 + ip3;
            }
        }
    }

    // rand_test_20_8(); //test
}

// Not very efficient but encode is used for unit testing only
void Golay_20_8_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    memset(encodedBits, 0, 20);

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 20; j++)
        {
            encodedBits[j] += origBits[i] * Golay_20_8_m_G[20*i + j];
        }
    }

    for (i = 0; i < 20; i++)
    {
        encodedBits[i] %= 2;
    }
}

// golay (20,8) hamming-weight of 6 reliably corrects at most 2 bit-errors
bool Golay_20_8_decode(unsigned char *rxBits)
{
    unsigned int syndromeI = 0; // syndrome index
    int is = 0;
    int i = 0;
    //tally corrections made, exceeding 2 will return false
    int correction = 0; 

    for (is = 0; is < 12; is++)
    {
        syndromeI += (((rxBits[0]  * Golay_20_8_m_H[20*is + 0])
                     + (rxBits[1]  * Golay_20_8_m_H[20*is + 1])
                     + (rxBits[2]  * Golay_20_8_m_H[20*is + 2])
                     + (rxBits[3]  * Golay_20_8_m_H[20*is + 3])
                     + (rxBits[4]  * Golay_20_8_m_H[20*is + 4])
                     + (rxBits[5]  * Golay_20_8_m_H[20*is + 5])
                     + (rxBits[6]  * Golay_20_8_m_H[20*is + 6])
                     + (rxBits[7]  * Golay_20_8_m_H[20*is + 7])
                     + (rxBits[8]  * Golay_20_8_m_H[20*is + 8])
                     + (rxBits[9]  * Golay_20_8_m_H[20*is + 9])
                     + (rxBits[10] * Golay_20_8_m_H[20*is + 10])
                     + (rxBits[11] * Golay_20_8_m_H[20*is + 11])
                     + (rxBits[12] * Golay_20_8_m_H[20*is + 12])
                     + (rxBits[13] * Golay_20_8_m_H[20*is + 13])
                     + (rxBits[14] * Golay_20_8_m_H[20*is + 14])
                     + (rxBits[15] * Golay_20_8_m_H[20*is + 15])
                     + (rxBits[16] * Golay_20_8_m_H[20*is + 16])
                     + (rxBits[17] * Golay_20_8_m_H[20*is + 17])
                     + (rxBits[18] * Golay_20_8_m_H[20*is + 18])
                     + (rxBits[19] * Golay_20_8_m_H[20*is + 19])) % 2) << (11-is);
    }

    if (syndromeI > 0)
    {
        i = 0;

        for (; i < 3; i++)
        {
            if (Golay_20_8_m_corr[syndromeI][i] == 0xFF)
            {
                break;
            }
            else
            {
                rxBits[Golay_20_8_m_corr[syndromeI][i]] ^= 1; // flip bit
                correction++;
            }
        }

        if (i == 0)
        {
            return false;
        }

        //return false due to exceeding the number of allowed corrected bits
        //being stingy and only allowing 1 error, may wreck some good data decodes, 
        //decided to play it safe with 2
        if (correction > 2)
        {
            return false; 
        }
    }

    return true;
}


// ========================================================================================


void Golay_23_12_init()
{
    int i1 = 0, i2 = 0, i3 = 0, ir = 0, ip = 0;
    int syndromeI = 0, syndromeIP = 0;
    int ip1 = 0, ip2 = 0, ip3 = 0;
    int syndromeIP1 = 0, syndromeIP2 = 0, syndromeIP3 = 0;

    memset(Golay_23_12_m_corr, 0xFF, 3*2048);

    for (i1 = 0; i1 < 12; i1++)
    {
        for (i2 = i1+1; i2 < 12; i2++)
        {
            for (i3 = i2+1; i3 < 12; i3++)
            {
                // 3 bit patterns
                syndromeI = 0;

                for (ir = 0; ir < 11; ir++)
                {
                    syndromeI += ((Golay_23_12_m_H[23*ir + i1] +  Golay_23_12_m_H[23*ir + i2] +  Golay_23_12_m_H[23*ir + i3]) % 2) << (10-ir);
                }

                Golay_23_12_m_corr[syndromeI][0] = i1;
                Golay_23_12_m_corr[syndromeI][1] = i2;
                Golay_23_12_m_corr[syndromeI][2] = i3;
            }

            // 2 bit patterns
            syndromeI = 0;

            for (ir = 0; ir < 11; ir++)
            {
                syndromeI += ((Golay_23_12_m_H[23*ir + i1] +  Golay_23_12_m_H[23*ir + i2]) % 2) << (10-ir);
            }

            Golay_23_12_m_corr[syndromeI][0] = i1;
            Golay_23_12_m_corr[syndromeI][1] = i2;

            // 1 possible bit flip left in the parity part
            for (ip = 0; ip < 11; ip++)
            {
                syndromeIP = syndromeI ^ (1 << (10-ip));
                Golay_23_12_m_corr[syndromeIP][0] = i1;
                Golay_23_12_m_corr[syndromeIP][1] = i2;
                Golay_23_12_m_corr[syndromeIP][2] = 12 + ip;
            }
        }

        // single bit patterns
        syndromeI = 0;

        for (ir = 0; ir < 11; ir++)
        {
            syndromeI += Golay_23_12_m_H[23*ir + i1] << (10-ir);
        }

        Golay_23_12_m_corr[syndromeI][0] = i1;

        for (ip1 = 0; ip1 < 11; ip1++) // 1 more bit flip in parity
        {
            syndromeIP1 = syndromeI ^ (1 << (10-ip1));
            Golay_23_12_m_corr[syndromeIP1][0] = i1;
            Golay_23_12_m_corr[syndromeIP1][1] = 12 + ip1;

            for (ip2 = ip1+1; ip2 < 11; ip2++) // 1 more bit flip in parity
            {
                syndromeIP2 = syndromeIP1 ^ (1 << (10-ip2));
                Golay_23_12_m_corr[syndromeIP2][0] = i1;
                Golay_23_12_m_corr[syndromeIP2][1] = 12 + ip1;
                Golay_23_12_m_corr[syndromeIP2][2] = 12 + ip2;
            }
        }
    }

    // no bit patterns (in message) -> all in parity
    for (ip1 = 0; ip1 < 11; ip1++) // 1 bit flip in parity
    {
        syndromeIP1 =  (1 << (10-ip1));
        Golay_23_12_m_corr[syndromeIP1][0] = 12 + ip1;

        for (ip2 = ip1+1; ip2 < 11; ip2++) // 1 more bit flip in parity
        {
            syndromeIP2 = syndromeIP1 ^ (1 << (10-ip2));
            Golay_23_12_m_corr[syndromeIP2][0] = 12 + ip1;
            Golay_23_12_m_corr[syndromeIP2][1] = 12 + ip2;

            for (ip3 = ip2+1; ip3 < 11; ip3++) // 1 more bit flip in parity
            {
                syndromeIP3 = syndromeIP2 ^ (1 << (10-ip3));
                Golay_23_12_m_corr[syndromeIP3][0] = 12 + ip1;
                Golay_23_12_m_corr[syndromeIP3][1] = 12 + ip2;
                Golay_23_12_m_corr[syndromeIP3][2] = 12 + ip3;
            }
        }
    }
}

// Not very efficient but encode is used for unit testing only
void Golay_23_12_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    memset(encodedBits, 0, 23);

    for (i = 0; i < 12; i++) // orig bits
    {
        for (j = 0; j < 23; j++) // codeword bits
        {
            encodedBits[j] += origBits[i] * Golay_23_12_m_G[23*i + j];
        }
    }

    for (i = 0; i < 23; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool Golay_23_12_decode(unsigned char *rxBits)
{
    unsigned int syndromeI = 0; // syndrome index
    int is = 0;
    int i = 0;

    for (is = 0; is < 11; is++)
    {
        syndromeI += (((rxBits[0]  * Golay_23_12_m_H[23*is + 0])
                     + (rxBits[1]  * Golay_23_12_m_H[23*is + 1])
                     + (rxBits[2]  * Golay_23_12_m_H[23*is + 2])
                     + (rxBits[3]  * Golay_23_12_m_H[23*is + 3])
                     + (rxBits[4]  * Golay_23_12_m_H[23*is + 4])
                     + (rxBits[5]  * Golay_23_12_m_H[23*is + 5])
                     + (rxBits[6]  * Golay_23_12_m_H[23*is + 6])
                     + (rxBits[7]  * Golay_23_12_m_H[23*is + 7])
                     + (rxBits[8]  * Golay_23_12_m_H[23*is + 8])
                     + (rxBits[9]  * Golay_23_12_m_H[23*is + 9])
                     + (rxBits[10] * Golay_23_12_m_H[23*is + 10])
                     + (rxBits[11] * Golay_23_12_m_H[23*is + 11])
                     + (rxBits[12] * Golay_23_12_m_H[23*is + 12])
                     + (rxBits[13] * Golay_23_12_m_H[23*is + 13])
                     + (rxBits[14] * Golay_23_12_m_H[23*is + 14])
                     + (rxBits[15] * Golay_23_12_m_H[23*is + 15])
                     + (rxBits[16] * Golay_23_12_m_H[23*is + 16])
                     + (rxBits[17] * Golay_23_12_m_H[23*is + 17])
                     + (rxBits[18] * Golay_23_12_m_H[23*is + 18])
                     + (rxBits[19] * Golay_23_12_m_H[23*is + 19])
                     + (rxBits[20] * Golay_23_12_m_H[23*is + 20])
                     + (rxBits[21] * Golay_23_12_m_H[23*is + 21])
                     + (rxBits[22] * Golay_23_12_m_H[23*is + 22])) % 2) << (10-is);
    }

    if (syndromeI > 0)
    {
        i = 0;

        for (; i < 3; i++)
        {
            if (Golay_23_12_m_corr[syndromeI][i] == 0xFF)
            {
                break;
            }
            else
            {
                rxBits[Golay_23_12_m_corr[syndromeI][i]] ^= 1; // flip bit
            }
        }

        if (i == 0)
        {
            return false;
        }
    }

    return true;
}


// ========================================================================================


void Golay_24_12_init()
{
    int i1 = 0, i2 = 0, i3 = 0, ir = 0, ip = 0;
    int syndromeI = 0, syndromeIP = 0;
    int ip1 = 0, ip2 = 0, ip3 = 0;
    int syndromeIP1 = 0, syndromeIP2 = 0, syndromeIP3 = 0;

    memset (Golay_24_12_m_corr, 0xFF, 3*4096);

    for (i1 = 0; i1 < 12; i1++)
    {
        for (i2 = i1+1; i2 < 12; i2++)
        {
            for (i3 = i2+1; i3 < 12; i3++)
            {
                // 3 bit patterns
                syndromeI = 0;

                for (ir = 0; ir < 12; ir++)
                {
                    syndromeI += ((Golay_24_12_m_H[24*ir + i1] +  Golay_24_12_m_H[24*ir + i2] +  Golay_24_12_m_H[24*ir + i3]) % 2) << (11-ir);
                }

                Golay_24_12_m_corr[syndromeI][0] = i1;
                Golay_24_12_m_corr[syndromeI][1] = i2;
                Golay_24_12_m_corr[syndromeI][2] = i3;
            }

            // 2 bit patterns
            syndromeI = 0;

            for (ir = 0; ir < 12; ir++)
            {
                syndromeI += ((Golay_24_12_m_H[24*ir + i1] +  Golay_24_12_m_H[24*ir + i2]) % 2) << (11-ir);
            }

            Golay_24_12_m_corr[syndromeI][0] = i1;
            Golay_24_12_m_corr[syndromeI][1] = i2;

            // 1 possible bit flip left in the parity part
            for (ip = 0; ip < 12; ip++)
            {
                syndromeIP = syndromeI ^ (1 << (11-ip));
                Golay_24_12_m_corr[syndromeIP][0] = i1;
                Golay_24_12_m_corr[syndromeIP][1] = i2;
                Golay_24_12_m_corr[syndromeIP][2] = 12 + ip;
            }
        }

        // single bit patterns
        syndromeI = 0;

        for (ir = 0; ir < 12; ir++)
        {
            syndromeI += Golay_24_12_m_H[24*ir + i1] << (11-ir);
        }

        Golay_24_12_m_corr[syndromeI][0] = i1;

        for (ip1 = 0; ip1 < 12; ip1++) // 1 more bit flip in parity
        {
            syndromeIP1 = syndromeI ^ (1 << (11-ip1));
            Golay_24_12_m_corr[syndromeIP1][0] = i1;
            Golay_24_12_m_corr[syndromeIP1][1] = 12 + ip1;

            for (ip2 = ip1+1; ip2 < 12; ip2++) // 1 more bit flip in parity
            {
                syndromeIP2 = syndromeIP1 ^ (1 << (11-ip2));
                Golay_24_12_m_corr[syndromeIP2][0] = i1;
                Golay_24_12_m_corr[syndromeIP2][1] = 12 + ip1;
                Golay_24_12_m_corr[syndromeIP2][2] = 12 + ip2;
            }
        }
    }

    // no bit patterns (in message) -> all in parity
    for (ip1 = 0; ip1 < 12; ip1++) // 1 bit flip in parity
    {
        syndromeIP1 =  (1 << (11-ip1));
        Golay_24_12_m_corr[syndromeIP1][0] = 12 + ip1;

        for (ip2 = ip1+1; ip2 < 12; ip2++) // 1 more bit flip in parity
        {
            syndromeIP2 = syndromeIP1 ^ (1 << (11-ip2));
            Golay_24_12_m_corr[syndromeIP2][0] = 12 + ip1;
            Golay_24_12_m_corr[syndromeIP2][1] = 12 + ip2;

            for (ip3 = ip2+1; ip3 < 12; ip3++) // 1 more bit flip in parity
            {
                syndromeIP3 = syndromeIP2 ^ (1 << (11-ip3));
                Golay_24_12_m_corr[syndromeIP3][0] = 12 + ip1;
                Golay_24_12_m_corr[syndromeIP3][1] = 12 + ip2;
                Golay_24_12_m_corr[syndromeIP3][2] = 12 + ip3;
            }
        }
    }
}

// Not very efficient but encode is used for unit testing only, I think this is wrong for M17
void Golay_24_12_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    // memset(encodedBits, 0, 24);

    for (i = 0; i < 12; i++)
    {
        for (j = 0; j < 24; j++)
        {
            encodedBits[j] += origBits[i] * Golay_24_12_m_G[24*i + j];
        }
    }
    // fprintf (stderr, "\n Inside Golay - Encoded Bits = "); //disable later on
    for (i = 0; i < 24; i++)
    {
        encodedBits[i] %= 2;
    //     fprintf (stderr, "%b", encodedBits[i]); //disable later on
    }
}

bool Golay_24_12_decode(unsigned char *rxBits)
{
    unsigned int syndromeI = 0; // syndrome index
    int is = 0;
    int i = 0;

    for (is = 0; is < 12; is++)
    {
        syndromeI += (((rxBits[0]  * Golay_24_12_m_H[24*is + 0])
                     + (rxBits[1]  * Golay_24_12_m_H[24*is + 1])
                     + (rxBits[2]  * Golay_24_12_m_H[24*is + 2])
                     + (rxBits[3]  * Golay_24_12_m_H[24*is + 3])
                     + (rxBits[4]  * Golay_24_12_m_H[24*is + 4])
                     + (rxBits[5]  * Golay_24_12_m_H[24*is + 5])
                     + (rxBits[6]  * Golay_24_12_m_H[24*is + 6])
                     + (rxBits[7]  * Golay_24_12_m_H[24*is + 7])
                     + (rxBits[8]  * Golay_24_12_m_H[24*is + 8])
                     + (rxBits[9]  * Golay_24_12_m_H[24*is + 9])
                     + (rxBits[10] * Golay_24_12_m_H[24*is + 10])
                     + (rxBits[11] * Golay_24_12_m_H[24*is + 11])
                     + (rxBits[12] * Golay_24_12_m_H[24*is + 12])
                     + (rxBits[13] * Golay_24_12_m_H[24*is + 13])
                     + (rxBits[14] * Golay_24_12_m_H[24*is + 14])
                     + (rxBits[15] * Golay_24_12_m_H[24*is + 15])
                     + (rxBits[16] * Golay_24_12_m_H[24*is + 16])
                     + (rxBits[17] * Golay_24_12_m_H[24*is + 17])
                     + (rxBits[18] * Golay_24_12_m_H[24*is + 18])
                     + (rxBits[19] * Golay_24_12_m_H[24*is + 19])
                     + (rxBits[20] * Golay_24_12_m_H[24*is + 20])
                     + (rxBits[21] * Golay_24_12_m_H[24*is + 21])
                     + (rxBits[22] * Golay_24_12_m_H[24*is + 22])
                     + (rxBits[23] * Golay_24_12_m_H[24*is + 23])) % 2) << (11-is);
    }

    if (syndromeI > 0)
    {
        i = 0;

        for (; i < 3; i++)
        {
            if (Golay_24_12_m_corr[syndromeI][i] == 0xFF)
            {
                break;
            }
            else
            {
                rxBits[Golay_24_12_m_corr[syndromeI][i]] ^= 1; // flip bit
            }
        }

        if (i == 0)
        {
            return false;
        }
    }

    return true;
}


// ========================================================================================


void QR_16_7_6_init()
{
    int i1 = 0, i2 = 0, ir = 0, ip = 0;
    int syndromeI = 0, syndromeIP = 0;
    int ip1 = 0, ip2 = 0;
    int syndromeIP1 = 0, syndromeIP2 = 0;

    memset(QR_16_7_6_m_corr, 0xFF, 2*512);

    for (i1 = 0; i1 < 7; i1++)
    {
        for (i2 = i1+1; i2 < 7; i2++)
        {
            // 2 bit patterns
            syndromeI = 0;

            for (ir = 0; ir < 9; ir++)
            {
                syndromeI += ((QR_16_7_6_m_H[16*ir + i1] +  QR_16_7_6_m_H[16*ir + i2]) % 2) << (8-ir);
            }

            QR_16_7_6_m_corr[syndromeI][0] = i1;
            QR_16_7_6_m_corr[syndromeI][1] = i2;
        }

        // single bit patterns
        syndromeI = 0;

        for (ir = 0; ir < 9; ir++)
        {
            syndromeI += QR_16_7_6_m_H[16*ir + i1] << (8-ir);
        }

        QR_16_7_6_m_corr[syndromeI][0] = i1;

        // 1 possible bit flip left in the parity part
        for (ip = 0; ip < 9; ip++)
        {
            syndromeIP = syndromeI ^ (1 << (8-ip));
            QR_16_7_6_m_corr[syndromeIP][0] = i1;
            QR_16_7_6_m_corr[syndromeIP][1] = 7 + ip;
        }
    }

    // no bit patterns (in message) -> all in parity
    for (ip1 = 0; ip1 < 9; ip1++) // 1 bit flip in parity
    {
        syndromeIP1 = (1 << (8-ip1));
        QR_16_7_6_m_corr[syndromeIP1][0] = 7 + ip1;

        for (ip2 = ip1+1; ip2 < 9; ip2++) // 1 more bit flip in parity
        {
            syndromeIP2 = syndromeIP1 ^ (1 << (8-ip2));
            QR_16_7_6_m_corr[syndromeIP2][0] = 7 + ip1;
            QR_16_7_6_m_corr[syndromeIP2][1] = 7 + ip2;
        }
    }
}

// Not very efficient but encode is used for unit testing only
void QR_16_7_6_encode(unsigned char *origBits, unsigned char *encodedBits)
{
    int i = 0, j = 0;

    memset(encodedBits, 0, 16);

    for (i = 0; i < 7; i++)
    {
        for (j = 0; j < 16; j++)
        {
            encodedBits[j] += origBits[i] * QR_16_7_6_m_G[16*i + j];
        }
    }

    for (i = 0; i < 16; i++)
    {
        encodedBits[i] %= 2;
    }
}

bool QR_16_7_6_decode(unsigned char *rxBits)
{
    //2 bit errors or less
    unsigned int syndromeI = 0; // syndrome index
    int is = 0;
    int i = 0;
    int corrections = 0;
    for (is = 0; is < 9; is++)
    {
        syndromeI += (((rxBits[0]  * QR_16_7_6_m_H[16*is + 0])
                     + (rxBits[1]  * QR_16_7_6_m_H[16*is + 1])
                     + (rxBits[2]  * QR_16_7_6_m_H[16*is + 2])
                     + (rxBits[3]  * QR_16_7_6_m_H[16*is + 3])
                     + (rxBits[4]  * QR_16_7_6_m_H[16*is + 4])
                     + (rxBits[5]  * QR_16_7_6_m_H[16*is + 5])
                     + (rxBits[6]  * QR_16_7_6_m_H[16*is + 6])
                     + (rxBits[7]  * QR_16_7_6_m_H[16*is + 7])
                     + (rxBits[8]  * QR_16_7_6_m_H[16*is + 8])
                     + (rxBits[9]  * QR_16_7_6_m_H[16*is + 9])
                     + (rxBits[10] * QR_16_7_6_m_H[16*is + 10])
                     + (rxBits[11] * QR_16_7_6_m_H[16*is + 11])
                     + (rxBits[12] * QR_16_7_6_m_H[16*is + 12])
                     + (rxBits[13] * QR_16_7_6_m_H[16*is + 13])
                     + (rxBits[14] * QR_16_7_6_m_H[16*is + 14])
                     + (rxBits[15] * QR_16_7_6_m_H[16*is + 15])) % 2) << (8-is);
    }

    if (syndromeI > 0)
    {
        i = 0;

        for (; i < 2; i++)
        {
            if (QR_16_7_6_m_corr[syndromeI][i] == 0xFF)
            {
                break;
            }
            else
            {
                rxBits[QR_16_7_6_m_corr[syndromeI][i]] ^= 1; // flip bit
                corrections++;
            }
        }

        if (i == 0)
        {
            return false;
        }

    }

    //disabling again for now
    // if (corrections > 1) //no more than 1-bit error, else consider failure?
    // {
    //     return false;
    // }

    return true;
}


// ========================================================================================


/* This function init all FEC functions
 * it must be called once at startup */
void InitAllFecFunction(void)
{
  Hamming_7_4_init();
  Hamming_12_8_init();
  Hamming_13_9_init();
  Hamming_15_11_init();
  Hamming_16_11_4_init();
  Golay_20_8_init();
  Golay_23_12_init();
  Golay_24_12_init();
  QR_16_7_6_init();
} /* End InitAllFEC() */


/* End of file */
