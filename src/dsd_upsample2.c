#include "dsd.h"

static const float fdmdv_os_filter48[48]= {
    -3.55606818e-04,
    -8.98615286e-04,
    -1.40119781e-03,
    -1.71713852e-03,
    -1.56471179e-03,
    -6.28128960e-04,
    1.24522223e-03,
    3.83138676e-03,
    6.41309478e-03,
    7.85893186e-03,
    6.93514929e-03,
    2.79361991e-03,
    -4.51051400e-03,
    -1.36671853e-02,
    -2.21034939e-02,
    -2.64084653e-02,
    -2.31425052e-02,
    -9.84218694e-03,
    1.40648474e-02,
    4.67316298e-02,
    8.39615986e-02,
    1.19925275e-01,
    1.48381174e-01,
    1.64097819e-01,
    1.64097819e-01,
    1.48381174e-01,
    1.19925275e-01,
    8.39615986e-02,
    4.67316298e-02,
    1.40648474e-02,
    -9.84218694e-03,
    -2.31425052e-02,
    -2.64084653e-02,
    -2.21034939e-02,
    -1.36671853e-02,
    -4.51051400e-03,
    2.79361991e-03,
    6.93514929e-03,
    7.85893186e-03,
    6.41309478e-03,
    3.83138676e-03,
    1.24522223e-03,
    -6.28128960e-04,
    -1.56471179e-03,
    -1.71713852e-03,
    -1.40119781e-03,
    -8.98615286e-04,
    -3.55606818e-04
};


//from CODEC2
//float filter /* Generate using fir1(47,1/2) in Octave */
static const float fdmdv_os_filter[48]= {
    -0.0008215855034550382,
    -0.0007833023901802921,
     0.001075563790768233,
     0.001199092367787555,
    -0.001765309502928316,
    -0.002055372115328064,
     0.002986877604154257,
     0.003462567920638414,
    -0.004856570111126334,
    -0.005563143845031497,
     0.007533613299748122,
     0.008563932468880897,
    -0.01126857129039911,
    -0.01280782411693687,
     0.01651443896361847,
     0.01894875110322284,
    -0.02421604439474981,
    -0.02845107338464062,
     0.03672973563400258,
     0.04542046150312214,
    -0.06189165826716491,
    -0.08721876380763803,
     0.1496157094199961,
     0.4497962274137046,
     0.4497962274137046,
     0.1496157094199961,
    -0.08721876380763803,
    -0.0618916582671649,
     0.04542046150312216,
     0.03672973563400257,
    -0.02845107338464062,
    -0.02421604439474984,
     0.01894875110322284,
     0.01651443896361848,
    -0.01280782411693687,
    -0.0112685712903991,
     0.008563932468880899,
     0.007533613299748123,
    -0.005563143845031501,
    -0.004856570111126346,
     0.003462567920638419,
     0.002986877604154259,
    -0.002055372115328063,
    -0.001765309502928318,
     0.001199092367787557,
     0.001075563790768233,
    -0.0007833023901802925,
    -0.0008215855034550383
};

//produce 6 float samples (48k) for every 1 float sample (8k)
void upsampleF (float invalue, float prev, float outbuf[6])
{

  float c, d;

  c = prev; //may ned up needing storage for the prev value to prevent the high pitch 'ring' sound
  d = invalue;

  // basic triangle interpolation
  outbuf[0] = c = ((invalue * (float) 0.166) + (c * (float) 0.834));
  outbuf[1] = c = ((invalue * (float) 0.332) + (c * (float) 0.668));
  outbuf[2] = c = ((invalue * (float) 0.500) + (c * (float) 0.5));
  outbuf[3] = c = ((invalue * (float) 0.668) + (c * (float) 0.332));
  outbuf[4] = c = ((invalue * (float) 0.834) + (c * (float) 0.166));
  outbuf[5] = c = d;
  prev = d; //shift prev to the last d value for the next repitition

  // fdmdv_os_filter48

}

//produce 6 short samples (48k) for every 1 short sample (8k)
void upsampleS (short invalue, short prev, short outbuf[6])
{

  float c, d;

  c = prev;
  d = invalue;

  // basic triangle interpolation
  outbuf[0] = c = ((invalue * (float) 0.166) + (c * (float) 0.834));
  outbuf[1] = c = ((invalue * (float) 0.332) + (c * (float) 0.668));
  outbuf[2] = c = ((invalue * (float) 0.500) + (c * (float) 0.5));
  outbuf[3] = c = ((invalue * (float) 0.668) + (c * (float) 0.332));
  outbuf[4] = c = ((invalue * (float) 0.834) + (c * (float) 0.166));
  outbuf[5] = c = d;
  prev = d; //shift prev to the last d value for the next repitition

}

// void fdmdv_8_to_48_short(short out48k[], short in8k[], int n)
// {
//     int i,j,k,l;
//     float acc;

//     //FDMDV_OS_TAPS_48K = 48
//     //FDMDV_OS_48 = 6
//     //unsure of what the n value would be, could be number of samples to process?
    
//     for(i=0; i<n; i++) {
// 	for(j=0; j<FDMDV_OS_48; j++) {
// 	    acc = 0.0;
// 	    for(k=0,l=0; k<FDMDV_OS_TAPS_48K; k+=FDMDV_OS_48,l++)
// 		acc += fdmdv_os_filter48[k+j]*in8k[i-l];
// 	    out48k[i*FDMDV_OS_48+j] = acc*FDMDV_OS_48;	    
// 	}
//     }	

//     /* update filter memory */

//     for(i=-FDMDV_OS_TAPS_48_8K; i<0; i++)
// 	in8k[i] = in8k[i + n];
// }

//this might be useful for the cygwin 'warning' that pops up during compilation

/* set up the calling convention for DLL function import/export for
   WIN32 cross compiling */
/*
#ifdef __CODEC2_WIN32__
#ifdef __CODEC2_BUILDING_DLL__
#define CODEC2_WIN32SUPPORT __declspec(dllexport) __stdcall //but use optarg?
#else
#define CODEC2_WIN32SUPPORT __declspec(dllimport) __stdcall
#endif
#else
#define CODEC2_WIN32SUPPORT
#endif
*/



//modified from CODEC2 source code
/*
   t48_8_short.c
   David Rowe
   Dec 2021
*/
void fdmdv_8_to_48_float(float out48k[], float in8k[], int n)
{
  int i,j,k,l;
  // int a,b;
  // a = 48;
  // b = 6;
  float baseline = 0.0f;

  //n = #define N8 159 /* processing buffer size at 8 kHz (odd number deliberate) */
  //a = FDMDV_OS_TAPS_48K = 48
  //b = FDMDV_OS_48 = 6
  //unsure of what the n value would be, could be number of samples to process, or to produce?

  float empty[160];
  memset (empty, baseline, sizeof(empty));
  //skip processing if its dead silence (2v or SF)
  if (memcmp(empty, in8k, sizeof(empty)) == 0)
    goto END_48f;

  // //add a float decimation value here -- switch to 192.0f * aout_gain later on
  // for (i = 0; i < 160; i++)
  //   in8k[i] /= 4800.0f;

  //shut up the warnings for now
  UNUSED(fdmdv_os_filter); //this filter sounds a bit 'tinny' and 'thin'
  // UNUSED(fdmdv_os_filter48); //this filter has a 'dampening' effect

  for(i=0; i<n; i++) 
  {
    for(j=0; j<6; j++)
    {
      out48k[i*6+j] = 0.0f;
      for(k=0,l=0; k<48; k+=6,l++)
        out48k[i*6+j] += fdmdv_os_filter48[k+j]*in8k[i-l];

      out48k[i*6+j] *= 6.0f;
    
    }
  }

  /* update filter memory */ //is this working properly?
  // for(i=-a; i<0; i++)
  //   in8k[i] = in8k[i + n];

  //doesn't really seem to matter much if before or after, I wonder if we lose a smmidge of float precision though going before
  //add a float decimation value here -- switch to 192.0f * aout_gain later on
  for (i = 0; i < 960; i++)
    out48k[i] /= 4800.0f;

  //debug
  // fprintf (stderr, "\n SAMPS = ");
  for (i = 0; i < 960; i++)
  {
    if (!(i % 6))
      fprintf (stderr, "\n SAMPS = ");
    fprintf (stderr, "(%f) ", out48k[i]);
  }
    


  END_48f: ; //do nothing
}