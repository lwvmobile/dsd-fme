//NXDN descramble/deperm/depuncture and utility functions
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

static const uint8_t scramble_t[] = { //values are the position values we need to invert in the descramble
	2, 5, 6, 7, 10, 12, 14, 16, 17, 22, 23, 25, 26, 27, 28, 30, 33, 34, 36, 37, 38, 41, 45, 47,
	52, 54, 56, 57, 59, 62, 63, 64, 65, 66, 67, 69, 70, 73, 76, 79, 81, 82, 84, 85, 86, 87, 88,
	89, 92, 95, 96, 98, 100, 103, 104, 107, 108, 116, 117, 121, 122, 125, 127, 131, 132, 134,
	137, 139, 140, 141, 142, 143, 144, 145, 147, 151, 153, 154, 158, 159, 160, 162, 164, 165,
	168, 170, 171, 174, 175, 176, 177, 181
};

static const int PARITY[] = {
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 
  1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 
  1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

//decoding functions here
void nxdn_descramble(uint8_t dibits[], int len)
{
	for (int i=0; i<sizeof(scramble_t); i++) {
		if (scramble_t[i] >= len)
			break;
		dibits[scramble_t[i]] ^= 0x2;	// invert sign of scrambled dibits
	}
}

void nxdn_deperm_facch(dsd_opts * opts, dsd_state * state, uint8_t bits[144])
{
	uint8_t deperm[144];
	uint8_t depunc[192];
	uint8_t trellis_buf[92];
	uint16_t crc = 0; //crc calculated by function
	uint16_t check = 0; //crc from payload for comparison
	int out;
	char buf[128];
	uint8_t answer[12];

	for (int i=0; i<144; i++) 
		deperm[PERM_16_9[i]] = bits[i]; 
	out = 0;
	for (int i=0; i<144; i+=3) {
		depunc[out++] = deperm[i+0];
		depunc[out++] = 0; //0
		depunc[out++] = deperm[i+1];
		depunc[out++] = deperm[i+2];
	}

	trellis_decode(trellis_buf, depunc, 92);

	//load tail 16 bits into check variable to compare vs computed crc value
	for (int i = 0; i < 12; i++)
	{
		check = check << 1;
		check = check + trellis_buf[80+i];
	}

	crc = crc12f (trellis_buf, 80);
	cfill(answer, trellis_buf, 12); //<-92 bits in 12 bytes, leaves 4 bits on tail to scrub off
	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " FACCH Payload ");
		answer[11] = answer[11] & 0xF0; //scrub the garbage 4 bits from cfill on the end for ppayload print
		for (int i = 0; i < 12; i++)
		{
			fprintf (stderr, "[%02X]", answer[i]); 
		}

		fprintf (stderr, " - %03X %03X", crc, check);

	}

	if (crc == check) NXDN_Elements_Content_decode(opts, state, 1, trellis_buf); 
	//only run data with bad CRC when payload is enabled and hide the ugly
	else if (opts->payload == 1) NXDN_Elements_Content_decode(opts, state, 0, trellis_buf); 
}

void nxdn_deperm_sacch(dsd_opts * opts, dsd_state * state, uint8_t bits[60])
{
	uint8_t deperm[60];
	uint8_t depunc[72];
	uint8_t trellis_buf[32];
	uint8_t answer[26];
	int o=0;
	uint8_t crc = 0; //value computed by crc6 on payload
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
		depunc[o++] = 0; //0
		depunc[o++] = deperm[p+5];
		depunc[o++] = deperm[p+6];
		depunc[o++] = deperm[p+7];
		depunc[o++] = deperm[p+8];
		depunc[o++] = deperm[p+9];
		depunc[o++] = 0; //0
	}

	trellis_decode(trellis_buf, depunc, 32);

	crc = crc6(trellis_buf, 32);
	for (int i = 0; i < 6; i++)
	{
		check = check << 1;
		check = check + trellis_buf[i+26];
	}

	//sf and ran together are denoted as SR in the manual (more confusing acronyms)
	//sf (structure field) and RAN will always exist in first 8 bits of each SACCH, then the next 18 bits are the fragment of the superframe
	sf = (trellis_buf[0] << 1) | trellis_buf[1];
	ran = (trellis_buf[2] << 5) | (trellis_buf[3] << 4) | (trellis_buf[4] << 3) | (trellis_buf[5] << 2) | (trellis_buf[6] << 1) | trellis_buf[7];
	if      (sf == 3) part_of_frame = 0;
	else if (sf == 2) part_of_frame = 1;
	else if (sf == 1) part_of_frame = 2;
	else if (sf == 0) part_of_frame = 3;
	else part_of_frame = 0; 

	//gives unusual results when non_superframe SF, not sure why, or if its accurate yet
	if (state->nxdn_sacch_non_superframe == FALSE) 
	{
		fprintf (stderr, "%s", KCYN);
		fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		//fprintf (stderr, " RAN %02d - SF %01d - POF %02d", ran, sf, part_of_frame);
		fprintf (stderr, "%s", KNRM);
	}
	else fprintf (stderr, "        ");
	
	//reset scrambler seed to key value on new superframe
	if (part_of_frame == 0 && state->nxdn_cipher_type == 0x1) state->payload_miN = 0;

	if (crc == 0 && state->nxdn_sacch_non_superframe == FALSE) //only set these when superframe type, or get odd results (can't verify correct)
	{
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

	//Hand off to LEH NXDN SACCH handling
	if (part_of_frame == 3 && state->nxdn_sacch_non_superframe == FALSE)
 	{
		NXDN_SACCH_Full_decode (opts, state);
	} 

	if (opts->payload == 1)
	{ 
		fprintf (stderr, "\n"); 
		fprintf (stderr, " SACCH Segment #%d - %05X", part_of_frame+1, sacch_segment);
		if (crc != 0) fprintf (stderr, " CRC ERR - %02X %02X", crc, check);
	}
	
}

void nxdn_deperm_facch2_udch(dsd_opts * opts, dsd_state * state, uint8_t bits[348])
{

	uint8_t deperm[348];
	uint8_t depunc[406];
	uint8_t trellis_buf[199];
	int id = 0;
	uint16_t crc = 0;

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
	trellis_decode(trellis_buf, depunc, 199);
	crc = crc15(trellis_buf, 199);

	uint8_t MessageType = 0;
	for (int i = 0; i < 6; i++)
	{
		MessageType = MessageType << 1;
		MessageType = MessageType + trellis_buf[i+2]; //on +2 on any facch1
	}

	fprintf (stderr, " FACCH2/UDCH ");
	if (crc == 0) NXDN_Elements_Content_decode(opts, state, 1, trellis_buf); 
	else
	{
		fprintf (stderr, "CRC ERR ");
		//NXDN_Elements_Content_decode(opts, state, 0, trellis_buf);
	} 

}

void nxdn_deperm_cac(dsd_opts * opts, dsd_state * state, uint8_t bits[300])
{

	uint8_t deperm[300];
	uint8_t depunc[350];
	uint8_t decode[171];
	int id = 0;
	uint16_t crc = 0;

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
	trellis_decode(decode, depunc, 171);
	crc = crc16cac(decode, 171); 

	fprintf (stderr, " CAC ");
	
	//these messages can also be double depending on CAC type, so need to check that first, and split the 
	//decode first, then we can run each message type and element decode if necessary
	uint8_t MessageType = 0;
	for (int i = 0; i < 6; i++)
	{
		MessageType = MessageType << 1;
		MessageType = MessageType + decode[i+2]; //double check this on CAC
	}

	//nxdn_message_type (opts, state, MessageType);

	//not sure if CAC will contain same type of 'elements' as a FACCH or SACCH
	//or if its purely for Control Signalling

	//need to check CAC type, CAC can contain two messages, or one long message,
	//then we need to break up the messages and process them individually
	if (crc == 0) NXDN_Elements_Content_decode(opts, state, 1, decode); 
	else
	{
		fprintf (stderr, " CRC ERR ");
		//NXDN_Elements_Content_decode(opts, state, 0, decode);
	} 

}

//its own function now to consolidate all the message types to one area to keep track of
//may need to add a 'source' of message so we don't call messages not associated with cac, facch, sacch, etc
void nxdn_message_type (dsd_opts * opts, dsd_state * state, uint8_t MessageType)
{

	//if (opts->payload == 1) fprintf (stderr, " M%02X ", MessageType);
	fprintf (stderr, "%s", KYEL);
	if      (MessageType == 0x10) fprintf(stderr, " IDLE");
	else if (MessageType == 0x01) fprintf(stderr, " VCALL");
	else if (MessageType == 0x03) fprintf(stderr, " VCALL IV");
	
	else if (MessageType == 0x07) fprintf(stderr, " TX_REL_EX");
	else if (MessageType == 0x08) fprintf(stderr, " TX_REL");
	

	//CAC only? valid in all, or just CAC?
	else if (MessageType == 0x04) fprintf(stderr, " VCALL ASSGN");
	else if (MessageType == 0x0E) fprintf(stderr, " DCALL ASSGN");
	else if (MessageType == 0x18) fprintf(stderr, " SITE_INFO");
	else if (MessageType == 0x19) fprintf(stderr, " SRV_INFO"); //per Op25
	else if (MessageType == 0x1C) fprintf(stderr, " FAIL_STAT_INFO"); //per the damn manual
	else if (MessageType == 0x1A) fprintf(stderr, " CCH_INFO");
	else if (MessageType == 0x1B) fprintf(stderr, " ADJ_SITE_INFO");
	else if (MessageType == 0x20) fprintf(stderr, " REG_RESP");
	else if (MessageType == 0x22) fprintf(stderr, " REG_C_RESP");
	else if (MessageType == 0x24) fprintf(stderr, " GRP_REG_RESP");
	else if (MessageType == 0x32) fprintf(stderr, " STAT_REQ");
	else if (MessageType == 0x33) fprintf(stderr, " STAT_RESP");
	else if (MessageType == 0x38) fprintf(stderr, " SDCALL_REQ_HEADER");
	else if (MessageType == 0x39) fprintf(stderr, " SDCALL_REQ_USERDATA");
	else if (MessageType == 0x3B) fprintf(stderr, " SDCALL_RESP");
	//end CAC only?

	else if (MessageType == 0x3F) fprintf(stderr, " ALIAS"); //observed from DSDPlus and OP25 dumps
	else if (MessageType == 0x0C) fprintf(stderr, " DCALL_ACK"); //need to verify these for accuracy vs DSDPlus
	else fprintf(stderr, " Unknown M%02X", MessageType);
	fprintf (stderr, "%s", KNRM);

	//zero out stale values so they won't persist after a transmit release
	if (MessageType == 0x08 || MessageType == 0x10)
	{
		memset (state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));
		state->nxdn_last_rid = 0;
		state->nxdn_last_tg = 0;
		state->nxdn_cipher_type = 0;
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

//utility functions here, consider moving to its own file later
static inline void cfill(uint8_t result[], const uint8_t src[], int len)
{
	for (int i=0; i<len; i++)
		result[i] = load_i(src+i*8, 8);
}

static inline int load_i(const uint8_t val[], int len) {
	int acc = 0;
	for (int i=0; i<len; i++){
		acc = (acc << 1) + (val[i] & 1);
	}
	return acc;
}

// trellis_1_2 encode: source is in bits, result in bits
static inline void trellis_encode(uint8_t result[], const uint8_t source[], int result_len, int reg)
{
	for (int i=0; i<result_len; i+=2) {
		reg = (reg << 1) | source[i>>1];
		result[i] = PARITY[reg & 0x19];
		result[i+1] = PARITY[reg & 0x17];
	}
}

// simplified trellis 2:1 decode; source and result in bits
// assumes that encoding was done with NTEST trailing zero bits
// result_len should be set to the actual number of data bits
// in the original unencoded message (excl. these trailing bits)
static inline void trellis_decode(uint8_t result[], const uint8_t source[], int result_len)
{
	int reg = 0;
	int min_d;
	int min_bt;
	static const int NTEST = 4;
	static const int NTESTC = 1 << NTEST;
	uint8_t bt[NTEST];
	uint8_t tt[NTEST*2];
	int dstats[4];
	int sum;
	for (int p=0; p < 4; p++)
		dstats[p] = 0;
	for (int p=0; p < result_len; p++) {
		for (int i=0; i<NTESTC; i++) {
			bt[0] = (i&8)>>3;
			bt[1] = (i&4)>>2;
			bt[2] = (i&2)>>1;
			bt[3] = (i&1);
			trellis_encode(tt, bt, NTEST*2, reg);
			sum=0;
			for (int j=0; j<NTEST*2; j++) {
				sum += tt[j] ^ source[p*2+j];
			}
			if (i == 0 || sum < min_d) {
				min_d = sum;
				min_bt = bt[0];
			}
		}
		result[p] = min_bt;
		reg = (reg << 1) | min_bt;
		dstats[(min_d > 3) ? 3 : min_d] += 1;
	}
	// fprintf (stderr, "stats\t%d %d %d %d\n", dstats[0], dstats[1], dstats[2], dstats[3]);
}

static uint8_t crc6(const uint8_t buf[], int len)
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

static uint16_t crc12f(const uint8_t buf[], int len)
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

static uint16_t crc15(const uint8_t buf[], int len)
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

static uint16_t crc16cac(const uint8_t buf[], int len)
{
	int crc = 0xc3ee;
        int poly = (1<<12) + (1<<5) + 1;
	for (int i=0;i<len;i++) {
                crc = ((crc << 1) | buf[i]) & 0x1ffff;
                if(crc & 0x10000)
                        crc = (crc & 0xffff) ^ poly;
	}
        crc = crc ^ 0xffff;
        return crc & 0xffff;
}