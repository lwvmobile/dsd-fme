#include "dsd.h"
#include "dstar_const.h"
#include "dstar_header.h"

//simplified DSTAR
void processDSTAR(dsd_opts * opts, dsd_state * state)
{
  uint8_t sd[480];
  memset (sd, 0, sizeof(sd));
	int i, j, dibit;
	char ambe_fr[4][24];
	const int *w, *x;
  memset(ambe_fr, 0, sizeof(ambe_fr));
  w = dW;
  x = dX;

	//20 voice and 19 slow data frames (20th is frame sync)
  for (j = 0; j < 21; j++)
  {

    memset(ambe_fr, 0, sizeof(ambe_fr));
    w = dW;
    x = dX;

    for (i = 0; i < 72; i++)
    {
      dibit = getDibit(opts, state);
      ambe_fr[*w][*x] = dibit & 1;
      w++;
      x++;
    }

    soft_mbe(opts, state, NULL, ambe_fr, NULL);

    if (j != 20)
    {
      for (i = 0; i < 24; i++)
      {
        //slow data
        sd[(j*24)+i] = getDibit(opts, state);
      }
    }

  }

  fprintf (stderr, "\n");


}

void processDSTAR_HD(dsd_opts * opts, dsd_state * state) {

  int i;
  int radioheaderbuffer[660];

  for (i = 0; i < 660; i++)
    radioheaderbuffer[i] = getDibit(opts, state);

  dstar_header_decode(state, radioheaderbuffer);
  processDSTAR(opts, state);

}
