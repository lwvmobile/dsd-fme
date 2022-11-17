/*-------------------------------------------------------------------------------
 * p25_frequency.c
 * P25 Channel to Frequency Calculator
 *
 * NXDN Channel to Frequency, Courtesy of IcomIcR20 and EricCottrell (base values)
 * 
 * LWVMOBILE
 * 2022-11 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//P25
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

//NXDN Channel to Frequency, Courtesy of IcomIcR20 on RR Forums
long int nxdn_channel_to_frequency(dsd_opts * opts, uint16_t channel)
{
	long int freq;
	long int base = 0;

	//the base freq value is per EricCottrell in the Understanding NXDN Trunking Forum Thread
	//will need to do some research or tests to confirm these values are accurate

	if ((channel > 0) && (channel <= 400))
	{
		if (opts->frame_nxdn48 == 1) base = 450000000;
		else base = 451000000;

		freq = base + (channel - 1) * 12500;
		fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
		return (freq);
	}
	else if ((channel >= 401) && (channel <= 800))
	{
		if (opts->frame_nxdn48 == 1) base = 460000000;
		else base = 461000000;

		freq = base + (channel - 401) * 12500;
		fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
		return (freq);
	}
	else if (channel > 800)
	{
		//write function to check the LCN import array for custom frequency values!
		//return (freq);
	}
	else
	{
		fprintf(stderr, "\n  This LCN is used for a non-standard frequency. No frequency can be reported");
		return (0);
	}

}