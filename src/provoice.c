#include "dsd.h"
#include "provoice_const.h"

// #define PVDEBUG

void processProVoice (dsd_opts * opts, dsd_state * state)
{
  int i, j, dibit;
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

  uint8_t spacer_bit[8]; //check this value to see if its a status thing (like P25 P1)
  memset (spacer_bit, 0, sizeof (spacer_bit));
  uint16_t bf = 0; //the 16-bit value in-between imbe 2 and imbe 3 that is usually 2175

  fprintf (stderr," VOICE");

  //print group and source values if EA trunked
  if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && state->ea_mode == 1)
  {
    fprintf (stderr, "%s", KGRN);
    fprintf (stderr, " Site: %lld Group: %d Source: %d LCN: %d ", 
              state->edacs_site_id, state->lasttg, state->lastsrc, state->edacs_tuned_lcn);
    fprintf (stderr, "%s", KNRM);
  }
  //print afs value if standard/networked trunked
  else if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && state->ea_mode == 0)
  {
    fprintf (stderr, "%s", KGRN);
    fprintf (stderr, " Site: %lld AFS: %d-%d LCN: %d ", 
              state->edacs_site_id, (state->lasttg >> 7) & 0xF, state->lasttg & 0x7F, state->edacs_tuned_lcn);
    fprintf (stderr, "%s", KNRM);
  }

  //load all initial bits before voice into raw_bits array for analysis/handling
  for (i = 0; i < 64+16+64; i++)
    raw_bits[i] = getDibit (opts, state);

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
      raw_bits[144+j+(i*12)] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[150+j+(i*12)] = dibit;
    }

  }

  for (j = 0; j < 6; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+282] = dibit;
  }

  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+288] = dibit;
  }

  // spacer bits
  dibit = getDibit (opts, state);
  spacer_bit[0] = dibit;
  raw_bits[292] = dibit;

  dibit = getDibit (opts, state);
  spacer_bit[1] = dibit;
  raw_bits[293] = dibit;

  // imbe frames 1,2 second half
  for (j = 0; j < 2; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+294] = dibit;
  }

  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[296+j+(i*12)] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[302+j+(i*12)] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+338] = dibit;
  }

  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+343] = dibit;
  }

  for (i = 0; i < 7; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[348+j+(i*12)] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[354+j+(i*12)] = dibit;
    }

  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+438] = dibit;
  }
  w -= 5;
  x -= 5;

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+443] = dibit;
  }

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);

  // spacer bits
  dibit = getDibit (opts, state);
  spacer_bit[2] = dibit;
  raw_bits[448] = dibit;

  dibit = getDibit (opts, state);
  spacer_bit[3] = dibit; 
  raw_bits[449] = dibit;

  //resume raw_bits at +6 current value to round out payload bytes
  //easier to visualize patterns

  for (i = 0; i < 16; i++)
    raw_bits[i+450+6] = getDibit (opts, state); 

  bf = (uint16_t)ConvertBitIntoBytes(&raw_bits[456], 16);

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
      raw_bits[6+466+j+(i*12)] = dibit;
    }

    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[6+472+j+(i*12)] = dibit;
    }
  }

  for (j = 0; j < 6; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+604+6] = dibit;
  }

  w -= 6;
  x -= 6;
  for (j = 0; j < 4; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[i+610+6] = dibit; 
  }

  // spacer bits
  dibit = getDibit (opts, state);
  spacer_bit[4];
  raw_bits[614+2] = dibit;

  dibit = getDibit (opts, state);
  spacer_bit[5];
  raw_bits[615+2] = dibit;

  // imbe frames 3,4 second half
  for (j = 0; j < 2; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+616+6] = dibit;
  }
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[6+618+j+(i*12)] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[6+624+j+(i*12)] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+660+6] = dibit;
  }
  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+665+6] = dibit;
  }

  for (i = 0; i < 7; i++)
  {
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr1[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[6+670+j+(i*12)] = dibit;
    }
    w -= 6;
    x -= 6;
    for (j = 0; j < 6; j++)
    {
      dibit = getDibit (opts, state);
      imbe7100_fr2[*w][*x] = dibit;
      w++;
      x++;
      raw_bits[6+676+j+(i*12)] = dibit;
    }
  }

  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr1[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+760+6] = dibit;
  }

  w -= 5;
  x -= 5;
  for (j = 0; j < 5; j++)
  {
    dibit = getDibit (opts, state);
    imbe7100_fr2[*w][*x] = dibit;
    w++;
    x++;
    raw_bits[j+765+6] = dibit;
  }

  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr1);
  processMbeFrame (opts, state, NULL, NULL, imbe7100_fr2);

  // spacer bits
  dibit = getDibit (opts, state);
  spacer_bit[6] = dibit;
  raw_bits[782] = dibit;

  dibit = getDibit (opts, state);
  spacer_bit[7] = dibit;
  raw_bits[783] = dibit;

#ifdef PVDEBUG

  spacer = (uint8_t)ConvertBitIntoBytes(&spacer_bit[0], 8);
  if (opts->payload == 1)
    fprintf (stderr, "\n SP: %X", spacer);

  //convert raw bits into raw bytes for payload analysis
  for (j = 0; j < 98; j++)
  {
    raw_bytes[j] = (uint8_t)ConvertBitIntoBytes(&raw_bits[(j*8)], 8);
  }

  //payload on all raw bytes for analysis...its a lot of 'em
  if (opts->payload == 1) //find better thingy later or change this
  // if (opcode != 0x3333)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\n pV Payload Dump: \n  ");
    for (j = 0; j < 98; j++)
    {
      fprintf (stderr, "[%02X]", raw_bytes[j]);
      if (j == 17 || j == 35 || j == 53 || j == 71 || j == 89)
      {
        fprintf (stderr, "\n  ");
      }
    }
    fprintf (stderr, "%s", KNRM);
  }

#endif

  //line break at end of frame
  fprintf (stderr,"\n");

}
