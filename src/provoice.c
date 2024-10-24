#include "dsd.h"
#include "provoice_const.h"

// #define PVDEBUG

void processProVoice (dsd_opts * opts, dsd_state * state)
{
  int i, j, dibit;
  uint16_t k = 0;
  char imbe7100_fr1[7][24];
  char imbe7100_fr2[7][24];
  const int *w, *x;

  //raw bits storage for analysis
  uint8_t raw_bits[800];
  memset (raw_bits, 0, sizeof(raw_bits)); 
  //raw bytes storage for analysis
  uint8_t raw_bytes[100];
  memset (raw_bytes, 0, sizeof(raw_bytes));

  unsigned long long int initial = 0; //initial 64-bits before the lid
  uint16_t lid = 0;     //lid value 16-bit
  unsigned long long int secondary = 0; //secondary 64-bits after lid, before voice

  uint16_t bf = 0; //the 16-bit value in-between imbe 2 and imbe 3 that is usually 2175

  fprintf (stderr," VOICE");

  //print group/target and source values if EA trunked
  if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && state->ea_mode == 1)
  {
    fprintf (stderr, "%s", KGRN);
    if (state->lasttg > 100000) {
      // I-Call
      fprintf (stderr, " Site: %lld Target: %d Source: %d LCN: %d ", 
                state->edacs_site_id, state->lasttg - 100000, state->lastsrc, state->edacs_tuned_lcn);
    } else {
      // Group call
      fprintf (stderr, " Site: %lld Group: %d Source: %d LCN: %d ", 
                state->edacs_site_id, state->lasttg, state->lastsrc, state->edacs_tuned_lcn);
    }
    fprintf (stderr, "%s", KNRM);
  }
  //print afs value if standard/networked trunked
  else if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && state->ea_mode == 0)
  {
    fprintf (stderr, "%s", KGRN);
    fprintf (stderr, " Site: %lld AFS: %d-%d LCN: %d ", 
              state->edacs_site_id, (state->lastsrc >> 7) & 0xF, state->lastsrc & 0x7F, state->edacs_tuned_lcn);
    fprintf (stderr, "%s", KNRM);
  }

  //load all initial bits before voice into raw_bits array for analysis/handling
  for (i = 0; i < 64+16+64; i++)
    raw_bits[k++] = getDibit (opts, state);

  //Note: the initial 144-bits seem to be provisioned differently depending on system type
  initial = (unsigned long long int)ConvertBitIntoBytes(&raw_bits[0], 64);
  lid = (uint16_t)ConvertBitIntoBytes(&raw_bits[64], 16);
  secondary = (unsigned long long int)ConvertBitIntoBytes(&raw_bits[80], 64);
  if (opts->payload == 1)
  {
    fprintf (stderr, "\n N64: %016llX", initial);
    fprintf (stderr, "\n LID: %04X", lid);
    fprintf (stderr, " %016llX", secondary);
  }

  // imbe frames 1,2 first half
  w = pW;
  x = pX;

  for (i = 0; i < 11; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }

  }

  for (j = 0; j < 6; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  // spacer bits
  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  // imbe frames 1,2 second half
  for (j = 0; j < 2; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  for (i = 0; i < 7; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }

  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }
  w -= 5;
  x -= 5;

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  if (opts->floating_point == 0)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 1)
    playSynthesizedVoiceFM(opts, state);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);
  if (opts->floating_point == 0)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 1)
    playSynthesizedVoiceFM(opts, state);

  // spacer bits
  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  for (i = 0; i < 16; i++)
    raw_bits[k++] = getDibit (opts, state); 

  bf = (uint16_t)ConvertBitIntoBytes(&raw_bits[54*8], 16);

  if (opts->payload == 1)
    fprintf (stderr, "\n BF: %04X ", bf);

  // imbe frames 3,4 first half
  w = pW;
  x = pX;
  for (i = 0; i < 11; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
  }

  for (j = 0; j < 6; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit; 
  }

  // spacer bits
  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  // imbe frames 3,4 second half
  for (j = 0; j < 2; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  for (i = 0; i < 7; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[k++] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[k++] = dibit;
  }

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  if (opts->floating_point == 0)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 1)
    playSynthesizedVoiceFM(opts, state);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);
  if (opts->floating_point == 0)
    playSynthesizedVoiceMS(opts, state);
  if (opts->floating_point == 1)
    playSynthesizedVoiceFM(opts, state);

  // spacer bits
  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

  dibit = getDibit (opts, state);
  raw_bits[k++] = dibit;

#ifdef PVDEBUG

  //payload on all raw bytes for analysis
  if (opts->payload == 1)
  // if (opcode != 0x3333) //EA Voice
  {

    fprintf (stderr, "\n pV Payload Dump: \n  ");
    for (i = 0; i < k/8; i++)
    {
      
      // if ( (i != 0) && ((i%26) == 0) )
      //   fprintf (stderr, "\n  ");

      uint16_t top = (uint16_t)ConvertBitIntoBytes(raw_bits+(i*8), 16);
      if (top == 0x0EBF && i != 0)
        fprintf (stderr, "\n  ");

      raw_bytes[i] = (uint8_t)ConvertBitIntoBytes(raw_bits+(i*8), 8);
      fprintf (stderr, "%02X", raw_bytes[i]);
    }
  }

#endif

  //line break at end of frame
  fprintf (stderr,"\n");

}
