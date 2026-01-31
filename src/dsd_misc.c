/*-------------------------------------------------------------------------------
 * dsd_misc.c
 * Misc Code that needs to be reorganized and sorted out
 *
 *
 *
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//audio filter stuff sourced from: https://github.com/NedSimao/FilteringLibrary
//no license / information provided in source code
#define PI 3.141592653

//do we need this for test input sthit?
// #define PLOT_POINTS     100
// #define SAMPLE_TIME_S   0.001
// #define CUTOFFFREQ_HZ   50
// #define sine_freq       40
// #define sine_freq_1     500
// #define HFC 50
// #define LFC 100

void LPFilter_Init(LPFilter *filter, float cutoffFreqHz, float sampleTimeS)
{

	float RC=0.0;
	RC=1.0/(2*PI*cutoffFreqHz);
	filter->coef[0]=sampleTimeS/(sampleTimeS+RC);
	filter->coef[1]=RC/(sampleTimeS+RC);

	filter->v_out[0]=0.0;
	filter->v_out[1]=0.0;

}

float LPFilter_Update(LPFilter *filter, float v_in)
{

	filter->v_out[1]=filter->v_out[0];
	filter->v_out[0]=(filter->coef[0]*v_in) + (filter->coef[1]*filter->v_out[1]);

	return (filter->v_out[0]);

}


/********************************************************************************************************
 *                              HIGH PASS FILTER
********************************************************************************************************/
void HPFilter_Init(HPFilter *filter, float cutoffFreqHz, float sampleTimeS)
{

	float RC=0.0;
	RC=1.0/(2*PI*cutoffFreqHz);

	filter->coef=RC/(sampleTimeS+RC);

	filter->v_in[0]=0.0;
	filter->v_in[1]=0.0;

	filter->v_out[0]=0.0;
	filter->v_out[1]=0.0;

}

float HPFilter_Update(HPFilter *filter, float v_in)
{

	filter->v_in[1]=filter->v_in[0];
	filter->v_in[0]=v_in;

	filter->v_out[1]=filter->v_out[0];
	filter->v_out[0]=filter->coef * (filter->v_in[0] - filter->v_in[1]+filter->v_out[1]);

	return (filter->v_out[0]);

}

/********************************************************************************************************
 *                              BAND PASS FILTER
********************************************************************************************************/


void PBFilter_Init(PBFilter *filter, float HPF_cutoffFreqHz, float LPF_cutoffFreqHz, float sampleTimeS)
{

	LPFilter_Init(&filter->lpf, LPF_cutoffFreqHz, sampleTimeS);
	HPFilter_Init(&filter->hpf, HPF_cutoffFreqHz, sampleTimeS);

	filter->out_in=0.0;

}

float PBFilter_Update(PBFilter *filter, float v_in)
{

	filter->out_in=HPFilter_Update(&filter->hpf, v_in);

	filter->out_in=LPFilter_Update(&filter->lpf, filter->out_in);

	return (filter->out_in);

}

/********************************************************************************************************
 *                              NOTCH FILTER
********************************************************************************************************/


void NOTCHFilter_Init(NOTCHFilter *filter, float centerFreqHz, float notchWidthHz, float sampleTimeS)
{

	//filter frequency to angular (rad/s)
	float w0_rps=2.0 * PI *centerFreqHz;
	float ww_rps=2.0 * PI *notchWidthHz;

	//pre warp center frequency
	float w0_pw_rps=(2.0/sampleTimeS) * tanf(0.5 * w0_rps * sampleTimeS);

	//computing filter coefficients

	filter->alpha=4.0 + w0_rps*w0_pw_rps*sampleTimeS*sampleTimeS;
	filter->beta=2.0*ww_rps*sampleTimeS;

	//clearing input and output  buffers

	for (uint8_t n=0; n<3; n++)
	{
		filter->vin[n]=0;
		filter->vout[n]=0;
	}

}

float NOTCHFilter_Update(NOTCHFilter *filter, float vin)
{

	//shifting samples
	filter->vin[2]=filter->vin[1];
	filter->vin[1]=filter->vin[0];

	filter->vout[2]=filter->vout[1];
	filter->vout[1]=filter->vout[0];

	filter->vin[0]=vin;

	//compute new output
	filter->vout[0]=(filter->alpha*filter->vin[0] + 2.0 *(filter->alpha -8.0)*filter->vin[1] + filter->alpha*filter->vin[2]
	-(2.0f*(filter->alpha-8.0)*filter->vout[1]+(filter->alpha-filter->beta)*filter->vout[2]))/(filter->alpha+filter->beta);


	return (filter->vout[0]);
}

void init_audio_filters (dsd_state * state)
{
	//still not sure if this is even correct or not, but 48k sounds good now
  LPFilter_Init(&state->RCFilter, 960, (float)1/(float)48000);
  HPFilter_Init(&state->HRCFilter, 960, (float)1/(float)48000);

	//left and right variants for stereo output testing on digital voice samples
  LPFilter_Init(&state->RCFilterL, 960, (float)1/(float)16000);
  HPFilter_Init(&state->HRCFilterL, 960, (float)1/(float)16000);
  LPFilter_Init(&state->RCFilterR, 960, (float)1/(float)16000);
  HPFilter_Init(&state->HRCFilterR, 960, (float)1/(float)16000);

	//PBFilter_Init(PBFilter *filter, float HPF_cutoffFreqHz, float LPF_cutoffFreqHz, float sampleTimeS);
	//NOTCHFilter_Init(NOTCHFilter *filter, float centerFreqHz, float notchWidthHz, float sampleTimeS);

	//NOTE: PBFilter_init also inits a LPF and HPF, but on another set of filters, might be worth
	//testing just using the PBFilter by itself and see how it does without the hpf used

	//passband filter working (seems to be), notch filter unsure which values to use, doesn't have any appreciable affect when used as is
	PBFilter_Init(&state->PBF, 8000, 12000, (float)1/(float)1536000); //RTL Sampling at 1536000 S/s.
	NOTCHFilter_Init(&state->NF, 1000, 4000, (float)1/(float)1536000);

}

//FUNCTIONS for handing use of above filters

//lpf
void lpf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = LPFilter_Update(&state->RCFilter, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf
void hpf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilter, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf digital left
void hpf_dL(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilterL, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//hpf digital right
void hpf_dR(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = HPFilter_Update(&state->HRCFilterR, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//nf
void nf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = NOTCHFilter_Update(&state->NF, input[i]);
		// fprintf (stderr, "\n out: %05d", input[i]);
	}
}

//pbf
void pbf(dsd_state * state, short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
	{
		// fprintf (stderr, "\n in: %05d", input[i]);
		input[i] = PBFilter_Update(&state->PBF, input[i]);
	}
}

//Generic RMS function derived from RTL_FM (RTL_SDR) RMS code (doesnt' really work correctly outside of RTL)
long int raw_rms(int16_t *samples, int len, int step) //use samplespersymbol as len
{

  int i;
  long int rms;
  long p, t, s;
  double dc, err;

  p = t = 0L;
  for (i=0; i<len; i+=step) {
    s = (long)samples[i];
    t += s;
    p += s * s;
  }
  /* correct for dc offset in squares */
  dc = (double)(t*step) / (double)len;
  err = t * 2 * dc - dc * dc * len;

  rms = (long int)sqrt((p-err) / len);
  if (rms < 0) rms = 150;
  return rms;
}
