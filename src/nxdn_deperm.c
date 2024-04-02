//NXDN descramble/deperm/depuncture and crc/utility functions
//Reworked portions from Osmocom OP25

/* -*- c++ -*- */
/* 
 * NXDN Encoder/Decoder (C) Copyright 2019 Max H. Parke KA1RBI
 * 
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "dsd.h"
#include "nxdn_const.h"

//NOTE: Descrambling was having an issue without a value inside the brackets, but only when
//the PARITY table was disabled, was this due to a memory issue or overflow?
static const uint8_t scramble_t[182] = { //values are the position values we need to invert in the descramble
	2, 5, 6, 7, 10, 12, 14, 16, 17, 22, 23, 25, 26, 27, 28, 30, 33, 34, 36, 37, 38, 41, 45, 47,
	52, 54, 56, 57, 59, 62, 63, 64, 65, 66, 67, 69, 70, 73, 76, 79, 81, 82, 84, 85, 86, 87, 88,
	89, 92, 95, 96, 98, 100, 103, 104, 107, 108, 116, 117, 121, 122, 125, 127, 131, 132, 134,
	137, 139, 140, 141, 142, 143, 144, 145, 147, 151, 153, 154, 158, 159, 160, 162, 164, 165,
	168, 170, 171, 174, 175, 176, 177, 181
};


//decoding functions here
void nxdn_descramble(uint8_t dibits[], int len)
{
	for (int i=0; i<len; i++)
	{
		if (scramble_t[i] >= len)
			break;
		dibits[scramble_t[i]] ^= 0x2;	// invert sign of scrambled dibits
	}
}

void nxdn_deperm_facch(dsd_opts * opts, dsd_state * state, uint8_t bits[144])
{
	uint8_t deperm[144]; //144
	uint8_t depunc[192]; //192
	uint8_t trellis_buf[96]; //96
	uint16_t crc = 1; //crc calculated by function
	uint16_t check = 0; //crc from payload for comparison
	int out;

	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));

	for (int i=0; i<144; i++) 
		deperm[PERM_16_9[i]] = bits[i]; 
	out = 0;
	for (int i=0; i<144; i+=3) {
		depunc[out++] = deperm[i+0];
		depunc[out++] = 0; 
		depunc[out++] = deperm[i+1];
		depunc[out++] = deperm[i+2];
	}

	//switch to the convolutional decoder
	uint8_t temp[200];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[20]; //13
	memset (temp, 0, sizeof(temp));
	memset (m_data, 0, sizeof(m_data));
	memset (trellis_buf, 0, sizeof(trellis_buf));

	for (int i = 0; i < 192; i++)
		temp[i] = depunc[i] << 1; 

	CNXDNConvolution_start();
  for (int i = 0; i < 96; i++) 
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 92);

	for(int i = 0; i < 12; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

	crc = crc12f (trellis_buf, 84); //80
	for (int i = 0; i < 12; i++)
	{
		check = check << 1;
		check = check | trellis_buf[84+i]; //80
	}

	//debug
	// if (crc == check)
		// fprintf (stderr, " Pass 1 ");

	//if the crc fails, attempt again with the other trellis decoder
	if (crc != check)
	{
		//debug
		// fprintf (stderr, " Pass 2 "); 
		crc = 1; check = 0;
		memset (trellis_buf, 0, sizeof(trellis_buf));
		memset (m_data, 0, sizeof(m_data));
		trellis_decode(trellis_buf, depunc, 92);
		//fill m_data bytes with trellis_buf
		for(int i = 0; i < 12; i++)
			m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
		crc = crc12f (trellis_buf, 84);
		for (int i = 0; i < 12; i++)
		{
			check = check << 1;
			check = check | trellis_buf[i+84];
		}
	}

	if (crc == check) NXDN_Elements_Content_decode(opts, state, 1, trellis_buf); 
	// else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, trellis_buf);

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " FACCH1 Payload ");
		for (int i = 0; i < 12; i++)
		{
			fprintf (stderr, "[%02X]", m_data[i]); 
		}
		if (crc != check && opts->payload == 1)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		}
	}

}

//sacch
void nxdn_deperm_sacch(dsd_opts * opts, dsd_state * state, uint8_t bits[60])
{
	//see about initializing these variables
	uint8_t deperm[60]; //60 
	uint8_t depunc[72]; //72
	uint8_t trellis_buf[32]; //32

	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));
	memset (trellis_buf, 0, sizeof(trellis_buf));

	int o = 0;
	uint8_t crc = 1; //value computed by crc6 on payload
	uint8_t check = 0; //value pulled from last 6 bits
	int sf = 0;
	int ran = 0;
	int part_of_frame = 0;

	for (int i=0; i<60; i++) 
		deperm[PERM_12_5[i]] = bits[i];
	for (int p=0; p<60; p+= 10) {
		depunc[o++] = deperm[p+0];
		depunc[o++] = deperm[p+1];
		depunc[o++] = deperm[p+2];
		depunc[o++] = deperm[p+3];
		depunc[o++] = deperm[p+4];
		depunc[o++] = 0;
		depunc[o++] = deperm[p+5];
		depunc[o++] = deperm[p+6];
		depunc[o++] = deperm[p+7];
		depunc[o++] = deperm[p+8];
		depunc[o++] = deperm[p+9];
		depunc[o++] = 0;
	}

	//switch to the convolutional decoder
	uint8_t temp[80];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[5]; //5

	memset (temp, 0, sizeof (temp));
	memset (m_data, 0, sizeof (m_data));
	memset (trellis_buf, 0, sizeof(trellis_buf));

	for (int i = 0; i < 72; i++)
		temp[i] = depunc[i] << 1; 

	CNXDNConvolution_start();
  for (int i = 0; i < 36; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

	//stored as 5 bytes, will need to convert to trellis_buf after running
  CNXDNConvolution_chainback(m_data, 32);

	for(int i = 0; i < 4; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

	crc = crc6(trellis_buf, 26); //32
	for (int i = 0; i < 6; i++)
	{
		check = check << 1;
		check = check | trellis_buf[i+26];
	}

	//debug
	// if (crc == check)
	// 	fprintf (stderr, " Pass 1 ");

	//if the crc fails, attempt again with the other trellis decoder
	if (crc != check)
	{
		//debug
		// fprintf (stderr, " Pass 2 "); 
		crc = 1; check = 0;
		memset (trellis_buf, 0, sizeof(trellis_buf));
		memset (m_data, 0, sizeof(m_data));
		trellis_decode(trellis_buf, depunc, 32);
		//fill m_data bytes with trellis_buf
		for(int i = 0; i < 4; i++)
			m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
		crc = crc6(trellis_buf, 26); //32
		for (int i = 0; i < 6; i++)
		{
			check = check << 1;
			check = check | trellis_buf[i+26];
		}
	}

	//FIRST! If part of a non_superframe, and CRC is good, send directly to NXDN_Elements_Content_decode
	if (state->nxdn_sacch_non_superframe == TRUE)
	{
		if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		else fprintf (stderr, "        ");

		uint8_t nsf_sacch[26];
		memset (nsf_sacch, 0, sizeof(nsf_sacch));
		for (int i = 0; i < 26; i++)
		{
			nsf_sacch[i] = trellis_buf[i+8];
		}

		if (crc == check)
		{
			ran = (trellis_buf[2] << 5) | (trellis_buf[3] << 4) | (trellis_buf[4] << 3) | (trellis_buf[5] << 2) | (trellis_buf[6] << 1) | trellis_buf[7];
			state->nxdn_last_ran = ran;
		} 

		fprintf (stderr, "PF 1/1");
		if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
		else if (state->M == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed

		if (crc == check) NXDN_Elements_Content_decode(opts, state, 1, nsf_sacch); 
		// else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, nsf_sacch);

		//I'm placing this here, my observation is that SACCH NSF 
		//is almost always, if not always, just IDLE
		else fprintf (stderr, " IDLE");

		if (opts->payload == 1)
		{ 
			fprintf (stderr, "\n SACCH NSF ");
			for (int i = 0; i < 4; i++)
			{
				fprintf (stderr, "[%02X]", m_data[i]);
			}

			if (crc != check)
			{

				fprintf (stderr, "%s", KRED);
				fprintf (stderr, " (CRC ERR)");
				fprintf (stderr, "%s", KNRM);
			}

		}
		
		//reset the sacch field -- Github Issue #118
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

	}
	
	//If part of superframe, collect the fragments and send to NXDN_SACCH_Full_decode instead
	else if (state->nxdn_sacch_non_superframe == FALSE)
	{
		//sf and ran together are denoted as SR in the manual (more confusing acronyms)
		//sf (structure field) and RAN will always exist in first 8 bits of each SACCH, then the next 18 bits are the fragment of the superframe
		sf = (trellis_buf[0] << 1) | trellis_buf[1];

		if      (sf == 3) part_of_frame = 0;
		else if (sf == 2) part_of_frame = 1;
		else if (sf == 1) part_of_frame = 2;
		else if (sf == 0) part_of_frame = 3;
		else part_of_frame = 0; 

		fprintf (stderr, "%s", KCYN);
		if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		else fprintf (stderr, "        ");
		fprintf (stderr, "%s", KNRM);

		fprintf (stderr, "PF %d/4", part_of_frame+1);
		if (part_of_frame == 0)
		{
			if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
			else if (state->M == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
		}

		// if (crc != check)
		// {
		// 	fprintf (stderr, "%s", KRED);
		// 	fprintf (stderr, " (CRC ERR)");
		// 	fprintf (stderr, "%s", KNRM);
		// }
		
		//reset scrambler seed to key value on new superframe
		// if (part_of_frame == 0 && state->nxdn_cipher_type == 0x1) state->payload_miN = 0;

		//reset scrambler seed to key value on new superframe
		if (part_of_frame == 0 && state->nxdn_cipher_type == 0x1)
		{
			if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
			else if (state->M == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
		}
		//this seems to work much better now
		else if (part_of_frame != 0 && state->nxdn_cipher_type == 0x1)
		{
			if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
			else if (state->M == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed

			//advance seed by required number of turns depending on the current pf value
			int start = 0; int end = part_of_frame;
			char ambe_temp[49] = {0};
			char ambe_d[49] = {0};
			for (start = 0; start < end; start++)
			{
				//run 4 times
				LFSRN(ambe_temp, ambe_d, state);
				LFSRN(ambe_temp, ambe_d, state);
				LFSRN(ambe_temp, ambe_d, state);
				LFSRN(ambe_temp, ambe_d, state);
			}
		}

		if (crc == check)
		{
			ran = (trellis_buf[2] << 5) | (trellis_buf[3] << 4) | (trellis_buf[4] << 3) | (trellis_buf[5] << 2) | (trellis_buf[6] << 1) | trellis_buf[7];
			state->nxdn_ran = state->nxdn_last_ran = ran;
			state->nxdn_sf = sf;
			state->nxdn_part_of_frame = part_of_frame;
			state->nxdn_sacch_frame_segcrc[part_of_frame] = 0; //zero indicates good check
		}
		else state->nxdn_sacch_frame_segcrc[part_of_frame] = 1; //1 indicates bad check

		int sacch_segment = 0;

		for (int i = 0; i < 18; i++)
		{
			sacch_segment = sacch_segment << 1;
			sacch_segment = sacch_segment + trellis_buf[i+8]; 
			state->nxdn_sacch_frame_segment[part_of_frame][i] = trellis_buf[i+8];
		}

		//Hand off to LEH NXDN_SACCH_Full_decode
		if (part_of_frame == 3)
		{
			NXDN_SACCH_Full_decode (opts, state);
		} 

		if (opts->payload == 1)
		{ 
			fprintf (stderr, "\n"); 
			fprintf (stderr, " SACCH SF Segment #%d ", part_of_frame+1);
			for (int i = 0; i < 4; i++)
			{
				fprintf (stderr, "[%02X]", m_data[i]);
			}
			if (crc != check) fprintf (stderr, " CRC ERR - %02X %02X", crc, check);
		}

	}	
	
}

void nxdn_deperm_facch2_udch(dsd_opts * opts, dsd_state * state, uint8_t bits[348], uint8_t type)
{
	uint8_t deperm[348]; //348
	uint8_t depunc[406]; //406
	uint8_t trellis_buf[208]; //199
	int id = 0;
	uint16_t crc = 0;
	uint16_t check = 0;

	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));

	for (int i=0; i<348; i++) {
		deperm[PERM_12_29[i]] = bits[i];
	}
	for (int i=0; i<29; i++) {
		depunc[id++] = deperm[i*12];
		depunc[id++] = deperm[i*12+1];
		depunc[id++] = deperm[i*12+2];
		depunc[id++] = 0;
		depunc[id++] = deperm[i*12+3];
		depunc[id++] = deperm[i*12+4];
		depunc[id++] = deperm[i*12+5];
		depunc[id++] = deperm[i*12+6];
		depunc[id++] = deperm[i*12+7];
		depunc[id++] = deperm[i*12+8];
		depunc[id++] = deperm[i*12+9];
		depunc[id++] = 0;
		depunc[id++] = deperm[i*12+10];
		depunc[id++] = deperm[i*12+11];
	}
	
	//switch to the convolutional decoder
	uint8_t temp[406];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[26]; //26
	memset (trellis_buf, 0, sizeof(trellis_buf));
	memset (temp, 0, sizeof (temp));
	memset (m_data, 0, sizeof (m_data));

	for (int i = 0; i < 406; i++)
		temp[i] = depunc[i] << 1; 

	CNXDNConvolution_start();
  for (int i = 0; i < 203; i++) 
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

	//numerals seem okay now
  CNXDNConvolution_chainback(m_data, 199); 

	for(int i = 0; i < 26; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

	crc = crc15(trellis_buf, 199);
	for (int i = 0; i < 15; i++)
	{
		check = check << 1;
		check = check | trellis_buf[i+184];
	}

	//debug
	// if (crc == check)
		// fprintf (stderr, " Pass 1 ");

	//if the crc fails, attempt again with the other trellis decoder
	if (crc != check)
	{
		//debug
		// fprintf (stderr, " Pass 2 "); 
		crc = 1; check = 0;
		memset (trellis_buf, 0, sizeof(trellis_buf));
		memset (m_data, 0, sizeof(m_data));
		trellis_decode(trellis_buf, depunc, 199);
		//fill m_data bytes with trellis_buf
		for(int i = 0; i < 26; i++)
			m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
		crc = crc15(trellis_buf, 199);
		for (int i = 0; i < 15; i++)
		{
			check = check << 1;
			check = check | trellis_buf[i+184];
		}
	}

	fprintf (stderr, "%s", KYEL);
	if (type == 0) fprintf (stderr, " UDCH");
	if (type == 1) fprintf (stderr, " FACCH2");
	fprintf (stderr, "%s", KNRM);

	uint8_t f2u_message_buffer[400]; //199
	memset (f2u_message_buffer, 0, sizeof(f2u_message_buffer));

	//just going to leave this all here in case its needed, like in cac,
	//don't have any samples with f2 or udch data in it
	for (int i = 0; i < 199-8-15; i++) 
	{
		f2u_message_buffer[i] = trellis_buf[i+8]; 
	}
	
	if (crc == check)
	{ 
		if (type == 1) NXDN_Elements_Content_decode(opts, state, 1, f2u_message_buffer);
		if (type == 0) {} //need handling for user data (text messages and AVL)
	} 
	// else if (opts->aggressive_framesync == 0)
	// {
	// 	if (type == 1) NXDN_Elements_Content_decode(opts, state, 0, f2u_message_buffer);
	// 	if (type == 0) {} //need handling for user data (text messages and AVL)
	// } 

	if (type == 0 && crc == check)
	{
		fprintf (stderr, "\n UDCH Data: "  );
		for (int i = 0; i < 24; i++) //all but last crc portion
			fprintf (stderr, "%02X", m_data[i]);

		fprintf (stderr, "\n UDCH Data: ASCII - "  );
		for (int i = 0; i < 24; i++) //remove crc portion
		{
			if (m_data[i] <= 0x7E && m_data[i] >=0x20)
			{
				fprintf (stderr, "%c", m_data[i]);
			}
			else fprintf (stderr, " ");
		}

		if (crc != check)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		}

	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		if (type == 0) fprintf (stderr, " UDCH");
		if (type == 1) fprintf (stderr, " FACCH2");
		fprintf (stderr, " Payload\n  ");
		for (int i = 0; i < 26; i++)
		{
			if (i == 13) fprintf (stderr, "\n  ");
			fprintf (stderr, "[%02X]", m_data[i]); 
		}
		
		if (crc != check)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		}

		// if (crc != check) fprintf (stderr, " CRC ERR ");

		// if (type == 0)
		// {
		// 	fprintf (stderr, "\n UDCH Data: ASCII - "  );
		// 	for (int i = 0; i < 24; i++) //remove crc portion
		// 	{
		// 		if (m_data[i] <= 0x7E && m_data[i] >=0x20)
		// 		{
		// 			fprintf (stderr, "%c", m_data[i]);
		// 		}
		// 		else fprintf (stderr, " ");
		// 	}
		// }
	}  

}
static int cac_fail = 0;
void nxdn_deperm_cac(dsd_opts * opts, dsd_state * state, uint8_t bits[300])
{
	uint8_t deperm[300]; //300
	uint8_t depunc[350]; //350
	uint8_t trellis_buf[176]; //176
	int id = 0;
	int ran = 0;
	uint16_t crc = 0;

	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));

	for (int i=0; i<300; i++) {
		deperm[PERM_12_25[i]] = bits[i];
	}
	for (int i=0; i<25; i++) {
		depunc[id++] = deperm[i*12];
		depunc[id++] = deperm[i*12+1];
		depunc[id++] = deperm[i*12+2];
		depunc[id++] = 0;
		depunc[id++] = deperm[i*12+3];
		depunc[id++] = deperm[i*12+4];
		depunc[id++] = deperm[i*12+5];
		depunc[id++] = deperm[i*12+6];
		depunc[id++] = deperm[i*12+7];
		depunc[id++] = deperm[i*12+8];
		depunc[id++] = deperm[i*12+9];
		depunc[id++] = 0;
		depunc[id++] = deperm[i*12+10];
		depunc[id++] = deperm[i*12+11];
	}

	//switch to the convolutional decoder
	uint8_t temp[179];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[22]; //26
	memset (trellis_buf, 0, sizeof(trellis_buf));
	memset (temp, 0, sizeof (temp));
	memset (m_data, 0, sizeof (m_data));

	for (int i = 0; i < 350; i++)
		temp[i] = depunc[i] << 1; 

	CNXDNConvolution_start();
  for (int i = 0; i < 175; i++) //179
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 171); //175

	for(int i = 0; i < 22; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

	crc = crc16cac(trellis_buf, 171); 

	//debug
	// if (crc == 0)
	// 	fprintf (stderr, " Pass 1 ");

	if (crc != 0)
	{
		//debug
		// fprintf (stderr, " Pass 2 "); 
		memset (trellis_buf, 0, sizeof(trellis_buf));
		memset (m_data, 0, sizeof(m_data));
		trellis_decode(trellis_buf, depunc, 171);
		//fill m_data bytes with trellis_buf
		for(int i = 0; i < 22; i++)
			m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
		crc = crc16cac(trellis_buf, 171);
	}

	uint8_t cac_message_buffer[147]; //171
	memset (cac_message_buffer, 0, sizeof(cac_message_buffer));

	//shift the cac_message into the appropriate byte arrangement for element_decoder -- skip SR field and ending crc
	for (int i = 0; i < 147; i++)
	{
		cac_message_buffer[i] = trellis_buf[i+8];
	}

	if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
	else fprintf (stderr, "        ");

	if (crc == 0)
	{
		ran = (trellis_buf[2] << 5) | (trellis_buf[3] << 4) | (trellis_buf[4] << 3) | (trellis_buf[5] << 2) | (trellis_buf[6] << 1) | trellis_buf[7];
		state->nxdn_last_ran = ran;
	} 

	fprintf (stderr, "%s", KYEL);
	fprintf (stderr, " CAC");
	fprintf (stderr, "%s", KNRM);

	if (crc != 0)
	{
		fprintf (stderr, "%s", KRED);
		fprintf (stderr, " (CRC ERR)");
		fprintf (stderr, "%s", KNRM);
	}

	//check for accumulative cac failures and reset if multiple errors pile up 
	//(LZ issue and also issue very occasssionally noticed on remote)
	if (crc != 0) cac_fail++;
	else cac_fail = 0;

	//reset some parameters if CAC continues to fail
	if (cac_fail > 10)
	{
		//simple reset
		state->synctype = 0;
		state->lastsynctype = -1;
		state->carrier = 0;
		state->last_cc_sync_time = time(NULL)+2; //probably not necesary
		cac_fail = 0; //reset

		//more advanced modulator reset
		state->center = 0;
		state->jitter = -1;
		state->synctype = -1;
		state->min = -15000;
		state->max = 15000;
		state->lmid = 0;
		state->umid = 0;
		state->minref = -12000;
		state->maxref = 12000;
		state->lastsample = 0;
		for (int i = 0; i < 128; i++) state->sbuf[i] = 0;
		state->sidx = 0;
		for (int i = 0; i < 1024; i++) state->maxbuf[i] = 15000;
		for (int i = 0; i < 1024; i++) state->minbuf[i] = -15000;
		state->midx = 0;
		state->symbolcnt = 0;

		//reset the dibit buffer
		state->dibit_buf_p = state->dibit_buf + 200;
		memset (state->dibit_buf, 0, sizeof (int) * 200);
		state->offset = 0;

		//debug notification
		// fprintf (stderr, " RESET CARRIER; ");

	}

	if (crc == 0) NXDN_Elements_Content_decode(opts, state, 1, cac_message_buffer);
	// else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, cac_message_buffer);  

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " CAC Payload\n  ");
		for (int i = 0; i < 22; i++)
		{
			fprintf (stderr, "[%02X]", m_data[i]); 
			if (i == 10) fprintf (stderr, "\n  ");
		}
		// if (crc != 0) fprintf (stderr, " CRC ERR ");

	} 

}

//Type-D "IDAS" 
void nxdn_deperm_scch(dsd_opts * opts, dsd_state * state, uint8_t bits[60], uint8_t direction)
{
	fprintf (stderr, "%s", KYEL);
	fprintf (stderr, " SCCH");

	//see about initializing these variables
	uint8_t deperm[60]; //60 
	uint8_t depunc[72]; //72
	uint8_t trellis_buf[32]; //32

	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));
	memset (trellis_buf, 0, sizeof(trellis_buf));

	int o = 0;
	uint8_t crc = 0; //value computed by crc7 on payload
	uint8_t check = 0; //value pulled from last 7 bits
	int sf = 0;
	int part_of_frame = 0;

	for (int i=0; i<60; i++) 
		deperm[PERM_12_5[i]] = bits[i];
	for (int p=0; p<60; p+= 10) {
		depunc[o++] = deperm[p+0];
		depunc[o++] = deperm[p+1];
		depunc[o++] = deperm[p+2];
		depunc[o++] = deperm[p+3];
		depunc[o++] = deperm[p+4];
		depunc[o++] = 0;
		depunc[o++] = deperm[p+5];
		depunc[o++] = deperm[p+6];
		depunc[o++] = deperm[p+7];
		depunc[o++] = deperm[p+8];
		depunc[o++] = deperm[p+9];
		depunc[o++] = 0;
	}

	//switch to the convolutional decoder
	uint8_t temp[72];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[5]; //5

	memset (temp, 0, sizeof (temp));
	memset (m_data, 0, sizeof (m_data));
	memset (trellis_buf, 0, sizeof(trellis_buf));

	for (int i = 0; i < 72; i++)
		temp[i] = depunc[i] << 1; 

	CNXDNConvolution_start();
  for (int i = 0; i < 36; i++) 
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 32);

	for(int i = 0; i < 4; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

	crc = crc7_scch(trellis_buf, 25);
	for (int i = 0; i < 7; i++)
	{
		check = check << 1;
		check = check | trellis_buf[i+25];
	}

	//debug
	// if (crc == check)
		// fprintf (stderr, " Pass 1 ");

	//if the crc fails, attempt again with the other trellis decoder
	if (crc != check)
	{
		//debug
		// fprintf (stderr, " Pass 2 "); 
		crc = 1; check = 0;
		memset (trellis_buf, 0, sizeof(trellis_buf));
		memset (m_data, 0, sizeof(m_data));
		trellis_decode(trellis_buf, depunc, 32); //32?
		//fill m_data bytes with trellis_buf
		for(int i = 0; i < 4; i++)
			m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
		crc = crc7_scch(trellis_buf, 25);
		for (int i = 0; i < 6; i++)
		{
			check = check << 1;
			check = check | trellis_buf[i+25];
		}
	}

	//check the sf early for scrambler reset, if required
	sf = (trellis_buf[0] << 1) | trellis_buf[1];
	if      (sf == 3) part_of_frame = 0;
	else if (sf == 2) part_of_frame = 1;
	else if (sf == 1) part_of_frame = 2;
	else if (sf == 0) part_of_frame = 3;
	else part_of_frame = 0; 

	//reset scrambler seed to key value on new superframe
	// if (part_of_frame == 0 && state->nxdn_cipher_type == 0x1) state->payload_miN = 0;

	//reset scrambler seed to key value on new superframe
	if (part_of_frame == 0 && state->nxdn_cipher_type == 0x1)
	{
		if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
		else if (state->M == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
	}

	/*
	4.3.2. Mapping to Functional Channel
		When Subscriber Unit performs voice communication in RTCH2, SCCH is allocated as
		described below.

		SCCH has super Frame Structure with 4 frame unit during communication, and has non
		super Frame Structure with single frame unit at the start time and end time of sending.
		Allocation of the other channel is not specified particularly.
	*/

	//English Translation: Superframe when voice, Non Superframe when not voice

	//What I've found is that by not using the superframe structure, and decoding each 'unit'
	//individually instead, we can get more expedient decoding on elements without having to 
	//sacrifice an entire superframe for one bad CRC, also each element cleanly divides into a 
	//single 'unit' except for enc parms IV, which can be stored seperately if needed

	//NOTE: scch has its own message format, and thus, doesn't go to content element decoding
	//like everything else does

	if (crc == check) NXDN_decode_scch (opts, state, trellis_buf, direction);
	// else if (opts->aggressive_framesync == 0) NXDN_decode_scch (opts, state, trellis_buf, direction);

	fprintf (stderr, "%s", KNRM);

	if (opts->payload == 1)
	{ 
		fprintf (stderr, "\n SCCH Payload ");
		for (int i = 0; i < 4; i++)
		{
			fprintf (stderr, "[%02X]", m_data[i]);
		}

		if (crc != check)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		} 
	}

	fprintf (stderr, "%s", KNRM);
	
}

void nxdn_deperm_facch3_udch2(dsd_opts * opts, dsd_state * state, uint8_t bits[288], uint8_t type)
{
	uint8_t deperm[144]; //144
	uint8_t depunc[192]; //192
	uint8_t trellis_buf[96]; //96
	uint8_t f3_udch2[288]; //completed bitstream without crc and tailing bits attached
	uint8_t f3_udch2_bytes[36]; //completed bytes - with crc and tail
	uint16_t crc[2]; //crc calculated by function
	uint16_t check[2]; //crc from payload for comparison
	int out;

	//switch to the convolutional decoder
	uint8_t temp[192];
	uint8_t s0;
  uint8_t s1;
	uint8_t m_data[12]; //13
	memset (temp, 0, sizeof(temp));
	memset (m_data, 0, sizeof(m_data));
	memset (trellis_buf, 0, sizeof(trellis_buf));
	memset (deperm, 0, sizeof(deperm));
	memset (depunc, 0, sizeof(depunc));
	memset (crc, 0, sizeof(crc));
	memset (check, 0, sizeof(check));
	memset (f3_udch2, 0, sizeof(f3_udch2));
	memset (f3_udch2_bytes, 0, sizeof(f3_udch2_bytes));

	for (int j=0; j<2; j++)
	{
		for (int i=0; i<144; i++) 
			deperm[PERM_16_9[i]] = bits[i+(j*144)]; 
		out = 0;
		for (int i=0; i<144; i+=3) {
			depunc[out++] = deperm[i+0];
			depunc[out++] = 0; 
			depunc[out++] = deperm[i+1];
			depunc[out++] = deperm[i+2];
		}

		for (int i = 0; i < 192; i++)
			temp[i] = depunc[i] << 1; 

		CNXDNConvolution_start();
		for (int i = 0; i < 96; i++) 
		{
			s0 = temp[(2*i)];
			s1 = temp[(2*i)+1];

			CNXDNConvolution_decode(s0, s1);
		}

		CNXDNConvolution_chainback(m_data, 92);

		for(int i = 0; i < 12; i++)
		{
			trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
			trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
			trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
			trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
			trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
			trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
			trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
			trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
		}

		crc[j] = crc12f (trellis_buf, 84); //84
		for (int i = 0; i < 12; i++)
		{
			check[j] = check[j] << 1;
			check[j] = check[j] | trellis_buf[84+i]; //84
		
		}

		//debug
		// if (crc[j] == check[j])
		// 	fprintf (stderr, "P:%d Pass 1 ", j);

		//if the crc fails, attempt again with the other trellis decoder
		if (crc[j] != check[j])
		{
			//debug
			// fprintf (stderr, "P:%d Pass 2 ", j); 
			crc[j] = 1; check[j] = 0;
			memset (trellis_buf, 0, sizeof(trellis_buf));
			memset (m_data, 0, sizeof(m_data));
			trellis_decode(trellis_buf, depunc, 92);
			//fill m_data bytes with trellis_buf
			for(int i = 0; i < 12; i++)
				m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);
			crc[j] = crc12f (trellis_buf, 84);
			for (int i = 0; i < 12; i++)
			{
				check[j] = check[j] << 1;
				check[j] = check[j] | trellis_buf[i+84];
			}
		}

		//transfer to storage sans crc and tail bits
		for (int i = 0; i < 80; i++) f3_udch2[i+(j*80)] = trellis_buf[i];
		for (int i = 0; i < 12; i++) f3_udch2_bytes[i+(j*12)] = m_data[i];
	}

	fprintf (stderr, "%s", KYEL);
	if (type == 0) fprintf (stderr, " UDCH2");
	if (type == 1) fprintf (stderr, " FACCH3");
	fprintf (stderr, "%s", KNRM);


	if (crc[0] == check[0] && crc[1] == check[1])
	{
		if (type == 1) NXDN_Elements_Content_decode(opts, state, 1, trellis_buf);
		if (type == 0) {} //need handling for user data (text messages and AVL)
	}  
	// else if (opts->aggressive_framesync == 0) 
	// {
	// 	if (type == 1) NXDN_Elements_Content_decode(opts, state, 0, trellis_buf);
	// 	if (type == 0) {} //need handling for user data (text messages and AVL)
	// }

	if (type == 0)
	{
		fprintf (stderr, "\n UDCH2 Data: "  );
		for (int i = 0; i < 22; i++) //all but last crc portion
		{
			if (i == 10)
			{
				fprintf (stderr, " "); //space seperator?
				i = 12;  //skip first crc portion
			}
			fprintf (stderr, "%02X", f3_udch2_bytes[i]);
		}

		fprintf (stderr, "\n UDCH2 Data: ASCII - "  );
		for (int i = 0; i < 22; i++) //all but last crc portion
		{
			if (i == 10) i = 12;  //skip first crc portion
			if (f3_udch2_bytes[i] <= 0x7E && f3_udch2_bytes[i] >=0x20)
			{
				fprintf (stderr, "%c", f3_udch2_bytes[i]);
			}
			else fprintf (stderr, " ");
		}

		if (crc[0] != check[0] && crc[1] != check[1])
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		}
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		if (type == 0) fprintf (stderr, " UDCH2");
		if (type == 1) fprintf (stderr, " FACCH3");
		fprintf (stderr, " Payload \n  ");
		for (int i = 0; i < 24; i++)
		{
			if (i == 12) fprintf (stderr, "\n  ");
			fprintf (stderr, "[%02X]", f3_udch2_bytes[i]); 
		}

		if ( (crc[0] != check[0] || crc[1] != check[1]) && opts->payload == 1)
		{

			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);

			// fprintf (stderr, " - %03X %03X", check[0], crc[0]);
			// fprintf (stderr, " - %03X %03X", check[1], crc[1]);
		}

		// if (type == 0)
		// {
		// 	fprintf (stderr, "\n UDCH2 Data: ASCII - "  );
		// 	for (int i = 0; i < 22; i++) //all but last crc portion
		// 	{
		// 		if (i == 12) i = 14;  //skip first crc portion
		// 		if (f3_udch2_bytes[i] <= 0x7E && f3_udch2_bytes[i] >=0x20)
		// 		{
		// 			fprintf (stderr, "%c", f3_udch2_bytes[i]);
		// 		}
		// 		else fprintf (stderr, " ");
		// 	}
		// }
		
	}

}

void nxdn_message_type (dsd_opts * opts, dsd_state * state, uint8_t MessageType)
{
	UNUSED(opts);

	//NOTE: Most Req/Resp (request and respone) share same message type but differ depending on channel type
	//RTCH Outbound will take precedent when differences may occur (except CALL_ASSGN)
	fprintf (stderr, "%s", KYEL);
	if      (MessageType == 0x10) fprintf(stderr, " IDLE");
	else if (MessageType == 0x00) fprintf(stderr, " CALL_RESP");
	else if (MessageType == 0x01) fprintf(stderr, " VCALL");
	else if (MessageType == 0x02) fprintf(stderr, " VCALL_REC_REQ");
	else if (MessageType == 0x03) fprintf(stderr, " VCALL_IV");
	else if (MessageType == 0x04) fprintf(stderr, " VCALL_ASSGN");
	else if (MessageType == 0x05) fprintf(stderr, " VCALL_ASSGN_DUP");
	else if (MessageType == 0x06) fprintf(stderr, " CALL_CONN_RESP");
	else if (MessageType == 0x07) fprintf(stderr, " TX_REL_EX");
	else if (MessageType == 0x08) fprintf(stderr, " TX_REL");
	else if (MessageType == 0x09) fprintf(stderr, " DCALL_HEADER");
	else if (MessageType == 0x0A) fprintf(stderr, " DCALL_REC_REQ");
	else if (MessageType == 0x0B) fprintf(stderr, " DCALL_UDATA");
	else if (MessageType == 0x0C) fprintf(stderr, " DCALL_ACK");
	else if (MessageType == 0x0D) fprintf(stderr, " DCALL_ASSGN_DUP");
	else if (MessageType == 0x0E) fprintf(stderr, " DCALL_ASSGN");
	else if (MessageType == 0x11) fprintf(stderr, " DISC");
	else if (MessageType == 0x17) fprintf(stderr, " DST_ID_INFO");
	else if (MessageType == 0x18) fprintf(stderr, " SITE_INFO");
	else if (MessageType == 0x19) fprintf(stderr, " SRV_INFO");
	else if (MessageType == 0x1A) fprintf(stderr, " CCH_INFO");
	else if (MessageType == 0x1B) fprintf(stderr, " ADJ_SITE_INFO");
	else if (MessageType == 0x1C) fprintf(stderr, " FAIL_STAT_INFO");
	else if (MessageType == 0x20) fprintf(stderr, " REG_RESP");
	else if (MessageType == 0x22) fprintf(stderr, " REG_C_RESP");
	else if (MessageType == 0x24) fprintf(stderr, " GRP_REG_RESP");
	else if (MessageType == 0x32) fprintf(stderr, " STAT_REQ");
	else if (MessageType == 0x33) fprintf(stderr, " STAT_RESP");
	else if (MessageType == 0x38) fprintf(stderr, " SDCALL_REQ_HEADER");
	else if (MessageType == 0x39) fprintf(stderr, " SDCALL_REQ_USERDATA");
	else if (MessageType == 0x3B) fprintf(stderr, " SDCALL_RESP");
	else if (MessageType == 0x3F) fprintf(stderr, " ALIAS"); 
	else fprintf(stderr, " Unknown M-%02X", MessageType);
	fprintf (stderr, "%s", KNRM);

	//Zero out stale values on DISC or TX_REL only (IDLE messaages occur often on NXDN96 VCH, and randomly on Type-C FACCH1 steals for some reason)
	if (MessageType == 0x08 || MessageType == 0x11)
	{
		memset (state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));
		state->nxdn_last_rid = 0;
		state->nxdn_last_tg = 0;
		state->nxdn_cipher_type = 0; //force will reactivate it if needed during voice tx
		if (state->keyloader == 1) state->R = 0;
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		sprintf (state->nxdn_call_type, "%s", "");
	}

	if (MessageType == 0x07 ||MessageType == 0x08 || MessageType == 0x11)
	{
		//reset gain
		if (opts->floating_point == 1)
			state->aout_gain = opts->audio_gain;
	}
	 
}

//voice descrambler
void LFSRN(char * BufferIn, char * BufferOut, dsd_state * state)
{
  int i;
  int lfsr;
  int pN[49] = {0};
  int bit = 0;

  lfsr = state->payload_miN & 0x7FFF;

  for (i = 0; i < 49; i++)
  {
    pN[i] = lfsr & 0x1;
    bit = ( (lfsr >> 1) ^ (lfsr >> 0) ) & 1;
    lfsr =  ( (lfsr >> 1 ) | (bit << 14) );
    BufferOut[i] = BufferIn[i] ^ pN[i];
  }

  state->payload_miN = lfsr & 0x7FFF;
}

int load_i(const uint8_t val[], int len) {
	int acc = 0;
	for (int i=0; i<len; i++){
		acc = (acc << 1) + (val[i] & 1);
	}
	return acc;
}

uint8_t crc6(const uint8_t buf[], int len)
{
	uint8_t s[6];
	uint8_t a;
	for (int i=0;i<6;i++)
		s[i] = 1;
	for (int i=0;i<len;i++) {
		a = buf[i] ^ s[0];
		s[0] = a ^ s[1];
		s[1] = s[2];
		s[2] = s[3];
		s[3] = a ^ s[4];
		s[4] = a ^ s[5];
		s[5] = a;
	}
	return load_i(s, 6);
}

uint16_t crc12f(const uint8_t buf[], int len)
{
	uint8_t s[12];
	uint8_t a;
	for (int i=0;i<12;i++)
		s[i] = 1;
	for (int i=0;i<len;i++) {
		a = buf[i] ^ s[0];
		s[0] = a ^ s[1];
		s[1] = s[2];
		s[2] = s[3];
		s[3] = s[4];
		s[4] = s[5];
		s[5] = s[6];
		s[6] = s[7];
		s[7] = s[8];
		s[8] = a ^ s[9];
		s[9] = a ^ s[10];
		s[10] = a ^ s[11];
		s[11] = a;
	}
	return load_i(s, 12);
}

uint16_t crc15(const uint8_t buf[], int len)
{
	uint8_t s[15];
	uint8_t a;
	for (int i=0;i<15;i++)
		s[i] = 1;
	for (int i=0;i<len;i++) {
		a = buf[i] ^ s[0];
		s[0] = a ^ s[1];
		s[1] = s[2];
		s[2] = s[3];
		s[3] = a ^ s[4];
		s[4] = a ^ s[5];
		s[5] = s[6];
		s[6] = s[7];
		s[7] = a ^ s[8];
		s[8] = a ^ s[9];
		s[9] = s[10];
		s[10] = s[11];
		s[11] = s[12];
		s[12] = a ^ s[13];
		s[13] = s[14];
		s[14] = a;
	}
	return load_i(s, 15);
}

uint16_t crc16cac(const uint8_t buf[], int len)
{
	uint32_t crc = 0xc3ee; //not sure why this though
	uint32_t poly = (1<<12) + (1<<5) + 1; //poly is fine
	for (int i=0;i<len;i++) 
	{
		crc = ((crc << 1) | buf[i]) & 0x1ffff;
		if(crc & 0x10000) crc = (crc & 0xffff) ^ poly;
	}
	crc = crc ^ 0xffff;
	return crc & 0xffff;
}

uint8_t crc7_scch(uint8_t bits[], int len)
{
	uint8_t s[7];
	uint8_t a;
	for (int i=0;i<7;i++)
		s[i] = 1;
	for (int i=0;i<len;i++) {
		a = bits[i] ^ s[0];
		s[0] = s[1];
		s[1] = s[2]; 
		s[2] = s[3]; 
		s[3] = a ^ s[4];
		s[4] = s[5];
		s[5] = s[6]; 
		s[6] = a;
	}
	return load_i(s, 7);
}