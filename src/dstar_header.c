/*
*
* This code is taken largely from on1arf's GMSK code. Original copyright below:
*
*
* Copyright (C) 2011 by Kristoff Bonne, ON1ARF
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#include "descramble.h"
#include "dstar_header.h"
#include "dsd.h"
// #include "fcs.h" //error:  multiple definition of `calc_fcs'; CMakeFiles/dsd-fme.dir/src/dstar.c.o:dstar.c:(.text+0x0): first defined here

void dstar_header_decode(dsd_state * state, int radioheaderbuffer[660])
{
	int radioheaderbuffer2[660];
	char radioheader[41];
	int octetcount, bitcount, loop;
	unsigned char bit2octet[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

	scramble(radioheaderbuffer, radioheaderbuffer2);
	deinterleave(radioheaderbuffer2, radioheaderbuffer);
	FECdecoder(radioheaderbuffer, radioheaderbuffer2);
	memset(radioheader, 0, 41);

	// note we receive 330 bits, but we only use 328 of them (41 octets)
	// bits 329 and 330 are unused
	octetcount = 0;
	bitcount = 0;
	for (loop = 0; loop < 328; loop++)
	{
		if (radioheaderbuffer2[loop])
			radioheader[octetcount] |= bit2octet[bitcount];
		bitcount++;
		// increase octetcounter and reset bitcounter every 8 bits
		if (bitcount >= 8)
		{
			octetcount++;
			bitcount = 0;
		}
	}

	char str1[9];
	char str2[9];
	char str3[9];
	char str4[13];

	memcpy (str1, radioheader+3, 8);
	memcpy (str2, radioheader+11, 8);
	memcpy (str3, radioheader+19, 8);
	memcpy (str4, radioheader+27, 12);

	str1[8] = '\0';
	str2[8] = '\0';
	str3[8] = '\0';
	str4[12] = '\0';

	//TODO: Add fcs_calc to header as well
	// uint16_t crc_ext = (radioheader[39] << 8) + radioheader[40];
	// uint16_t crc_cmp = calc_fcs(radioheader, 39);

	//debug
	// fprintf (stderr, "\n HD: ");
	// for (int i = 0; i < 40; i++)
	// 	fprintf (stderr, "%02X ", radioheader[i]);
	// fprintf (stderr, "\n");

	fprintf (stderr, " RPT 2: %s", str1);
	fprintf (stderr, " RPT 1: %s", str2);
	fprintf (stderr, " DST: %s", str3);
	fprintf (stderr, " SRC: %s", str4);

	//check flags for info
	if (radioheader[0] & 0x80) fprintf (stderr, " DATA");
	if (radioheader[0] & 0x40) fprintf (stderr, " REPEATER");
	if (radioheader[0] & 0x20) fprintf (stderr, " INTERRUPTED");
	if (radioheader[0] & 0x10) fprintf (stderr, " CONTROL SIGNAL");
	if (radioheader[0] & 0x08) fprintf (stderr, " URGENT");

	memcpy (state->dstar_rpt2, str1, sizeof(str1));
	memcpy (state->dstar_rpt1, str2, sizeof(str2));
	memcpy (state->dstar_dst, str3, sizeof(str3));
	memcpy (state->dstar_src, str4, sizeof(str4));

	//TODO: Call History for DSTAR
}
