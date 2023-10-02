/*-------------------------------------------------------------------------------
 * p25_crc.c
 * P25 Misc CRC checks
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//modified from the LEH ComputeCrcCCITT to accept variable len buffer bits
uint16_t ComputeCrcCCITT16b(const uint8_t buf[], unsigned int len)
{
  uint32_t i;
  uint16_t CRC = 0x0000;
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
	static const uint8_t poly[13] = {1,1,0,0,0,1,0,0,1,0,1,1,1}; // p25 p2 crc 12 poly
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
