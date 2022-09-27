/*-------------------------------------------------------------------------------
 * p25_frequency.c
 * P25 Channel to Frequency Calculator
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

long int process_channel_to_freq (dsd_opts * opts, dsd_state * state, int channel)
{

	//RX and SU TX frequencies.
	//SU RX = (Base Frequency) + (Channel Number) x (Channel Spacing).

	/*
	Channel Spacing: This is a frequency multiplier for the channel
	number. It is used as a multiplier in other messages that specify
	a channel field value. The channel spacing (kHz) is computed as
	(Channel Spacing) x (0.125 kHz).
	*/

	//Note: Base Frequency is calculated as (Base Frequency) x (0.000005 MHz) from the IDEN_UP message.

	long int freq = -1;
	int iden = channel >> 12; 
	int type = state->p25_chan_type[iden];
	int slots_per_carrier[16] = {1,1,1,2,4,2,2,2,2,2,2,2,2,2,2,2}; //from OP25
	int step = (channel & 0xFFF) / slots_per_carrier[type];
	
	if (state->p25_base_freq[iden] != 0)
	{
		freq = (state->p25_base_freq[iden] * 5) + ( step * state->p25_chan_spac[iden] * 125);
        fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
        return (freq);
	}
	else 
    {
        fprintf (stderr, "\n  Base Frequency Not Found - Iden [%d]", iden);
        return (0);
    }

}