/*-------------------------------------------------------------------------------
 * p25p2_frame.c
 * Phase 2 TDMA Frame Processing
 *
 * original copyrights for portions used below (OP25 DUID table, MAC len table)
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//DUID Look Up Table from OP25
static const int16_t duid_lookup[256] = { //128 triggers false 4V on bad signal
	0,0,0,-1,0,-1,-1,1,0,-1,-1,4,-1,8,2,-1,
	0,-1,-1,1,-1,1,1,1,-1,3,9,-1,5,-1,-1,1,
	0,-1,-1,10,-1,6,2,-1,-1,3,2,-1,2,-1,2,2,
	-1,3,7,-1,11,-1,-1,1,3,3,-1,3,-1,3,2,-1,
	0,-1,-1,4,-1,6,12,-1,-1,4,4,4,5,-1,-1,4,
	-1,13,7,-1,5,-1,-1,1,5,-1,-1,4,5,5,5,-1,
	-1,6,7,-1,6,6,-1,6,14,-1,-1,4,-1,6,2,-1,
	7,-1,7,7,-1,6,7,-1,-1,3,7,-1,5,-1,-1,15,
	-1,-1,-1,10,-1,8,12,-1,-1,8,9,-1,8,8,-1,8, //first value was 0 for 4V, changing to -1 for testing -- 1000 0000 (perfect 4V should be 0000 0000, so is this the correct hamming distance?)
	-1,13,9,-1,11,-1,-1,1,9,-1,9,9,-1,8,9,-1,
	-1,10,10,10,11,-1,-1,10,14,-1,-1,10,-1,8,2,-1,
	11,-1,-1,10,11,11,11,-1,-1,3,9,-1,11,-1,-1,15,
	-1,13,12,-1,12,-1,12,12,14,-1,-1,4,-1,8,12,-1,
	13,13,-1,13,-1,13,12,-1,-1,13,9,-1,5,-1,-1,15,
	14,-1,-1,10,-1,6,12,-1,14,14,14,-1,14,-1,-1,15,
	-1,13,7,-1,11,-1,-1,15,14,-1,-1,15,-1,15,15,15,
};

//4V and 2V deinterleave schedule
const int c0[25] = {
	23,5,22,4,21,3,20,2,19,1,18,0,
	17,16,15,14,13,12,11,10,9,8,7,6
};

const int c1[24] = { 
	10,9,8,7,6,5,22,4,21,3,20,2,
	19,1,18,0,17,16,15,14,13,12,11
};

const int c2[12] = { 
	3,2,1,0,10,9,8,7,6,5,4
};

const int c3[15] = { 
	13,12,11,10,9,8,7,6,5,4,3,2,1,0
};

const int csubset[73] = { 
	0,0,1,2,0,0,1,2,0,0,1,2,0,0,1,2,
	0,0,1,3,0,0,1,3,0,1,1,3,0,1,1,3,
	0,1,1,3,0,1,1,3,0,1,1,3,0,1,2,3,
	0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,
	0,1,2,3,0,1,2,3
};

const int *w;

char ambe_fr1[4][24] = {0};
char ambe_fr2[4][24] = {0};
char ambe_fr3[4][24] = {0};
char ambe_fr4[4][24] = {0};

//MAC message lengths
//moved to p25p2_vpdu.c

int ts_counter = 0; //timeslot counter for time slots 0-11
int p2bit[4320] = {0}; //4320
int p2lbit[8640] = {0}; //bits generated by lsfr scrambler, doubling up for offset roll-over
int p2xbit[4320] = {0}; //bits xored from p2bit and p2lbit
int dibit = 0;
int vc_counter = 0;
int framing_counter = 0;
int voice = 0; //if voice in vch 0 or vch 1

uint64_t isch = 0;
int isch_decoded = -1;
uint8_t p2_duid[8] = {0};
int16_t duid_decoded = -1;

int ess_b[2][96] = {0}; //96 bits for 4 - 24 bit ESS_B fields starting bit 168 (RS 44,16,29)
int ess_a[2][168] = {0}; //ESS_A 1 (96 bit) and 2 (72 bit) fields, starting at bit 168 and bit 266 (RS Parity)

int facch[2][156] = {0};
int facch_rs[2][114] = {0};

int sacch[2][180] = {0};
int sacch_rs[2][132] = {0};

//store an entire p2 superframe worth of dibits into a bit buffer
void p2_dibit_buffer (dsd_opts * opts, dsd_state * state)
{
	//we used to grab 2140 (entire superframe minus sync)
	//now we grab 700 (4 Timeslots minus sync)
  for (int i = 0; i < 700; i++) //2160 for an entire superframe of dibits - 20 sync dibits
  {
		//symbol capture reading now handled directly by getSymbol and getDibit
		dibit = getDibit(opts, state);
		//dibit inversion handled internally by getDibit if sync type is inverted
  	p2bit[i*2]   = (dibit >> 1) & 1;
  	p2bit[i*2+1] = (dibit & 1);

  }


}

void process_Frame_Scramble (dsd_opts * opts, dsd_state * state)
{
  UNUSED(opts);

  //The bits of the scramble sequence corresponding to signal bits that are not scrambled or not used are discarded.
  //descramble frame scrambled by LFSR of WACN, SysID, and CC(NAC)
  unsigned long long int seed = 0;

	//below calc is the same as shifting left the required number of bits.
	seed = ( (state->p2_wacn * 16777216) + (state->p2_sysid * 4096) + state->p2_cc );

  unsigned long long int bit = 1; //temp bit for storage during LFSR operation

  for (int i = 0; i < 4320; i++)
  {
  	//External LFSR in figure 7.1 BBAC

		//assign our scramble bit to the array
		p2lbit[i] = (seed >> 43) & 0x1;
		//assign same bit to position +4320 to allow for a rollover with an offset value
		p2lbit[i+4320] = (seed >> 43) & 0x1;
		//compute our next scramble bit and shift the seed register and append bit to LSB
		bit = ((seed >> 33) ^ (seed >> 19) ^ (seed >> 14) ^ (seed >> 8) ^ (seed >> 3) ^ (seed >> 43)) & 0x1;
		seed = (seed << 1) | bit;
  }

	for (int i = 0; i < 4300; i++)
  {
		//offset by 20 for sync, then 360 for each ts frame off from start of superframe
		p2xbit[i] = p2bit[i] ^ p2lbit[i+20+(360*state->p2_scramble_offset)];
	}

}

//process_MAC_VPDU
//moved to p25p2_vpdu.c

//process_SACCH_MAC_PDU and process_FACCH_MAC_PDU
//moved to p25p2_xcch.c

void process_FACCHc (dsd_opts * opts, dsd_state * state)
{
	//gather and process FACCH w/o scrambling (S-OEMI) so we know what to do with the containing data.
	for (int i = 0; i < 72; i++)
	{
		facch[state->currentslot][i] = p2bit[i+2+(ts_counter*360)];
	}
	//skip DUID 1
	for (int i = 0; i < 62; i++)
	{
		facch[state->currentslot][i+72] = p2bit[i+76+(ts_counter*360)];
	}
	//skip sync
	for (int i = 0; i < 22; i++)
	{
		facch[state->currentslot][i+134] = p2bit[i+180+(ts_counter*360)];
	}
	//gather FACCH RS parity bits
	for (int i = 0; i < 42; i++)
	{
		facch_rs[state->currentslot][i] = p2bit[i+202+(ts_counter*360)];
	}
	//skip DUID 3
	for (int i = 0; i < 72; i++)
	{
		facch_rs[state->currentslot][i+42] = p2bit[i+246+(ts_counter*360)];
	}

	//send payload and parity to ez_rs28_facch for error correction
	int ec = -2;
	ec = ez_rs28_facch (facch[state->currentslot], facch_rs[state->currentslot]);

	int opcode = 0;
	opcode = (facch[state->currentslot][0] << 2) | (facch[state->currentslot][1] << 1) | (facch[state->currentslot][2] << 0);

	if (state->currentslot == 0)
	{
		state->dmr_so = opcode;
	}
	else state->dmr_soR = opcode;

	if (ec > -1) //unsure of upper limit, CRC check will pass or fail from this point
	{
		process_FACCH_MAC_PDU (opts, state, facch[state->currentslot]);
	}
	else
	{
		fprintf (stderr, " R-S ERR Fc");
		//if (state->currentslot == 0) state->dmrburstL = 13;
		//else state->dmrburstR = 13;
	}

}

void process_FACCHs (dsd_opts * opts, dsd_state * state)
{
	//gather and process FACCH w scrambling (S-OEMI) so we know what to do with the containing data.
	for (int i = 0; i < 72; i++)
	{
		facch[state->currentslot][i] = p2xbit[i+2+(ts_counter*360)];
	}
	//skip DUID 1
	for (int i = 0; i < 62; i++)
	{
		facch[state->currentslot][i+72] = p2xbit[i+76+(ts_counter*360)];
	}
	//skip sync
	for (int i = 0; i < 22; i++)
	{
		facch[state->currentslot][i+134] = p2xbit[i+180+(ts_counter*360)];
	}
	//gather FACCh RS parity bits
	for (int i = 0; i < 42; i++)
	{
		facch_rs[state->currentslot][i] = p2xbit[i+202+(ts_counter*360)];
	}
	//skip DUID 3
	for (int i = 0; i < 72; i++)
	{
		facch_rs[state->currentslot][i+42] = p2xbit[i+246+(ts_counter*360)];
	}

	//send payload and parity to ez_rs28_facch for error correction
	int ec = -2;
	ec = ez_rs28_facch (facch[state->currentslot], facch_rs[state->currentslot]);

	int opcode = 0;
	opcode = (facch[state->currentslot][0] << 2) | (facch[state->currentslot][1] << 1) | (facch[state->currentslot][2] << 0);

	if (state->currentslot == 0)
	{
		state->dmr_so = opcode;
	}
	else state->dmr_soR = opcode;

	if (ec > -1) //unsure of upper limit, CRC check will pass or fail from this point
	{
		process_FACCH_MAC_PDU (opts, state, facch[state->currentslot]);
	}
	else
	{
		fprintf (stderr, " R-S ERR Fs");
		//if (state->currentslot == 0) state->dmrburstL = 13;
		//else state->dmrburstR = 13;
	}
}

void process_SACCHc (dsd_opts * opts, dsd_state * state)
{
	//gather and process SACCH w/o scrambling (I-OEMI) so we know what to do with the containing data.
	for (int i = 0; i < 72; i++)
	{
		sacch[state->currentslot][i] = p2bit[i+2+(ts_counter*360)];
	}
	//skip DUID 1
	for (int i = 0; i < 108; i++)
	{
		sacch[state->currentslot][i+72] = p2bit[i+76+(ts_counter*360)];
	}
	//start collecting parity
	for (int i = 0; i < 60; i++)
	{
		sacch_rs[state->currentslot][i] = p2bit[i+184+(ts_counter*360)];
	}
	//skip DUID 3
	for (int i = 0; i < 72; i++)
	{
		sacch_rs[state->currentslot][i+60] = p2bit[i+246+(ts_counter*360)];
	}

	//send payload and parity to ez_rs28_sacch for error correction
	int ec = -2;
	ec = ez_rs28_sacch (sacch[0], sacch_rs[0]);

	int opcode = 0;
	opcode = (sacch[state->currentslot][0] << 2) | (sacch[state->currentslot][1] << 1) | (sacch[state->currentslot][2] << 0);

	//set inverse true for SACCH
	if (state->currentslot == 0)
	{
		state->dmr_soR = opcode;
	}
	else state->dmr_so = opcode;

	if (ec > -1) //unsure of upper limit, CRC check will pass or fail from this point
	{
		process_SACCH_MAC_PDU (opts, state, sacch[state->currentslot]);
	}
	else
	{
		fprintf (stderr, " R-S ERR Sc");
		// if (state->currentslot == 0) state->dmrburstL = 13;
		// else state->dmrburstR = 13;
	}

}

void process_SACCHs (dsd_opts * opts, dsd_state * state)
{
	//gather and process SACCH w scrambling (I-OEMI) so we know what to do with the containing data.
	for (int i = 0; i < 72; i++)
	{
		sacch[state->currentslot][i] = p2xbit[i+2+(ts_counter*360)];
	}
	//skip DUID 1
	for (int i = 0; i < 108; i++)
	{
		sacch[state->currentslot][i+72] = p2xbit[i+76+(ts_counter*360)];
	}
	//start collecting parity
	for (int i = 0; i < 60; i++)
	{
		sacch_rs[state->currentslot][i] = p2xbit[i+184+(ts_counter*360)];
	}
	//skip DUID 3
	for (int i = 0; i < 72; i++)
	{
		sacch_rs[state->currentslot][i+60] = p2xbit[i+246+(ts_counter*360)];
	}

	//send payload and parity to ez_rs28_sacch for error correction
	int ec = -2;
	ec = ez_rs28_sacch (sacch[0], sacch_rs[0]);

	int opcode = 0;
	opcode = (sacch[state->currentslot][0] << 2) | (sacch[state->currentslot][1] << 1) | (sacch[state->currentslot][2] << 0);

	//set inverse true for SACCH
	if (state->currentslot == 0)
	{
		state->dmr_soR = opcode;
	}
	else state->dmr_so = opcode;

	if (ec > -1) //unsure of upper limit, CRC check will pass or fail from this point
	{
		process_SACCH_MAC_PDU (opts, state, sacch[state->currentslot]);
	}
	else
	{
		fprintf (stderr, " R-S ERR Ss");
		// if (state->currentslot == 0) state->dmrburstL = 13;
		// else state->dmrburstR = 13;
	}

}

void process_ISCH (dsd_opts * opts, dsd_state * state)
{
  UNUSED(opts);

  isch = 0;
  for (int i = 0; i < 40; i++)
  {
   	isch = isch << 1;
   	isch = isch | p2bit[i+320+(360*framing_counter)];
  }

	if (isch == 0x575D57F7FF) //S-ISCH frame sync, pass;
	{
		//do nothing
	}
	else
	{
		isch_decoded = isch_lookup(isch);

		if (isch_decoded > -1)
		{
			int uf_count = isch_decoded & 0x3;
			int free = (isch_decoded >> 2) & 0x1;
			int isch_loc = (isch_decoded >> 3) & 0x3;
			int chan_num = (isch_decoded >> 5) & 0x3;
			UNUSED2(uf_count, free);
			state->p2_vch_chan_num = chan_num;

			//old rules for entire superframe worth
			//determine where the offset should be by first finding TS 0
			// if (chan_num == 0 && isch_loc == 0)
			// {
			// 	state->p2_scramble_offset = 11 - framing_counter;
			// }
			// //find TS1 if TS0 isn't found
			// else if (chan_num == 1 && isch_loc == 0)
			// {
			// 	state->p2_scramble_offset = 12 - framing_counter; 
			// }

			//new rules for relative position to the only chan 1 we should see
			if (chan_num == 1 && isch_loc == 0)
			{
				state->p2_scramble_offset = 12 - framing_counter; 
			}
			else if (chan_num == 1 && isch_loc == 1)
			{
				state->p2_scramble_offset = 4 - framing_counter; 
			}
			else if (chan_num == 1 && isch_loc == 2)
			{
				state->p2_scramble_offset = 8 - framing_counter; 
			}

		}
		else
		{
			//if -2(no return value) or -1(fec error)
		}

	}
	
	isch_decoded = -1; //reset to bad value after running

}

void process_4V (dsd_opts * opts, dsd_state * state)
{

	w = csubset;
	int b = 0;
	int q = 0;
	int r = 0;
	int s = 0;
	int t = 0;
	for (int x = 0; x < 72; x++)
	{
		int ww = *w;
		if (ww == 0)
		{
			b = c0[q];
			q++;
		}
		if (ww == 1)
		{
			b = c1[r];
			r++;
		}
		if (ww == 2)
		{
			b = c2[s];
			s++;
		}
		if (ww == 3)
		{
			b = c3[t];
			t++;
		}

		ambe_fr1[*w][b] = p2xbit[x+2+vc_counter];
		ambe_fr2[*w][b] = p2xbit[x+76+vc_counter];
		ambe_fr3[*w][b] = p2xbit[x+172+vc_counter];
		ambe_fr4[*w][b] = p2xbit[x+246+vc_counter];
		w++;
	}

	//collect our ESS_B fragments
	for (int i = 0; i < 24; i++)
	{
		state->ess_b[state->currentslot][i+(state->fourv_counter[state->currentslot]*24)] =
		p2xbit[i+148+vc_counter];
	}

	state->fourv_counter[state->currentslot]++;

	//sanity check, reset if greater than 3 (bad signal or tuned away)
	if (state->fourv_counter[state->currentslot] > 3)
	{
		state->fourv_counter[state->currentslot] = 0;
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
	}

	// #ifdef AERO_BUILD //FUN FACT: OSS stutters only on Cygwin, using padsp in linux, it actually opens two virtual /dev/dsp audio streams for output
	// //Cygwin OSS Slot Preference Pre-emption shim
	// if (opts->audio_out_type == 5)
	// {
	// 	//set to 21 for MBE OSS shim to preempt audio 
	// 	if (state->currentslot == 0) state->dmrburstL = 21;
	// 	else state->dmrburstR = 21;
	// }
	// #endif

	//unsure of the best location for these counter resets
	if (state->voice_counter[0] >= 18)
		state->voice_counter[0] = 0;

	if (state->voice_counter[1] >= 18)
		state->voice_counter[1] = 0;
	
	processMbeFrame (opts, state, NULL, ambe_fr1, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[0], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[0], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[0], state->s_lu, sizeof(state->s_lu));
	}
	else
	{
		memcpy(state->f_r4[0], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[0], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[0], state->s_ru, sizeof(state->s_ru));
	}

	processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[1], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[1], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[1], state->s_lu, sizeof(state->s_lu));
	}
	else
	{
		memcpy(state->f_r4[1], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[1], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[1], state->s_ru, sizeof(state->s_ru));
	}

	processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[2], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[2], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[2], state->s_lu, sizeof(state->s_lu));
	}
	else
	{
		memcpy(state->f_r4[2], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[2], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[2], state->s_ru, sizeof(state->s_ru));
	}

	processMbeFrame (opts, state, NULL, ambe_fr4, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[3], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[3], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[3], state->s_lu, sizeof(state->s_lu));
	}
	else
	{
		memcpy(state->f_r4[3], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[3], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[3], state->s_ru, sizeof(state->s_ru));
	}

}

void process_ESS (dsd_opts * opts, dsd_state * state)
{
	//collect and process ESS info (MI, Key ID, Alg ID)
	//hand over to (RS 44,16,29) decoder to receive ESS values

	int payload[96] = {0}; //local storage for ESS_A and ESS_B arrays
	for (int i = 0; i < 96; i++)
	{
		payload[i] = state->ess_b[state->currentslot][i];
	}

	int parity[168] = {0};
	for (int i = 0; i < 168; i++)
	{
		parity[i] = ess_a[state->currentslot][i];
	}

	int ec = 69; 
	ec = ez_rs28_ess(payload, parity); 

	int algid = 0;
	for (short i = 0; i < 8; i++)
	{
		algid = algid << 1;
		algid = algid | payload[i];
	}

	unsigned long long int essb_hex1 = 0;
	unsigned long long int essb_hex2 = 0;
	for (int i = 0; i < 32; i++)
	{
		essb_hex1 = essb_hex1 << 1;
		essb_hex1 = essb_hex1 | payload[i];
	}
	for (int i = 0; i < 64; i++)
	{
		essb_hex2 = essb_hex2 << 1;
		essb_hex2 = essb_hex2 | payload[i+32];
	}
	fprintf (stderr, "%s", KYEL);

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, " VCH %d - ESS_B %08llX%016llX ERR = %02d", state->currentslot, essb_hex1, essb_hex2, ec);
	}

	if (ec >= 0 && ec < 15) //corrected up to 14 errors and not -1 failure//
	{
		if (state->currentslot == 0)
		{
			state->payload_algid = (essb_hex1 >> 24) & 0xFF;
			state->payload_keyid = (essb_hex1 >> 8) & 0xFFFF;
			state->payload_miP =  ((essb_hex1 & 0xFF) << 56) | ((essb_hex2 & 0xFFFFFFFFFFFFFF00) >> 8);
			if (state->payload_algid != 0x80 && state->payload_algid != 0x0)
			{
				fprintf (stderr, "\n VCH 0 -");
				fprintf (stderr, " ALG ID 0x%02X", state->payload_algid);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyid);
				fprintf (stderr, " MI 0x%016llX", state->payload_miP);
				if (state->R != 0 && state->payload_algid == 0xAA) fprintf (stderr, " Key 0x%010llX", state->R);
				fprintf (stderr, " ESSB");
			}

		}
		if (state->currentslot == 1)
		{
			state->payload_algidR = (essb_hex1 >> 24) & 0xFF;
			state->payload_keyidR = (essb_hex1 >> 8) & 0xFFFF;
			state->payload_miN =  ((essb_hex1 & 0xFF) << 56) | ((essb_hex2 & 0xFFFFFFFFFFFFFF00) >> 8);
			if (state->payload_algidR != 0x80 && state->payload_algidR != 0x0)
			{
				fprintf (stderr, "\n VCH 1 -");
				fprintf (stderr, " ALG ID 0x%02X", state->payload_algidR);
				fprintf (stderr, " KEY ID 0x%04X", state->payload_keyidR);
				fprintf (stderr, " MI 0x%016llX", state->payload_miN);
				if (state->RR != 0 && state->payload_algidR == 0xAA) fprintf (stderr, " Key 0x%010llX", state->RR);
				fprintf (stderr, " ESSB");
			}

		}

		#define P25p2_ENC_LO //disable if this behavior is detremental
		#ifdef P25p2_ENC_LO

		//If trunking and tuning ENC calls is disabled, lock out and go back to CC
		int enc_lo = 1;
		int ttg = 0; //checking to a valid TG will help make sure we have a good MAC_PTT or SACCH Channel Update First
		int alg = 0; //set alg and key based on current slot values
		unsigned long long int key = 0;

		if (state->currentslot == 0)
		{
			ttg = state->lasttg;
			alg = state->payload_algid;
			if (alg == 0xAA) key = state->R;
			// else if (future condition) key = 1;
			// else if (future condition) key = 1;
		}

		if (state->currentslot == 1)
		{
			ttg = state->lasttgR;
			alg = state->payload_algidR;
			if (alg == 0xAA) key = state->RR;
			// else if (future condition) key = 1;
			// else if (future condition) key = 1;
		}

		if (alg != 0 && opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && opts->trunk_tune_enc_calls == 0)
		{
			//NOTE: This may still cause an issue IF we havent' loaded the key yet from keyloader
			if (alg == 0xAA && key != 0) enc_lo = 0;
			// else if (future condition) enc_lo = 0;
			// else if (future condition) enc_lo = 0;

			//if this is locked out by conditions above, then write it into the TG mode if we have a TG value assigned
			if (enc_lo == 1 && ttg != 0)
			{
				int xx = 0; int enc_wr = 0;
				for (xx = 0; xx < state->group_tally; xx++)
				{
					if (state->group_array[xx].groupNumber == ttg)
					{
						enc_wr = 1; //already in there, so no need to assign it
						break;
					}
				}

				//if not already in there, so save it there now
				if (enc_wr == 0)
				{
					state->group_array[state->group_tally].groupNumber = ttg;
					sprintf (state->group_array[state->group_tally].groupMode, "%s", "DE");
					sprintf (state->group_array[state->group_tally].groupName, "%s", "ENC LO"); //was xx and not state->group_tally
					state->group_tally++;
				}

				//return to the control channel -- NOTE: Disabled, just mark as lockout for now, return would require complex check of the other slot activity
				// fprintf (stderr, " No Enc Following on P25p2 Trunking; Return to CC; \n");
				// return_to_cc (opts, state);
			}
		}
		#endif //P25p2_ENC_LO

	}
	if (ec == -1 || ec >= 15)
	{
		//ESS R-S Failure -- run LFSR on current MI if applicable
		if (state->currentslot == 0 && state->payload_algid != 0x80 && state->payload_keyid != 0 && state->payload_miP != 0)
			LFSRP(state);
		if (state->currentslot == 1 && state->payload_algidR != 0x80 && state->payload_keyidR != 0 && state->payload_miN != 0)
			LFSRP(state);
	}
	fprintf (stderr, "%s", KNRM);

	state->fourv_counter[state->currentslot] = 0; 

}

void process_2V (dsd_opts * opts, dsd_state * state)
{

	w = csubset;
	int b = 0;
	int q = 0;
	int r = 0;
	int s = 0;
	int t = 0;
	for (int x = 0; x < 72; x++)
	{
		int ww = *w;
		if (ww == 0)
		{
			b = c0[q];
			q++;
		}
		if (ww == 1)
		{
			b = c1[r];
			r++;
		}
		if (ww == 2)
		{
			b = c2[s];
			s++;
		}
		if (ww == 3)
		{
			b = c3[t];
			t++;
		}

		ambe_fr1[*w][b] = p2xbit[x+2+vc_counter];
		ambe_fr2[*w][b] = p2xbit[x+76+vc_counter];
		w++;
	}

	//collect ESS_A and then run process_ESS
	for (short i = 0; i < 96; i++)
	{
		ess_a[state->currentslot][i] = p2xbit[i+148+vc_counter];
	}

	for (short i = 0; i < 72; i++) //load up ESS_A 2
	{
		ess_a[state->currentslot][i+96] = p2xbit[i+246+vc_counter];
	}

	if (opts->payload == 1)
	{
		fprintf (stderr, "\n");
	}

	// #ifdef AERO_BUILD //FUN FACT: OSS stutters only on Cygwin, using padsp in linux, it actually opens two virtual /dev/dsp audio streams for output
	// //Cygwin OSS Slot Preference Pre-emption shim
	// if (opts->audio_out_type == 5)
	// {
	// 	//set to 21 for MBE OSS shim to preempt audio 
	// 	if (state->currentslot == 0) state->dmrburstL = 21;
	// 	else state->dmrburstR = 21;
	// }
	// #endif

	//unsure of the best location for these counter resets
	if (state->voice_counter[0] >= 18)
		state->voice_counter[0] = 0;

	if (state->voice_counter[1] >= 18)
		state->voice_counter[1] = 0;

	processMbeFrame (opts, state, NULL, ambe_fr1, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[0], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[0], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[0], state->s_lu, sizeof(state->s_lu));
	}
	else
	{
		memcpy(state->f_r4[0], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[0], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[0], state->s_ru, sizeof(state->s_ru));
	}

	processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
	if(state->currentslot == 0)
	{
		memcpy(state->f_l4[1], state->audio_out_temp_buf, sizeof(state->audio_out_temp_buf));
		// memcpy(state->s_l4[1], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4[(state->voice_counter[0]++)%18], state->s_l, sizeof(state->s_l));
		memcpy(state->s_l4u[1], state->s_lu, sizeof(state->s_lu));

	}
	else
	{
		memcpy(state->f_r4[1], state->audio_out_temp_bufR, sizeof(state->audio_out_temp_bufR));
		// memcpy(state->s_r4[1], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4[(state->voice_counter[1]++)%18], state->s_r, sizeof(state->s_r));
		memcpy(state->s_r4u[1], state->s_ru, sizeof(state->s_ru));
	}
	// if (state->currentslot == 0) state->voice_counter[0] = 0; if (state->currentslot == 1) state->voice_counter[1] = 0;
	process_ESS(opts, state);

	//reset drop bytes after a 2V
	if (state->currentslot == 0 && state->payload_algid == 0xAA)
		state->dropL = 256;

	if (state->currentslot == 1 && state->payload_algidR == 0xAA)
		state->dropR = 256;

}

//P2 Data Unit ID
void process_P2_DUID (dsd_opts * opts, dsd_state * state)
{
	//DUID exist on all P25p2 frames, need to check this so we can process the TS frame properly
	vc_counter = 0;
	int err_counter = 0;

  for (ts_counter = 0; ts_counter < 4; ts_counter++) //12
  {
		duid_decoded = -2;
		int sacch = 0; UNUSED(sacch);
		p2_duid[0] = p2bit[0+(ts_counter*360)];
		p2_duid[1] = p2bit[1+(ts_counter*360)];
		p2_duid[2] = p2bit[74+(ts_counter*360)];
		p2_duid[3] = p2bit[75+(ts_counter*360)];
		p2_duid[4] = p2bit[244+(ts_counter*360)];
		p2_duid[5] = p2bit[245+(ts_counter*360)];
		p2_duid[6] = p2bit[318+(ts_counter*360)];
		p2_duid[7] = p2bit[319+(ts_counter*360)];

		//process p2_duid with (8,4,4) encoding/decoding
		int p2_duid_complete = 0;
		for (int i = 0; i < 8; i++)
		{
			p2_duid_complete = p2_duid_complete << 1;
			p2_duid_complete = p2_duid_complete | p2_duid[i];
		}
		duid_decoded = duid_lookup[p2_duid_complete];

		char * timestr = getTimeC();

		fprintf (stderr, "\n");
		fprintf (stderr,"%s ", timestr);
		fprintf (stderr, "       P25p2 ");
		
		if (timestr != NULL)
		{
			free (timestr);
			timestr = NULL;
		}


		if (state->currentslot == 0 && duid_decoded != 3 && duid_decoded != 12 && duid_decoded != 13 && duid_decoded != 4)
		{
			fprintf (stderr, "LCH 0 ");
			//open MBEout file - slot 1 - USE WITH CAUTION on Phase 2! Consider using a symbol capture bin instead!
			if (duid_decoded == 0 || duid_decoded == 6) //4V or 2V (voice)
			{
				voice = 1;
				if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
			}
		}
		else if (state->currentslot == 1 && duid_decoded != 3 && duid_decoded != 12 && duid_decoded != 13 && duid_decoded != 4)
		{
			fprintf (stderr, "LCH 1 ");
			//open MBEout file - slot 2 - USE WITH CAUTION on Phase 2! Consider using a symbol capture bin instead!
			if (duid_decoded == 0 || duid_decoded == 6) //4V or 2V (voice)
			{
				voice = 1;
      	if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_fR == NULL)) openMbeOutFileR (opts, state);
			}
		}
		//The LCCH may occupy LCH 0 or LCH 1 or both. BBAD 3.3 p8
		else if (duid_decoded == 13) //MAC_SIGNAL, or clear LCCH
		{
			// sacch = 1; //only an 'inverted' slot when its TS index 10 or 11
			fprintf (stderr, "LCCH  ");
		}
		else if (duid_decoded == 4) //Scrambled LCCH (TDMA_CC only...look in the manual again)
		{
			// sacch = 1; //only an 'inverted' slot when its TS index 10 or 11
			fprintf (stderr, "LCCHs ");
		}
		else
		{
			sacch = 1; //always an "inverted" sacch slot
			fprintf (stderr, "SACCH ");
		}

		//check to see when last voice activity occurred in order to allow tuning on phase 2
		//mac_signal or mac_idle when no more voice activity on current channel
		//this is primarily a fix for TDMA control channels that carry voice (Duke P25)
		//but may also allow for chain tuning without returning to the control channel <--may be problematic since we can assign a p25_cc_freq from the pdu
		if (duid_decoded == 13 && opts->p25_is_tuned == 1 && ((time(NULL) - state->last_vc_sync_time) > opts->trunk_hangtime) )  //version for MAC_SIGNAL only, no idle
		{
			opts->p25_is_tuned = 0;
			state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
			memset (state->active_channel, 0, sizeof (state->active_channel)); //zero out here? I think this will be fine
			//clear out stale voice samples left in the buffer and reset counter value
			state->voice_counter[0] = 0;
			state->voice_counter[1] = 0;
			memset(state->s_l4, 0, sizeof(state->s_l4));
			memset(state->s_r4, 0, sizeof(state->s_r4));
		}
		else if (duid_decoded == 13 && ((time(NULL) - state->last_active_time) > 2) && opts->p25_is_tuned == 0) //should we use && opts->p25_is_tuned == 1?
		{
			memset (state->active_channel, 0, sizeof (state->active_channel)); //zero out here? I think this will be fine
			//clear out stale voice samples left in the buffer and reset counter value
			state->voice_counter[0] = 0;
			state->voice_counter[1] = 0;
			memset(state->s_l4, 0, sizeof(state->s_l4));
			memset(state->s_r4, 0, sizeof(state->s_r4));
		}

		if (duid_decoded == 0)
		{
			fprintf (stderr, " 4V %d", state->fourv_counter[state->currentslot]+1);
			//debug see which 4V and which 2V randomly pop on Duke P25p2 CC (8 bit binary code)
			// fprintf (stderr, " DUID: %d", p2_duid_complete);
			if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0 &&
					state->p2_wacn != 0xFFFFF && state->p2_cc != 0xFFF && state->p2_sysid != 0xFFF)
			{
				state->last_vc_sync_time = time(NULL);
				process_4V (opts, state);
			}
		}
		else if (duid_decoded == 6)
		{
			fprintf (stderr, " 2V");
			//debug see which 4V and which 2V randomly pop on Duke P25p2 CC (8 bit binary code)
			// fprintf (stderr, " DUID: %d", p2_duid_complete);
			if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0 &&
					state->p2_wacn != 0xFFFFF && state->p2_cc != 0xFFF && state->p2_sysid != 0xFFF)
			{
				state->last_vc_sync_time = time(NULL);
				process_2V (opts, state);
			}
		}
		else if (duid_decoded == 3)
		{
			if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0 &&
					state->p2_wacn != 0xFFFFF && state->p2_cc != 0xFFF && state->p2_sysid != 0xFFF)
			{
				process_SACCHs(opts, state);
			}
		}
		else if (duid_decoded == 12)
		{
			process_SACCHc(opts, state);
		}
		else if (duid_decoded == 15)
		{
			process_FACCHc(opts, state);
		}
		else if (duid_decoded == 9)
		{
			if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0 &&
					state->p2_wacn != 0xFFFFF && state->p2_cc != 0xFFF && state->p2_sysid != 0xFFF)
			{
				process_FACCHs(opts, state);
			}
		}
		else if (duid_decoded == 13)
		{
			state->p2_is_lcch = 1;
			process_SACCHc(opts, state);
		}
		else if (duid_decoded == 4)
		{
			if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0 &&
					state->p2_wacn != 0xFFFFF && state->p2_cc != 0xFFF && state->p2_sysid != 0xFFF)
			{
				state->p2_is_lcch = 1;
				process_SACCHs(opts, state);
			}
		}
		else
		{
			fprintf (stderr, " DUID ERR %d", duid_decoded);
			//if (state->currentslot == 0) state->dmrburstL = 12;
			//else state->dmrburstR = 12;
			err_counter++;
		}
		if (err_counter > 1) //&& opts->aggressive_framesync == 1
		{
			//zero out values when errs accumulate in DUID
			//most likely cause will be signal drop or tuning away
			state->payload_algid = 0;
			state->payload_keyid = 0;
			state->payload_algidR = 0;
			state->payload_keyidR = 0;
			state->lastsrc = 0;
			state->lastsrcR = 0;
			state->lasttg = 0;
			state->lasttgR = 0;
			state->p2_is_lcch = 0;
			state->fourv_counter[0] = 0;
			state->fourv_counter[1] = 0;
			state->voice_counter[0] = 0;
  		state->voice_counter[1] = 0;

			goto END;
		}
		//since we are in a while loop, run ncursesPrinter here.
		if (opts->use_ncurses_terminal == 1)
		{
			ncursesPrinter(opts, state);
		}
		//add 360 bits to each counter
		vc_counter = vc_counter + 360;

		//debug enable both slots before playback
		// opts->slot1_on = 1;
		// opts->slot2_on = 1;

		//NOTE: Could be an issue if MAC_SIGNAL onn LCH 1 and voice in LCH 0? It might Stutter?
		if (sacch == 0 && ts_counter & 1 && opts->floating_point == 1 && opts->pulse_digi_rate_out == 8000)
				playSynthesizedVoiceFS4 (opts, state);

		// if (sacch == 0 && ts_counter & 1 && opts->floating_point == 0 && opts->pulse_digi_rate_out == 8000)
		// 		playSynthesizedVoiceSS4 (opts, state);

		// fprintf (stderr, " VCH0: %d;", state->voice_counter[0]); //debug
		// fprintf (stderr, " VCH1: %d;", state->voice_counter[1]); //debug

		//this works, but may still have an element of 'dual voice stutter' which was my initial complaint, but shouldn't 'lag' during trunking operations (hopefully)
		if ( (state->voice_counter[0] >= 18 || state->voice_counter[1] >= 18 ) && opts->floating_point == 0 && opts->pulse_digi_rate_out == 8000 && ts_counter & 1)
		{
			//debug test, see what each counter is at during playback on dual voice
			// fprintf (stderr, " VC1: %02d; VC2: %02d;", state->voice_counter[0], state->voice_counter[1] );

			playSynthesizedVoiceSS18 (opts, state);
			state->voice_counter[0] = 0; //reset
			state->voice_counter[1] = 0; //reset
		}

		//debug: fix burst indicator for ncurses if marginal signal
		// if (voice)
		// {
		// 	if (state->currentslot == 0) state->dmrburstL = 21;
		// 	else state->dmrburstR = 21;
		// }

		//flip slots after each TS processed
		if (state->currentslot == 0)
		{
			state->currentslot = 1;
		}
		else
		{
			state->currentslot = 0;
		}

		//reset voice after each compliment of 2 slots
		if (ts_counter & 1)
			voice = 0;

  }
	END:
	voice = 0; //reset before exit

}

void processP2 (dsd_opts * opts, dsd_state * state)
{
	state->dmr_stereo = 1; 
	p2_dibit_buffer (opts, state);
	voice = 0;

	//look at our ISCH values and determine location in superframe before running frame scramble
	for (framing_counter = 0; framing_counter < 4; framing_counter++)
	{
		//run ISCH in here so we know when to start descramble offset
		process_ISCH (opts, state); 
	}

	//set initial current slot depending on offset value
	if (state->p2_scramble_offset % 2)
	{
		state->currentslot = 1; 
	}
	else state->currentslot = 0; 

	//frame_scramble runs lfsr and creates an array of unscrambled bits to pull from
  process_Frame_Scramble (opts, state);

	//process DUID will run through all collected frames and handle them appropriately
  process_P2_DUID (opts, state);

	state->dmr_stereo = 0; 
	state->p2_is_lcch = 0;

  fprintf (stderr, "\n"); 
}
