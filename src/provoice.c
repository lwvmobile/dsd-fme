#include "dsd.h"
#include "provoice_const.h"

void
processProVoice (dsd_opts * opts, dsd_state * state)
{
  int i, j, dibit, k;
  uint8_t lid[80];
  uint8_t lidbyte[10];
  uint8_t init[64];
  uint8_t initbyte[8];
  char imbe7100_fr1[7][24];
  char imbe7100_fr2[7][24];
  const int *w, *x;

  if (opts->errorbars == 1)
    {
      fprintf (stderr,"VOICE ");
    }

  for (i = 0; i < 64; i++)
    {
      dibit = getDibit (opts, state);
      init[i] = dibit;
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif
  //init Bits (64)...MI?
  if (opts->payload == 1)
  {
    k = 0;
    fprintf (stderr, "\nInit 64 ");
    for(i = 0; i < 8; i++)
    {
      initbyte[i] = 0;
      for (j = 0; j < 8; j++)
      {
        initbyte[i] = initbyte[i] << 1;
        initbyte[i] = initbyte[i] | init[k];
        k++;
      }
      fprintf (stderr, "%02X", initbyte[i]);
    }
    fprintf (stderr, "\n");
  }
  // lid
  for (i = 0; i < 16; i++)
    {
      dibit = getDibit (opts, state);
      lid[i] = dibit;
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif

  for (i = 0; i < 64; i++)
    {
      dibit = getDibit (opts, state);
      lid[i+16] = dibit;
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif
  //load lid (bits) into lidbyte
  if (opts->payload == 1)
  {
    k = 0;
    fprintf (stderr, "LID ");
    for(i = 0; i < 10; i++)
    {
      lidbyte[i] = 0;
      for (j = 0; j < 8; j++)
      {
        lidbyte[i] = lidbyte[i] << 1;
        lidbyte[i] = lidbyte[i] | lid[k];
        k++;
      }
      fprintf (stderr, "%02X", lidbyte[i]);
    }
    fprintf (stderr, "\n");
  }


  // imbe frames 1,2 first half
  w = pW;
  x = pX;

  for (i = 0; i < 11; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
    }

  for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif
  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif

  // spacer bits
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  fprintf (stderr,"%i", dibit);
#endif
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  fprintf (stderr,"%i", dibit);
  fprintf (stderr," ");
#endif

  // imbe frames 1,2 second half

  for (j = 0; j < 2; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif

  for (i = 0; i < 3; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
    }

  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif

  for (i = 0; i < 7; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
    }

  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);

  // spacer bits
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  fprintf (stderr,"%i", dibit);
#endif
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  fprintf (stderr,"%i", dibit);
  fprintf (stderr," ");
#endif

  for (i = 0; i < 16; i++)
    {
      dibit = getDibit (opts, state); //HERE
      init[i] = dibit; //recycle
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif
if (opts->payload == 1)
{
  k = 0;
  fprintf (stderr, "16 bits? ");
  for(i = 0; i < 2; i++)
  {
    initbyte[i] = 0;
    for (j = 0; j < 8; j++)
    {
      initbyte[i] = initbyte[i] << 1;
      initbyte[i] = initbyte[i] | init[k];
      k++;
    }
    fprintf (stderr, "%02X", initbyte[i]);
  }
  fprintf (stderr, "\n");
}
  // imbe frames 3,4 first half
  w = pW;
  x = pX;
  for (i = 0; i < 11; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      fprintf (stderr,"_");
#endif
    }
  for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr,"_");
#endif
  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  fprintf (stderr," ");
#endif

  // spacer bits
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"%i", dibit);
#endif
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"%i", dibit);
  //fprintf (stderr,"_");
#endif

  // imbe frames 3,4 second half
  for (j = 0; j < 2; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"_");
#endif
  for (i = 0; i < 3; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"_");
#endif
    }

  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"_");
#endif
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  //fprintf (stderr," ");
#endif

  for (i = 0; i < 7; i++)
    {
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr1[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"_");
#endif
      w -= 6;
      x -= 6;
      for (j = 0; j < 6; j++)
        {
          dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
          //fprintf (stderr,"%i", dibit);
#endif
          imbe7100_fr2[*w][*x] = dibit;
          w++;
          x++;
        }
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"_");
#endif
    }

  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"_");
#endif
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
    {
      dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
      //fprintf (stderr,"%i", dibit);
#endif
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
    }
#ifdef PROVOICE_DUMP
  //fprintf (stderr," ");
#endif

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);

  // spacer bits
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"%i", dibit);
#endif
  dibit = getDibit (opts, state);
#ifdef PROVOICE_DUMP
  //fprintf (stderr,"%i", dibit);
  //fprintf (stderr," ");
#endif

  if (opts->errorbars == 1)
    {
      fprintf (stderr,"\n");
    }
}
