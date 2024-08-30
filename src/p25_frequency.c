/*-------------------------------------------------------------------------------
 * p25_frequency.c
 * P25 Channel to Frequency Calculator
 *
 * NXDN Channel to Frequency Calculator
 * 
 * LWVMOBILE
 * 2022-11 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//P25
long int process_channel_to_freq (dsd_opts * opts, dsd_state * state, int channel)
{
	UNUSED(opts);

	//RX and SU TX frequencies.
	//SU RX = (Base Frequency) + (Channel Number) x (Channel Spacing).

	/*
	Channel Spacing: This is a frequency multiplier for the channel
	number. It is used as a multiplier in other messages that specify
	a channel field value. The channel spacing (kHz) is computed as
	(Channel Spacing) x (0.125 kHz).
	*/

	//return 0 if channel value is 0 or 0xFFFF
	if (channel == 0) return 0;
	if (channel == 0xFFFF) return 0;

	//Note: Base Frequency is calculated as (Base Frequency) x (0.000005 MHz) from the IDEN_UP message.

	long int freq = -1;
	int iden = channel >> 12; 
	int type = state->p25_chan_type[iden];
	int slots_per_carrier[16] = {1,1,1,2,4,2,2,2,2,2,2,2,2,2,2,2}; //from OP25
	int step = (channel & 0xFFF) / slots_per_carrier[type];

	//first, check channel map
	if (state->trunk_chan_map[channel] != 0)
	{
		freq = state->trunk_chan_map[channel];
		fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
		return (freq);
	}

	//if not found, attempt to find it via calculation 
	else
	{
		if (state->p25_base_freq[iden] != 0)
		{
			freq = (state->p25_base_freq[iden] * 5) + ( step * state->p25_chan_spac[iden] * 125);
					fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
					return (freq);
		}
		else 
		{
				fprintf (stderr, "\n  Base Frequency Not Found - Iden [%d]", iden);
				fprintf(stderr, "\n    or Channel not found in import file");
				return (0);
		}
	}	

}

long int nxdn_channel_to_frequency(dsd_opts * opts, dsd_state * state, uint16_t channel)
{
	UNUSED(opts);

	long int freq;
	long int base = 0;
	long int step = 0;

	//reworked to include Direct Frequency Assignment if available

	//first, check channel map for imported value, DFA systems most likely won't need an import,
	//unless it has 'system definable' attributes
	if (state->trunk_chan_map[channel] != 0)
	{
		freq = state->trunk_chan_map[channel];
		fprintf (stderr, "\n  Frequency [%.6lf] MHz", (double)freq/1000000);
		return (freq);
	}

	//then, let's see if its DFA instead -- 6.5.36
	else if (state->nxdn_rcn == 1)
	{
		//determine the base frequency in Hz
		if (state->nxdn_base_freq == 1)				base = 100000000; //100 MHz
		else if (state->nxdn_base_freq == 2) 	base = 330000000; //330 Mhz
		else if (state->nxdn_base_freq == 3)	base = 400000000; //400 Mhz
		else if (state->nxdn_base_freq == 4)	base = 750000000; //750 Mhz
		else base = 0; //just set to zero, will be system definable most likely and won't be able to calc

		//determine the step value in Hz
		if (state->nxdn_step == 2)				step = 1250; //1.25 kHz
		else if (state->nxdn_step == 3)	step = 3125; //3.125 kHz
		else step = 0; //just set to zero, will be system definable most likely and won't be able to calc

		//if we have a valid base and step, then calc frequency
		//6.5.45. Outbound/Inbound Frequency Number (OFN/IFN)
		if (base && step)
		{
			freq = base + (channel * step);
			fprintf (stderr, "\n  DFA Frequency [%.6lf] MHz", (double)freq/1000000);
			return (freq);
		}
		else 
		{
			fprintf(stderr, "\n    Custom DFA Settings -- Unknown Freq;");
			return (0);
		}
		
	}

	else
	{
		fprintf(stderr, "\n    Channel not found in import file");
		return (0);
	} 

}