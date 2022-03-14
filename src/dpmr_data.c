/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "dsd.h"


/* Convert the Channel code (24 bit) into a valid
 * dPMR color code [0..63]
 * Return -1 on error */
int32_t GetdPmrColorCode(uint8_t ChannelCodeBit[24])
{
  uint32_t ChannelCodeByte;
  int32_t  ColorCode;
  uint32_t i;

  /* Reconstitute the 24 bit channel code into a 3 bytes integer */
  ChannelCodeByte = 0;
  for(i = 0; i < 24; i++)
  {
    ChannelCodeByte <<= 1;
    ChannelCodeByte |= (ChannelCodeBit[i] & 1);
  }

  /* By analyzing all channel code on the
   * dPMR standard, I observe that if we decompose the
   * 24 bit into 12 dibit, all dibit's LSB are always
   * equal to "1", so apply a mask to correct
   * flipped LSB dibit.
   *
   * For more details see ETSI TS 102 658 chapter
   * 6.1.5.2.2 "Channel Code Determined by Frequency
   * and System Identity Code".
   *
   * Apply the mask 0x555555
   * 0x555555 = 0b010101010101010101010101 => All dibit's LSB
   * are equal to "1".
   */
  ChannelCodeByte |= 0x555555;


  /* Switch to the right color code */
  switch(ChannelCodeByte & 0xFF0000)
  {
    case 0x570000:
    {
      switch(ChannelCodeByte)
      {
        case 0x575F77: {ColorCode =   0; break;}
        case 0x577577: {ColorCode =   1; break;}
        case 0x57DD75: {ColorCode =   2; break;}
        case 0x57F775: {ColorCode =   3; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x570000 */

    case 0x550000:
    {
      switch(ChannelCodeByte)
      {
        case 0x55577D: {ColorCode =   4; break;}
        case 0x557D7D: {ColorCode =   5; break;}
        case 0x55D57F: {ColorCode =   6; break;}
        case 0x55FF7F: {ColorCode =   7; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x550000 */

    case 0x5F0000:
    {
      switch(ChannelCodeByte)
      {
        case 0x5F555F: {ColorCode =   8; break;}
        case 0x5F7F5F: {ColorCode =   9; break;}
        case 0x5FD75D: {ColorCode =  10; break;}
        case 0x5FFD5D: {ColorCode =  11; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x5F0000 */

    case 0x5D0000:
    {
      switch(ChannelCodeByte)
      {
        case 0x5D5D55: {ColorCode =  12; break;}
        case 0x5D7755: {ColorCode =  13; break;}
        case 0x5DDF57: {ColorCode =  14; break;}
        case 0x5DF557: {ColorCode =  15; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x5D0000 */

    case 0x770000:
    {
      switch(ChannelCodeByte)
      {
        case 0x775DD7: {ColorCode =  16; break;}
        case 0x7777D7: {ColorCode =  17; break;}
        case 0x77DFD5: {ColorCode =  18; break;}
        case 0x77F5D5: {ColorCode =  19; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x770000 */

    case 0x750000:
    {
      switch(ChannelCodeByte)
      {
        case 0x7555DD: {ColorCode =  20; break;}
        case 0x757FDD: {ColorCode =  21; break;}
        case 0x75D7DF: {ColorCode =  22; break;}
        case 0x75FDDF: {ColorCode =  23; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x750000 */

    case 0x7F0000:
    {
      switch(ChannelCodeByte)
      {
        case 0x7F57FF: {ColorCode =  24; break;}
        case 0x7F7DFF: {ColorCode =  25; break;}
        case 0x7FD5FD: {ColorCode =  26; break;}
        case 0x7FFFFD: {ColorCode =  27; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x7F0000 */

    case 0x7D0000:
    {
      switch(ChannelCodeByte)
      {
        case 0x7D5FF5: {ColorCode =  28; break;}
        case 0x7D75F5: {ColorCode =  29; break;}
        case 0x7DDDF7: {ColorCode =  30; break;}
        case 0x7DF7F7: {ColorCode =  31; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0x7D0000 */

    case 0xD70000:
    {
      switch(ChannelCodeByte)
      {
        case 0xD755F7: {ColorCode =  32; break;}
        case 0xD77FF7: {ColorCode =  33; break;}
        case 0xD7D7F5: {ColorCode =  34; break;}
        case 0xD7FDF5: {ColorCode =  35; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xD70000 */

    case 0xD50000:
    {
      switch(ChannelCodeByte)
      {
        case 0xD55DFD: {ColorCode =  36; break;}
        case 0xD577FD: {ColorCode =  37; break;}
        case 0xD5DFFF: {ColorCode =  38; break;}
        case 0xD5F5FF: {ColorCode =  39; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xD50000 */

    case 0xDF0000:
    {
      switch(ChannelCodeByte)
      {
        case 0xDF5FDF: {ColorCode =  40; break;}
        case 0xDF75DF: {ColorCode =  41; break;}
        case 0xDFDDDD: {ColorCode =  42; break;}
        case 0xDFF7DD: {ColorCode =  43; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xDF0000 */

    case 0xDD0000:
    {
      switch(ChannelCodeByte)
      {
        case 0xDD57D5: {ColorCode =  44; break;}
        case 0xDD7DD5: {ColorCode =  45; break;}
        case 0xDDD5D7: {ColorCode =  46; break;}
        case 0xDDFFD7: {ColorCode =  47; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xDD0000 */

    case 0xF70000:
    {
      switch(ChannelCodeByte)
      {
        case 0xF75757: {ColorCode =  48; break;}
        case 0xF77D57: {ColorCode =  49; break;}
        case 0xF7D555: {ColorCode =  50; break;}
        case 0xF7FF55: {ColorCode =  51; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xF70000 */

    case 0xF50000:
    {
      switch(ChannelCodeByte)
      {
        case 0xF55F5D: {ColorCode =  52; break;}
        case 0xF5755D: {ColorCode =  53; break;}
        case 0xF5DD5F: {ColorCode =  54; break;}
        case 0xF5F75F: {ColorCode =  55; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xF50000 */

    case 0xFF0000:
    {
      switch(ChannelCodeByte)
      {
        case 0xFF5D7F: {ColorCode =  56; break;}
        case 0xFF777F: {ColorCode =  57; break;}
        case 0xFFDF7D: {ColorCode =  58; break;}
        case 0xFFF57D: {ColorCode =  59; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xFF0000 */

    case 0xFD0000:
    {
      switch(ChannelCodeByte)
      {
        case 0xFD5575: {ColorCode =  60; break;}
        case 0xFD7F75: {ColorCode =  61; break;}
        case 0xFDD777: {ColorCode =  62; break;}
        case 0xFDFD77: {ColorCode =  63; break;}
        default:       {ColorCode =  -1; break;}
      } /* End switch(ChannelCodeByte) */
      break;
    } /* End case 0xFD0000 */

    default:
    {
      ColorCode = -1;
      break;
    }
  } /* End switch(ChannelCodeByte & 0xFF0000) */

  /* Return the colour code */
  return ColorCode;
}


/* End of file */
