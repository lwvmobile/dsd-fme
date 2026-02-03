/*-------------------------------------------------------------------------------
 * nxdn_deperm.c
 * NXDN Depuncturing / Soft Decision Viterbi Decoding and Related Misc Functions
 *
 * Reworked portions from Osmocom OP25 rx_sync.cc
 * NXDN Encoder/Decoder (C) Copyright 2019 Max H. Parke KA1RBI
 *
 * LWVMOBILE
 * 2026-01 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "nxdn_const.h"

void nxdn_pn95_dibit_scrambler(dsd_state * state, uint8_t * dibits, int len)
{

	uint16_t lfsr = state->nxdn_pn95_seed; //default value is 228 / 0xE4

	uint16_t  bit = 0;
	uint8_t pN95[182];
	memset (pN95, 0, sizeof(pN95));

	for (int i = 0; i < len; i++)
	{
		//before feedback, take the bit
		pN95[i] = lfsr & 1;

		//since this is right shift, the taps are 0 and 4, and not 8 and 4 (9,5)
		bit = ((lfsr >> 4) ^ (lfsr >> 0)) & 1;
		lfsr >>= 1;
		lfsr |= (bit << 8);

	}

	//convert the pN sequence to a scramble table
	uint8_t scramble_table[182]; memset(scramble_table, 0, sizeof(scramble_table));
	int k = 0;
	for (int i = 0; i < len; i++)
	{
		if (pN95[i] == 1)
			scramble_table[k++] = i;
	}

	//the entries into the scramble_table are the dibits (symbols) that are inverted
	for (int i = 0; i < k; i++)
	{
		dibits[scramble_table[i]] ^= 0x2;
		dibits[scramble_table[i]] &= 0x3;
	}

	//debug, make sure we don't overflow dibits array
	// fprintf (stderr, " PN95: K: %d; Len: %d; ", k, len);

}

void nxdn_message_type (dsd_opts * opts, dsd_state * state, uint8_t MessageType)
{

	uint8_t flag1 = (MessageType >> 7) & 1;
	uint8_t flag2 = (MessageType >> 6) & 1;

	fprintf (stderr, "%s", KYEL);

	//NOTE: Most Req/Resp (request and respone) share same message type but differ depending on channel type
	//RTCH Outbound will take precedent when differences may occur (except CALL_ASSGN)
	//Messages Found on Table 6.4-12 List of Layer 3 Messages Part 1-A p. 143-144 
	
	if      (MessageType == 0x00) fprintf(stderr, " CALL_RESP");
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
	else if (MessageType == 0x0F) fprintf(stderr, " HEAD_DLY");
	else if (MessageType == 0x10) fprintf(stderr, " IDLE");
	else if (MessageType == 0x11) fprintf(stderr, " DISC");
	else if (MessageType == 0x17) fprintf(stderr, " DST_ID_INFO");
	else if (MessageType == 0x18) fprintf(stderr, " SITE_INFO");
	else if (MessageType == 0x19) fprintf(stderr, " SRV_INFO");
	else if (MessageType == 0x1A) fprintf(stderr, " CCH_INFO");
	else if (MessageType == 0x1B) fprintf(stderr, " ADJ_SITE_INFO");
	else if (MessageType == 0x1C) fprintf(stderr, " FAIL_STAT_INFO");
	else if (MessageType == 0x20) fprintf(stderr, " REG_RESP");
	else if (MessageType == 0x22) fprintf(stderr, " REG_C_RESP");
	else if (MessageType == 0x23) fprintf(stderr, " REG_COMM");
	else if (MessageType == 0x24) fprintf(stderr, " GRP_REG_RESP");
	else if (MessageType == 0x28) fprintf(stderr, " AUTH_INQ_REQ");
	else if (MessageType == 0x29) fprintf(stderr, " AUTH_INQ_RESP");
	else if (MessageType == 0x2A) fprintf(stderr, " AUTH_INQ_REQ2");
	else if (MessageType == 0x2B) fprintf(stderr, " AUTH_INQ_RESP2");
	else if (MessageType == 0x30) fprintf(stderr, " STAT_INQ_REQ");
	else if (MessageType == 0x31) fprintf(stderr, " STAT_INQ_RESP");
	else if (MessageType == 0x32) fprintf(stderr, " STAT_REQ");
	else if (MessageType == 0x33) fprintf(stderr, " STAT_RESP");
	else if (MessageType == 0x34) fprintf(stderr, " REM_CON_REQ");
	else if (MessageType == 0x35) fprintf(stderr, " REM_CON_RESP");
	else if (MessageType == 0x36) fprintf(stderr, " REM_CON_E_REQ");
	else if (MessageType == 0x37) fprintf(stderr, " REM_CON_E_RESP");
	else if (MessageType == 0x38) fprintf(stderr, " SDCALL_REQ_HEADER");
	else if (MessageType == 0x39) fprintf(stderr, " SDCALL_REQ_USERDATA");
	else if (MessageType == 0x3A) fprintf(stderr, " SDCALL_IV");
	else if (MessageType == 0x3B) fprintf(stderr, " SDCALL_RESP");

	//PROP_FORM handled by checkdown for ALIAS or other
	else if (MessageType == 0x3F) {}

	//ARIB STD-T102 第 2 編 w/ F1 on, F2 off //NOTE: These three are handled by dcr_sacch internally, but idle may still hit on sacch
	else if (MessageType == 0x81) {} //fprintf(stderr, " VCALL");  //音声通信
	else if (MessageType == 0x88) {} //fprintf(stderr, " TX_REL"); //終話
	else if (MessageType == 0x90) {} //fprintf(stderr, " IDLE");   //アイドル

	//observed from #318 and found in ARIB STD-B54, F1 and flags on (p. 22) listed in table order
	else if (MessageType == 0xE1) fprintf(stderr, " VCALL_STD_B54");  //選択呼出音声通信
	else if (MessageType == 0xE2) fprintf(stderr, " GPS_HEADER");     //GPS ヘッダ
	else if (MessageType == 0xE3) fprintf(stderr, " GPS_DATA");       //GPS データ
	else if (MessageType == 0xE4) fprintf(stderr, " BEARER_HEADER");  //ベアラヘッダ
	else if (MessageType == 0xE5) fprintf(stderr, " BEARER_DATA");    //ベアラデータ
	else if (MessageType == 0xE7) fprintf(stderr, " ALIAS_STD_B54");  //発信者名情報
	else if (MessageType == 0xE8) fprintf(stderr, " TX_REL_STD_B54"); //選択呼出終話

	//NOTE: Other STD_B54 messages have F1 and F2 as 0, appear to mostly correspond to NXDN normalized messages 
	//(DCALL, STAT_RESP, etc) or are handled by PICH_TCH instead (dummy_header -> preamble(HEAD_DLY), etc)

	//any unknown message w/ associated flags
	else fprintf(stderr, " Unknown Message: %02X; F1: %d; F2: %d; ", MessageType, flag1, flag2);

	fprintf (stderr, "%s", KNRM);

	//Zero out stale values on DISC or TX_REL only (IDLE messaages occur often on NXDN96 VCH, and randomly on Type-C FACCH1 steals for some reason)
	if (MessageType == 0x08 || MessageType == 0x11)
	{
		memset (state->generic_talker_alias[0], 0, sizeof(state->generic_talker_alias[0]));
		sprintf (state->generic_talker_alias[0], "%s", "");
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

void LFSR128n(dsd_state * state)
{
  //generate a 128-bit IV from a 64-bit IV for AES blocks
  unsigned long long int lfsr = state->payload_miN;

  //start packing aes_iv
	state->aes_iv[0] = (lfsr >> 56) & 0xFF;
	state->aes_iv[1] = (lfsr >> 48) & 0xFF;
	state->aes_iv[2] = (lfsr >> 40) & 0xFF;
	state->aes_iv[3] = (lfsr >> 32) & 0xFF;
	state->aes_iv[4] = (lfsr >> 24) & 0xFF;
	state->aes_iv[5] = (lfsr >> 16) & 0xFF;
	state->aes_iv[6] = (lfsr >> 8 ) & 0xFF;
	state->aes_iv[7] = (lfsr >> 0 ) & 0xFF;


  int cnt = 0; int x = 64;
  unsigned long long int bit;
  //polynomial P(x) = 1 + X15 + X27 + X38 + X46 + X62 + X64
  for(cnt=0;cnt<64;cnt++)
  {
    //63,61,45,37,27,14
    // Polynomial is C(x) = x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    bit = ((lfsr >> 63) ^ (lfsr >> 61) ^ (lfsr >> 45) ^ (lfsr >> 37) ^ (lfsr >> 26) ^ (lfsr >> 14)) & 0x1;
    lfsr = (lfsr << 1) | bit;

    //continue packing aes_iv
		state->aes_iv[x/8] = (state->aes_iv[x/8] << 1) + bit;
    x++;
  }

	fprintf (stderr, "%s", KYEL);
		fprintf (stderr, "\n");
	fprintf (stderr, " IV(128): 0x");
	for (x = 0; x < 16; x++)
		fprintf (stderr, "%02X", state->aes_iv[x]);
	fprintf (stderr, "%s", KNRM);

}

//viterbi bit and byte array sizes are max value for largest required (facch2/udch)
uint8_t viterbi_bits[26*8];
uint8_t viterbi_bytes[26];

//puncture pattern arrays
uint8_t sacch_puncture[12]  = {1,1,1,1,1,0,1,1,1,1,1,0}; //matrix is read top, bottom, top, bottom...
uint8_t facch1_puncture[4]  = {1,0,1,1};
uint8_t facch2_puncture[14] = {1,1,1,0,1,1,1,1,1,1,1,0,1,1};
uint8_t cac_puncture[14]    = {1,1,1,0,1,1,1,1,1,1,1,0,1,1};

//facch1 storage, for duplicate message check
static uint8_t facch1_storage[12] = {0};

//sacch soft decision making
void nxdn_sacch(dsd_opts * opts, dsd_state * state, uint8_t * bits)
{

	//erase facch1 storage (facch1 only happens along with a sacch/scch frame)
	memset(facch1_storage, 0, sizeof(facch1_storage));

	uint8_t crc = 1;   //value computed by crc6 on payload
	uint8_t check = 0; //value pulled from last 6 bits
	int sf = 0;
	int ran = 0;
	int part_of_frame = 0;

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset (viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 60;
	int p_len = 12;
	int num_bytes = 5;
	int offset = 7;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_12_5, sacch_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " S-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc6(viterbi_bits, 26); //32
	for (int i = 0; i < 6; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[i+26];
	}

	//if the crc is bad, its possible the SR value for part of frame is bad, so
	//its better to invalidate all SACCH frames in storage to prevent
	//random data splicing passing to the sacch assembly for decode
	if (crc != check)
	{
		//reset the sacch field
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
	}

	//FIRST! If part of a non_superframe, and CRC is good, send directly to NXDN_Elements_Content_decode
	if (state->nxdn_sacch_non_superframe == TRUE)
	{
		if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		else fprintf (stderr, "        ");

		//needed for DES and AES
		// state->nxdn_part_of_frame = 0; //might be needed for VCALL_IV ks gen, but first voice should be in a SF SACCH PF 1/4

		uint8_t nsf_sacch[26];
		memset (nsf_sacch, 0, sizeof(nsf_sacch));
		for (int i = 0; i < 26; i++)
			nsf_sacch[i] = viterbi_bits[i+8];

		if (crc == check)
		{
			ran = (viterbi_bits[2] << 5) | (viterbi_bits[3] << 4) | (viterbi_bits[4] << 3) | (viterbi_bits[5] << 2) | (viterbi_bits[6] << 1) | viterbi_bits[7];
			state->nxdn_last_ran = ran;
		}

		//indicate whether or not this individual sacch is valid
		if (crc == check)
		{
			state->nxdn_part_of_frame = 3; //set to 3 so that a starting SF SACCH sequence check will be okay
			fprintf (stderr, "PF 1/1");
		}
		else
		{
			state->nxdn_part_of_frame = 0; //will not be an expected next value
			fprintf (stderr, "PF X/1");
		}

		if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
		else if (state->forced_alg_id == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed

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
				fprintf (stderr, "[%02X]", viterbi_bytes[i]);
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
		sf = (viterbi_bits[0] << 1) | viterbi_bits[1];

		if      (sf == 3) part_of_frame = 0;
		else if (sf == 2) part_of_frame = 1;
		else if (sf == 1) part_of_frame = 2;
		else if (sf == 0) part_of_frame = 3;
		else part_of_frame = 0;

		//sequence check, if not expected next part of frame,
		//or the first frame, then invalidate all sacch fields
		uint8_t valid_sequence = 0;
		// if (part_of_frame == ((state->nxdn_part_of_frame+1)%4) )
		if (crc == check && (part_of_frame == ((state->nxdn_part_of_frame+1)%4)) )
			valid_sequence = 1;
		else if (crc == check && part_of_frame == 0)
			valid_sequence = 1;

		//its better to invalidate all SACCH frames in storage to prevent
		//random data splicing passing to the sacch assembly for decode
		if (valid_sequence == 0)
		{
			//reset the sacch field
			memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
			memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
		}

		//needed for DES and AES
		state->nxdn_part_of_frame = part_of_frame;

		fprintf (stderr, "%s", KCYN);
		if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		else fprintf (stderr, "        ");
		fprintf (stderr, "%s", KNRM);

		//indicate whether or not this individual sacch is valid or in sequence
		if (crc == check && valid_sequence == 1)
			fprintf (stderr, "PF %d/4", part_of_frame+1);
		else if (crc == check && part_of_frame == 0)
			fprintf (stderr, "PF %d/4", part_of_frame+1);
		else fprintf (stderr, "PF X/4");

		if (part_of_frame == 0)
		{
			if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
			else if (state->forced_alg_id == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
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
			else if (state->forced_alg_id == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
		}
		//this seems to work much better now
		else if (part_of_frame != 0 && state->nxdn_cipher_type == 0x1)
		{
			if (state->nxdn_cipher_type == 1 && state->R != 0) state->payload_miN = state->R; //reset scrambler seed
			else if (state->forced_alg_id == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed

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
			ran = (viterbi_bits[2] << 5) | (viterbi_bits[3] << 4) | (viterbi_bits[4] << 3) | (viterbi_bits[5] << 2) | (viterbi_bits[6] << 1) | viterbi_bits[7];
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
			sacch_segment = sacch_segment + viterbi_bits[i+8];
			state->nxdn_sacch_frame_segment[part_of_frame][i] = viterbi_bits[i+8];
		}

		//Hand off to LEH NXDN_SACCH_Full_decode
		if (part_of_frame == 3)
			NXDN_SACCH_Full_decode (opts, state);

		if (opts->payload == 1)
		{
			fprintf (stderr, "\n");
			fprintf (stderr, " SACCH SF Segment #%d ", part_of_frame+1);
			for (int i = 0; i < 4; i++)
				fprintf (stderr, "[%02X]", viterbi_bytes[i]);

			if (crc != check) fprintf (stderr, " CRC ERR - %02X %02X", crc, check);
		}

	}

}

void nxdn_facch1(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t frame)
{

	uint16_t crc = 0;
	uint16_t check = 0;

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset(viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 144;
	int p_len = 4;
	int num_bytes = 12;
	int offset = 8;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_16_9, facch1_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " F1-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc12f (viterbi_bits, 80);
	for (int i = 0; i < 12; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[80+i];
	}

	//duplicate frame check if frame == 2
	uint8_t duplicate = 0;
	if (frame == 2)
	{
		if (memcmp(facch1_storage, viterbi_bytes, 12) == 0)
			duplicate = 1;
	}

	//erase facch1 storage
	memset(facch1_storage, 0, sizeof(facch1_storage));

	//store facch1 message viterbi bytes if frame == 1
	if (frame == 1)
		memcpy(facch1_storage, viterbi_bytes, sizeof(facch1_storage));

	if (crc == check && duplicate == 0) NXDN_Elements_Content_decode(opts, state, 1, viterbi_bits);
	// else if (opts->aggressive_framesync == 0) NXDN_Elements_Content_decode(opts, state, 0, viterbi_bits);

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " FACCH1 Payload ");
		for (int i = 0; i < 12; i++)
		{
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);
		}
		if (crc != check && opts->payload == 1)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);

			//debug
			// fprintf (stderr, " %03X / %03X", crc, check);
		}
	}

}

static int cac_fail = 0;
void nxdn_cac(dsd_opts * opts, dsd_state * state, uint8_t * bits)
{

	int ran = 0;
	uint16_t crc = 0;

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset (viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 300;
	int p_len = 14;
	int num_bytes = 22;
	int offset = 8;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_12_25, cac_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " C-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc16cac(viterbi_bits, 171);

	uint8_t cac_message_buffer[147]; //171
	memset (cac_message_buffer, 0, sizeof(cac_message_buffer));

	//shift the cac_message into the appropriate byte arrangement for element_decoder -- skip SR field and ending crc
	for (int i = 0; i < 147; i++)
	{
		cac_message_buffer[i] = viterbi_bits[i+8];
	}

	if (state->nxdn_last_ran != -1) fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
	else fprintf (stderr, "        ");

	if (crc == 0)
	{
		ran = (viterbi_bits[2] << 5) | (viterbi_bits[3] << 4) | (viterbi_bits[4] << 3) | (viterbi_bits[5] << 2) | (viterbi_bits[6] << 1) | viterbi_bits[7];
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
	else state->nxdn_part_of_frame = 3; //reset expected SF SACCH PF to 3 if trunking and no sync break

	//check for accumulative cac failures and reset if multiple errors pile up
	if (crc != 0) cac_fail++;
	else cac_fail = 0;

	if (crc == 0) NXDN_Elements_Content_decode(opts, state, 1, cac_message_buffer);

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " CAC Payload\n  ");
		for (int i = 0; i < 22; i++)
		{
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);
			if (i == 10) fprintf (stderr, "\n  ");
		}
		if (crc != 0) fprintf (stderr, " CRC ERR ");

	}

	//reset some parameters if CAC continues to fail
	if (cac_fail > 5)
	{
		//simple reset
		state->synctype = -1; //was 0, which is p25p1
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

}

void idas_scch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t direction)
{

	//erase facch1 storage (facch1 only happens along with a sacch/scch frame)
	memset(facch1_storage, 0, sizeof(facch1_storage));

	fprintf (stderr, "%s", KYEL);
	fprintf (stderr, " SCCH");

	uint8_t crc = 1;   //value computed by crc6 on payload
	uint8_t check = 0; //value pulled from last 6 bits
	int sf = 0;
	int part_of_frame = 0;

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset (viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 60;
	int p_len = 12;
	int num_bytes = 5;
	int offset = 7;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_12_5, sacch_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " S-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc7_scch(viterbi_bits, 25);
	for (int i = 0; i < 7; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[i+25];
	}

	//check the sf early for scrambler reset, if required
	sf = (viterbi_bits[0] << 1) | viterbi_bits[1];
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
		else if (state->forced_alg_id == 1 && state->R != 0) state->payload_miN = state->R; //force reset scrambler seed
	}

	if (crc == check) NXDN_decode_scch (opts, state, viterbi_bits, direction);
	// else if (opts->aggressive_framesync == 0) NXDN_decode_scch (opts, state, viterbi_bits, direction);

	fprintf (stderr, "%s", KNRM);

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n SCCH Payload ");
		for (int i = 0; i < 4; i++)
		{
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);
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

void idas_facch3_udch2(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t type)
{

	
	uint8_t f3_udch2[288]; //completed bitstream without crc and tailing bits attached
	uint8_t f3_udch2_bytes[48]; //completed bytes - with crc and tail
	uint16_t crc[2]; //crc calculated by function
	uint16_t check[2]; //crc from payload for comparison

	memset (crc, 0, sizeof(crc));
	memset (check, 0, sizeof(check));
	memset (f3_udch2, 0, sizeof(f3_udch2));
	memset (f3_udch2_bytes, 0, sizeof(f3_udch2_bytes));

	for (int j = 0; j < 2; j++)
	{

		memset(viterbi_bits, 0, sizeof(viterbi_bits));
		memset(viterbi_bytes, 0, sizeof(viterbi_bytes));

		//soft decision based viterbi
		int d_len = 144;
		int p_len = 4;
		int num_bytes = 12;
		int offset = 8;
		uint32_t error =
			nxdn_soft_decision_viterbi(bits+(144*j), PERM_16_9, facch1_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

		//track viterbi error / cost metric
		state->m17_viterbi_err = (float)error/(float)0xFFFF;

		//debug
		if (opts->payload == 1)
			fprintf (stderr, " F3U2-Ve: %1.1f; ", state->m17_viterbi_err);

		crc[j] = crc12f (viterbi_bits, 80);
		for (int i = 0; i < 12; i++)
		{
			check[j] = check[j] << 1;
			check[j] = check[j] | viterbi_bits[80+i];
		}

		//transfer to storage
		for (int i = 0; i < 80; i++) f3_udch2[i+(j*80)] = viterbi_bits[i];
		for (int i = 0; i < 12; i++) f3_udch2_bytes[i+(j*12)] = viterbi_bytes[i];

	}

	fprintf (stderr, "%s", KYEL);
	if (type == 0) fprintf (stderr, " UDCH2");
	if (type == 1) fprintf (stderr, " FACCH3");
	fprintf (stderr, "%s", KNRM);


	if (crc[0] == check[0] && crc[1] == check[1])
	{
		if (type == 1) NXDN_Elements_Content_decode(opts, state, 1, f3_udch2);
		if (type == 0) {} //need handling for user data (text messages and AVL)
	}

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
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		if (type == 0) fprintf (stderr, " UDCH2");
		if (type == 1) fprintf (stderr, " FACCH3");
		fprintf (stderr, " Payload \n  ");
		for (int i = 0; i < 12; i++)
			fprintf (stderr, "[%02X]", f3_udch2_bytes[i]);

		if (crc[0] != check[0])
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
			fprintf (stderr, " - %03X %03X", check[0], crc[0]);
		}

		fprintf (stderr, "\n  ");

		for (int i = 12; i < 24; i++)
			fprintf (stderr, "[%02X]", f3_udch2_bytes[i]);

		if (crc[1] != check[1])
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
			fprintf (stderr, " - %03X %03X", check[1], crc[1]);
		}

	}

}

void dcr_pich_tch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t lich)
{

	uint16_t crc = 0;
	uint16_t check = 0;

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset(viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 144;
	int p_len = 4;
	int num_bytes = 12;
	int offset = 8;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_16_9, facch1_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " PT-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc12f (viterbi_bits, 80);
	for (int i = 0; i < 12; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[80+i];
	}

	//STD-T98 DCR suggests TCH1 has data, and TCH2 will be zero fill
	//could vary by PDU, but limited data points suggests the same
	if (crc == check)
	{
		uint8_t  opcode = (uint8_t)ConvertBitIntoBytes(&viterbi_bits[0], 8);
		uint8_t  gi     = viterbi_bits[16];
		uint16_t source = (uint16_t)ConvertBitIntoBytes(&viterbi_bits[24], 16);
		uint16_t target = (uint16_t)ConvertBitIntoBytes(&viterbi_bits[40], 16);

		//SB0 with CSM
		if (lich == 0x08)
		{
			unsigned long long int csm = 0;
			for (int i = 0; i < 9; i++)
			{
				csm <<= 4;
				uint8_t bcd = (uint8_t)ConvertBitIntoBytes(&viterbi_bits[0+(i*4)], 4);

				// if (bcd < 10)
				// 	csm |= bcd;
				// else csm |= 0;

				csm |= bcd;
			}

			fprintf (stderr, "\n ");
			fprintf (stderr, "Call Sign Memory: %09llX; ", csm);

			//Assigning this to talker alias, see notes below on decimal value
			sprintf (state->generic_talker_alias[0], "CSM %09llX", csm);
			sprintf (state->event_history_s[0].Event_History_Items[0].alias, "CSM %09llX", csm);

			//convert from hex to string to decimal w/ sscanf
			// char csm_str[32]; memset(csm_str, 0, sizeof(csm_str));
			// sprintf (csm_str, "%llX", csm);
			// unsigned long long int csm_dec = 0;
			// sscanf(csm_str, "%lld", &csm_dec);

			//the issue is that this value will exceed the bit allotment for nxdn src value (needs long long at 36-bit)
			//even values in range 100000000 - 200000000 are 33-bits minimum
			// fprintf (stderr, "Call Sign Memory: %09lld; ", csm_dec);
		}
		//anything else
		else
		{

			//may only be relevant on MFID 0x30 "F.R.C." Radios
			if (opcode == 0x0F)
			{
				fprintf (stderr, "\n ");
				fprintf (stderr, "Source: %d; Target: %d; ", source, target);
				if (gi)
					fprintf (stderr, "Private; ");
				else fprintf (stderr, "Group; ");
				
				fprintf (stderr, "Data Preamble; ");
				uint8_t countdown = (uint8_t)ConvertBitIntoBytes(&viterbi_bits[64], 8);
				fprintf (stderr, "Countdown: %d; ", countdown);

			}

			//may only be relevant on MFID 0x30 "F.R.C." Radios
			if (opcode == 0x32)
			{
				fprintf (stderr, "\n ");
				fprintf (stderr, "Source: %d; Target: %d; ", source, target);
				if (gi)
					fprintf (stderr, "Private; ");
				else fprintf (stderr, "Group; ");
				
				fprintf (stderr, "Precoded Message; ");
				uint8_t idx = (uint8_t)ConvertBitIntoBytes(&viterbi_bits[64], 8);
				fprintf (stderr, "Index#: %d;", idx);

			}

		}

		// if (opcode == 0x00)
		// {
		// 	fprintf (stderr, "\n NULL TCH; ");
		// }
			
	}
	else if (opts->payload == 0)
	{
		fprintf (stderr, "\n ");
		fprintf (stderr, "%s", KRED);
		if (lich == 0x08)
			fprintf (stderr, "PICH (CRC ERR)");
		else fprintf (stderr, "TCH (CRC ERR)");
		fprintf (stderr, "%s", KNRM);
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		if (lich == 0x08)
			fprintf (stderr, " PICH Payload ");
		else fprintf (stderr, " TCH Payload ");
		for (int i = 0; i < 12; i++)
		{
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);
		}
		if (crc != check)
		{
			fprintf (stderr, "%s", KRED);
			fprintf (stderr, " (CRC ERR)");
			fprintf (stderr, "%s", KNRM);
		}
	}

}

void dcr_sacch(dsd_opts * opts, dsd_state * state, uint8_t * bits)
{
	
	uint8_t crc = 1;   //value computed by crc6 on payload
	uint8_t check = 0; //value pulled from last 6 bits

	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset (viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 60;
	int p_len = 12;
	int num_bytes = 5;
	int offset = 7;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_12_5, sacch_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " S-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc6(viterbi_bits, 26); //32
	for (int i = 0; i < 6; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[i+26];
	}

	crc = crc6(viterbi_bits, 26);
	check = (uint8_t) convert_bits_into_output(viterbi_bits+26, 6);

	//Configuration of Single Message or Multi Part Message
	uint8_t sf_fb  = viterbi_bits[0];
	uint8_t sf_num = (uint8_t) convert_bits_into_output(viterbi_bits+1, 2);
	uint8_t sf_mes = (uint8_t) convert_bits_into_output(viterbi_bits+3, 5);
	uint8_t sf_pof = 3-sf_num;

	if (crc == check)
	{
		if (sf_fb && sf_pof) //single message, single unit
			fprintf (stderr, "PF: %d/1; ", sf_num+1);
		else //multiple unit message
			fprintf (stderr, "PF: %d/4; ", sf_pof+1);

		if (sf_mes == 0x01)
			fprintf (stderr, "Call; ");
		else if (sf_mes == 0x02)
			fprintf (stderr, "PDU;  ");
		else if (sf_mes == 0x1E)
			fprintf (stderr, "End;  ");
		else if (sf_mes == 0x00)
			fprintf (stderr, "Idle; ");
		else fprintf (stderr, "Res: %02X; ", sf_mes);

	}
	else if (crc != check)
	{
		fprintf (stderr, "%s", KRED);
		fprintf (stderr, "SACCH (CRC ERR)");
		fprintf (stderr, "%s", KNRM);
	}

	if (crc == check)
		state->nxdn_sacch_frame_segcrc[sf_num] = 0;
	else state->nxdn_sacch_frame_segcrc[sf_num] = 1;

	//entire superframe has good crc
	uint8_t crc_sf_check = 0;
	for (int i = 0; i < 4; i++)
		crc_sf_check += state->nxdn_sacch_frame_segcrc[i];

	//values for storage and which parts to store
	int sf_full = 26; //full size of a sacch frame, minus CRC6
	int sf_size = 18; //size of superframe portion (18 bits)
	int sf_end  = 0;  //end of sf
	int sf_idx = sf_size*sf_pof;  //index position for this frame compared to super frame
	int bf_idx = sf_full-sf_size; //index position for buffer to superframe

	if (sf_fb && sf_pof) //single unit message
	{
		memset (state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
		memcpy(state->dmr_pdu_sf[0]+0, viterbi_bits+bf_idx, sf_size*sizeof(uint8_t));
	}
	else //multiple unit message
		memcpy(state->dmr_pdu_sf[0]+sf_idx, viterbi_bits+bf_idx, sf_size*sizeof(uint8_t));

	//if force application of scrambler key, then let's reset, regardless of CRC check
	if (sf_fb && state->forced_alg_id == 1)
		state->payload_miN = 0;

	//currently using static values so event log will log something, and do wav files, etc
	//disable this is random false positive for this lich code triggers this often enough
	if (crc == check)
	{
		state->gi[0] = 0;
		state->nxdn_last_ran = 7;
		state->nxdn_last_tg = 777;
		state->nxdn_last_rid = 777;

		//disabled with CSM going into Alias Value
		// sprintf (state->generic_talker_alias[0], "%s", "JPN DCR");
		// sprintf (state->event_history_s[0].Event_History_Items[0].alias, "%s; ", "JPN DCR");

		//sf_fb is the head message in a multi part, or the only message in a single part message
		if (sf_fb)
			state->payload_miN = 0;
	}

	//check for valid crc on single, or on all received
	if ( (sf_fb && sf_pof && crc == check) || //single frame
	     (sf_num == sf_end && crc_sf_check == 0)         ) //multi part frame
	{
		uint8_t cipher = (uint16_t) convert_bits_into_output(state->dmr_pdu_sf[0]+0, 2);
		uint16_t user_code = (uint16_t) convert_bits_into_output(state->dmr_pdu_sf[0]+2, 9);
		fprintf (stderr, "UC: %03d; ", user_code);
		if (cipher == 0x01)
		{
			fprintf (stderr, "Scrambler; ");
			state->nxdn_cipher_type = 1;
			if (state->R != 0)
				fprintf (stderr, "Key: %lld; ", state->R);
		}
		else if (cipher != 0x00)
		{
			fprintf (stderr, "Reserved Comms: %d; ", cipher);
		}

		//set enc bit here so we can tell playSynthesizedVoice whether or not to play enc traffic
		if (state->nxdn_cipher_type != 0)
			state->dmr_encL = 1;
		if (state->nxdn_cipher_type == 0 || state->R != 0)
			state->dmr_encL = 0;

		//this always appears to be 0, but could be other values
		uint8_t mfid = (uint16_t) convert_bits_into_output(state->dmr_pdu_sf[0]+11, 7);
		if (mfid != 0)
			fprintf (stderr, "MFID: %02X; ", mfid);

		//multi-part message, continue decoding
		if (sf_fb == 0 && sf_num == 0)
		{

			fprintf (stderr, "\n");

			//can't find definitions for these elements, even when MT == 1 and MFID == 0
			unsigned long long int mes_hex = (unsigned long long int ) convert_bits_into_output(state->dmr_pdu_sf[0]+18, 54);
			fprintf (stderr, " Message: %014llX; ", mes_hex << 0);

		}
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n DCR SACCH ");
		for (int i = 0; i < 4; i++)
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);

		if (sf_num == sf_end)
		{
			fprintf (stderr, "\n DCR SFULL ");
			for (int i = 0; i < 9; i++)
				fprintf (stderr, "[%02X]", (uint8_t)convert_bits_into_output(state->dmr_pdu_sf[0]+(i*8), 8));
		}

	}

	//clear out if run, or crc error
	if (sf_num == sf_end)
	{
		memset (state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
	}
	else if (sf_fb && sf_pof) //single
	{
		memset (state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
		memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
		memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));
	}

}

//Using Denny's FACCH2 sample with station id in it for testing fixed remaining issues here
void nxdn_facch2_udch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t type)
{

	uint16_t crc = 0;
	uint16_t check = 0;
	
	memset (viterbi_bits, 0, sizeof(viterbi_bits));
  memset(viterbi_bytes, 0, sizeof(viterbi_bytes));

	//soft decision based viterbi
	int d_len = 406;
	int p_len = 14;
	int num_bytes = 25;
	int offset = 7;
	uint32_t error =
		nxdn_soft_decision_viterbi(bits, PERM_12_29, facch2_puncture, d_len, p_len, num_bytes, offset, viterbi_bits, viterbi_bytes);

	//track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

	//debug
	if (opts->payload == 1)
		fprintf (stderr, " F2U-Ve: %1.1f; ", state->m17_viterbi_err);

	crc = crc15(viterbi_bits, 184);
	for (int i = 0; i < 15; i++)
	{
		check = check << 1;
		check = check | viterbi_bits[i+184];
	}

	int sf  = (viterbi_bits[0] << 1) | viterbi_bits[1]; //not sure why an SF field if there is no SACCH, unless UDCH data is segmented this way?
	int ran = (viterbi_bits[2] << 5) | (viterbi_bits[3] << 4) | (viterbi_bits[4] << 3) | (viterbi_bits[5] << 2) | (viterbi_bits[6] << 1) | viterbi_bits[7];
	if (crc == check)
	{
		state->nxdn_last_ran = ran;
		fprintf (stderr, " RAN %02d ", state->nxdn_last_ran);
		// fprintf (stderr, "PF %d/4", 4-sf); //on FACCH2 sample, was 4/4 sf = 0;
		state->nxdn_part_of_frame = 3 - sf; //this, or hardset 3?
	}
	else
	{
		fprintf (stderr, "        ");
		state->nxdn_part_of_frame = 0; //should be invalid, pretty sure all FACCH2 are set to sf 0 (pf 3)
	}

	fprintf (stderr, "%s", KYEL);
	if (type == 0) fprintf (stderr, " UDCH");
	if (type == 1) fprintf (stderr, " FACCH2");
	fprintf (stderr, "%s", KNRM);

	uint8_t f2u_message_buffer[26*8];
	memset (f2u_message_buffer, 0, sizeof(f2u_message_buffer));

	//facch2 and udch have SR info in it, skip past that
	for (int i = 0; i < 199-8-15; i++)
		f2u_message_buffer[i] = viterbi_bits[i+8];

	if (crc == check)
	{
		if (type == 1) NXDN_Elements_Content_decode(opts, state, 1, f2u_message_buffer);
		if (type == 0) {} //need handling for user data (text messages and AVL)
	}

	if (type == 0 && crc == check)
	{
		fprintf (stderr, "\n UDCH Data: "  );
		for (int i = 0; i < 24; i++) //all but last crc portion
			fprintf (stderr, "%02X", viterbi_bytes[i]);

		fprintf (stderr, "\n UDCH Data: ASCII - "  );
		for (int i = 0; i < 24; i++) //remove crc portion
		{
			if (viterbi_bytes[i] <= 0x7E && viterbi_bytes[i] >=0x20)
			{
				fprintf (stderr, "%c", viterbi_bytes[i]);
			}
			else fprintf (stderr, " ");
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
			fprintf (stderr, "[%02X]", viterbi_bytes[i]);
		}

	}

	if (crc != check)
	{
		fprintf (stderr, "%s", KRED);
		fprintf (stderr, " (CRC ERR)");
		fprintf (stderr, "%s", KNRM);

		//debug
		// fprintf (stderr, " C: %04X; E: %04X;", crc, check);
	}

}
