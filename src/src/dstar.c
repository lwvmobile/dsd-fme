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

/*
 * Note: D-STAR support is fairly complete at this point.
 * The ambe3600x2450 decoder is similar butnot compatible with D-STAR voice frames.
 * The dstar interleave pattern is different as well.
 * GMSK modulation optimizations will also required to get a usable bit error
 */

#include "dsd.h"
#include "dstar_const.h"
#include "dstar_header.h"


void processDSTAR(dsd_opts * opts, dsd_state * state) {
	// extracts AMBE frames from D-STAR voice frame
	int i, j, dibit;
	char ambe_fr[4][24];
	unsigned char data[9];
	unsigned int bits[4];
	int framecount;
	int sync_missed = 0;
	unsigned char slowdata[4];
	unsigned int bitbuffer = 0;
	const int *w, *x;

	//set carrier to 1 so ncurses will light up green
	state->carrier = 1;

	if (opts->errorbars == 1) {
		fprintf (stderr, "e:");
	}

#ifdef DSTAR_DUMP
	fprintf (stderr, "\n");
#endif

	if (state->synctype == 18) {
		framecount = 0;
		state->synctype = 6;
	} else if (state->synctype == 19) {
		framecount = 0;
		state->synctype = 7;
	} else {
		framecount = 1; //just saw a sync frame; there should be 20 not 21 till the next
	}
	//why is this running in a loop for?
	while (sync_missed < 3) {

		memset(ambe_fr, 0, 96);
		// voice frame
	    w = dW;
	    x = dX;

		for (i = 0; i < 72; i++) {

			dibit = getDibit(opts, state);

			bitbuffer <<= 1;
			if (dibit == 1) {
				bitbuffer |= 0x01;
			}
			if ((bitbuffer & 0x00FFFFFF) == 0x00AAB468) {
				// we're slipping bits
				fprintf (stderr, "sync in voice after i=%d, restarting\n", i);
				//ugh just start over
				i = 0;
			    w = dW;
			    x = dX;
				framecount = 1;
				continue;
			}

			ambe_fr[*w][*x] = (1 & dibit);
			w++;
			x++;

		}


		processMbeFrame(opts, state, NULL, ambe_fr, NULL);

		//  data frame - 24 bits
		for (i = 73; i < 97; i++) {
			dibit = getDibit(opts, state);
			bitbuffer <<= 1;
			if (dibit == 1) {
				bitbuffer |= 0x01;
			}
			if ((bitbuffer & 0x00FFFFFF) == 0x00AAB468) {
				// looking if we're slipping bits
				if (i != 96) {
					fprintf (stderr, "sync after i=%d\n", i);
					i = 96;
				}
			}
		}

		slowdata[0] = (bitbuffer >> 16) & 0x000000FF;
		slowdata[1] = (bitbuffer >> 8) & 0x000000FF;
		slowdata[2] = (bitbuffer) & 0x000000FF;
		slowdata[3] = 0;

		if ((bitbuffer & 0x00FFFFFF) == 0x00AAB468) {
			//We got sync!
			//fprintf (stderr, "Sync on framecount = %d\n", framecount);
			sync_missed = 0;
		} else if ((bitbuffer & 0x00FFFFFF) == 0xAAAAAA) {
			//End of transmission
			fprintf (stderr, "End of transmission\n");
			goto end;
		} else if (framecount % 21 == 0) {
			fprintf (stderr, "Missed sync on framecount = %d, value = %x/%x/%x\n",
					framecount, slowdata[0], slowdata[1], slowdata[2]);
			sync_missed++;
		} else if (framecount != 0 && (bitbuffer & 0x00FFFFFF) != 0x000000) {
			slowdata[0] ^= 0x70;
			slowdata[1] ^= 0x4f;
			slowdata[2] ^= 0x93;
			//fprintf (stderr, "unscrambled- %02X %02X %02X %02X \n", slowdata[0], slowdata[1], slowdata[2], slowdata[3] );

		} else if (framecount == 0) {
			//fprintf (stderr, "never scrambled-%c\n",slowdata);
			//fprintf (stderr, "never scrambled- %02X %02X %02X %02X \n", slowdata[0], slowdata[1], slowdata[2], slowdata[3] );

			}
		for (short int i = 0; i < 4; i++)
		{
			if (slowdata[i] <= 0x7E && slowdata[i] >=0x20)
	     {
	       //fprintf (stderr, "%c", slowdata[i]);
	     }
	     //else fprintf (stderr, ".");
		}
		//fprintf (stderr, "\n");
		framecount++;

		//since we are in a while loop, run ncursesPrinter here.
	  if (opts->use_ncurses_terminal == 1)
	  {
	    ncursesPrinter(opts, state);
	  }

	} //end while loop

	end: if (opts->errorbars == 1) {
		fprintf (stderr, "\n");
	}
}

void processDSTAR_HD(dsd_opts * opts, dsd_state * state) {

	int i, j;
	int radioheaderbuffer[660];

	for (j = 0; j < 660; j++) {
					radioheaderbuffer[j] = getDibit(opts, state);
			}

	// Note: These routines contain GPLed code. Remove if you object to that.
	// Due to this, they are in a separate source file.
	dstar_header_decode(state, radioheaderbuffer);

	//We officially have sync now, so just pass on to the above routine:

	processDSTAR(opts, state);

}
