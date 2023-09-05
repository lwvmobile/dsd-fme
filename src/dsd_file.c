/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "dsd.h"

time_t nowF;
char * getTimeF(void) //get pretty hh:mm:ss timestamp
{
  time_t t = time(NULL);

  char * curr;
  char * stamp = asctime(localtime( & t));

  curr = strtok(stamp, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");

  return curr;
}

char * getDateF(void) {
  #ifdef AERO_BUILD
  char datename[80];
  #else
  char datename[99];
  #endif
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y-%m-%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

void saveImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  for (i = 0; i < 11; i++)
  {
    b = 0;

    for (j = 0; j < 8; j++)
      {
        b = b << 1;
        b = b + imbe_d[k];
        k++;
      }
    fputc (b, opts->mbe_out_f);
  }
  fflush (opts->mbe_out_f);
}

void saveAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  for (i = 0; i < 6; i++) 
  {
    b = 0;
    for (j = 0; j < 8; j++)
    {
      b = b << 1;
      b = b + ambe_d[k];
      k++;
    }
    fputc (b, opts->mbe_out_f);
  }
  b = ambe_d[48];
  fputc (b, opts->mbe_out_f);
  fflush (opts->mbe_out_f);
}

void saveAmbe2450DataR (dsd_opts * opts, dsd_state * state, char *ambe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2R;
  fputc (err, opts->mbe_out_fR);

  k = 0;
  for (i = 0; i < 6; i++) 
  {
    b = 0;
    for (j = 0; j < 8; j++)
    {
      b = b << 1;
      b = b + ambe_d[k];
      k++;
    }
    fputc (b, opts->mbe_out_fR);
  }
  b = ambe_d[48];
  fputc (b, opts->mbe_out_fR);
  fflush (opts->mbe_out_fR);
}

void PrintIMBEData (dsd_opts * opts, dsd_state * state, char *imbe_d) //for P25P1 and ProVoice
{
  int i, j, k;
  unsigned char b;
  unsigned char err;
  UNUSED(err);

  err = (unsigned char) state->errs2;
  k = 0;
  if (opts->payload == 1) //print IMBE info
  {
    fprintf(stderr, "\n IMBE ");
  }
  for (i = 0; i < 11; i++)
  {
    b = 0;
    for (j = 0; j < 8; j++)
    {
      b = b << 1;
      b = b + imbe_d[k];
      k++;
    }
    if (opts->payload == 1)
    {
      fprintf (stderr, "%02X", b);
    }
  }

  if (opts->payload == 1)
  {
    fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
  }

  //fprintf (stderr, "\n");
}

void PrintAMBEData (dsd_opts * opts, dsd_state * state, char *ambe_d) 
{
  int i, j, k;
  unsigned char b;
  unsigned char err;
  UNUSED(err);

  err = (unsigned char) state->errs2;
  k = 0;
  if (opts->dmr_stereo == 0 && opts->dmr_mono == 0)
  {
    fprintf (stderr, "\n");
  }
  if (opts->payload == 1) //print AMBE info from DMR Stereo method
  {
    fprintf(stderr, " AMBE ");
  }

  for (i = 0; i < 7; i++) 
    {
      b = 0;
      for (j = 0; j < 8; j++)
        {
          b = b << 1;
          b = b + ambe_d[k];
          k++;
        }
        if (opts->payload == 1 && i < 6) 
        {
          fprintf (stderr, "%02X", b);
        }
        if (opts->payload == 1 && i == 6) 
        {
          fprintf (stderr, "%02X", b & 0x80); 
        }
    }
    if (state->currentslot == 0)
    {
      fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
    }
    else fprintf(stderr, " err = [%X] [%X] ", state->errsR, state->errs2R);
  b = ambe_d[48];
  if (opts->dmr_stereo == 1 || opts->dmr_mono == 1) //need to fix the printouts again
  {
    fprintf (stderr, "\n");
  }

}

int
readImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d)
{

  int i, j, k;
  unsigned char b, x;

  state->errs2 = fgetc (opts->mbe_in_f);
  state->errs = state->errs2;


  k = 0;
  if (opts->payload == 1) 
  {
    fprintf(stderr, "\n");
  }
  for (i = 0; i < 11; i++)
    {
      b = fgetc (opts->mbe_in_f);
      if (feof (opts->mbe_in_f))
        {
          return (1);
        }
      for (j = 0; j < 8; j++)
        {
          imbe_d[k] = (b & 128) >> 7;

          x = x << 1;
          x |= ((b & 0x80) >> 7);

          b = b << 1;
          b = b & 255;
          k++;
        }
        
        if (opts->payload == 1) 
        {
          fprintf (stderr, "%02X", x);
        }
        
    }
    if (opts->payload == 1)
    {
      fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2); //not sure that errs here are legit values
    }
  return (0);
}

int
readAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d)
{

  int i, j, k;
  unsigned char b, x;

  state->errs2 = fgetc (opts->mbe_in_f);
  state->errs = state->errs2;

  k = 0;
  if (opts->payload == 1) 
  {
    fprintf(stderr, "\n");
  }

  for (i = 0; i < 6; i++) //breaks backwards compatablilty with 6 files
    {
      b = fgetc (opts->mbe_in_f);
      if (feof (opts->mbe_in_f))
        {
          return (1);
        }
      for (j = 0; j < 8; j++)
        {
          ambe_d[k] = (b & 128) >> 7;

          x = x << 1;
          x |= ((b & 0x80) >> 7);

          b = b << 1;
          b = b & 255;
          k++;
        }
        if (opts->payload == 1 && i < 6) 
        {
          fprintf (stderr, "%02X", x);
        }
        if (opts->payload == 1 && i == 6) 
        {
          fprintf (stderr, "%02X", x & 0x80); 
        }
    }
    if (opts->payload == 1)
    {
      fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
    }
  b = fgetc (opts->mbe_in_f);
  ambe_d[48] = (b & 1);

  return (0);
}

void
openMbeInFile (dsd_opts * opts, dsd_state * state)
{

  char cookie[5];

  opts->mbe_in_f = fopen (opts->mbe_in_file, "ro");
  if (opts->mbe_in_f == NULL)
    {
      fprintf (stderr,"Error: could not open %s\n", opts->mbe_in_file);
    }

  // read cookie
  cookie[0] = fgetc (opts->mbe_in_f);
  cookie[1] = fgetc (opts->mbe_in_f);
  cookie[2] = fgetc (opts->mbe_in_f);
  cookie[3] = fgetc (opts->mbe_in_f);
  cookie[4] = 0;
  //ambe+2
  if (strstr (cookie, ".amb") != NULL)
  {
    state->mbe_file_type = 1;
  }
  //p1 and pv
  else if (strstr (cookie, ".imb") != NULL)
  {
    state->mbe_file_type = 0;
  }
  //d-star ambe
  else if (strstr (cookie, ".dmb") != NULL)
  {
    state->mbe_file_type = 2;
  }
  else
    {
      state->mbe_file_type = -1;
      fprintf (stderr,"Error - unrecognized file type\n");
    }

}

//slot 1
void closeMbeOutFile (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  if (opts->mbe_out == 1)
  {
    if (opts->mbe_out_f != NULL)
    {
      fflush (opts->mbe_out_f);
      fclose (opts->mbe_out_f);
      opts->mbe_out_f = NULL;
      opts->mbe_out = 0;
      fprintf (stderr, "\nClosing MBE out file 1.\n");
    }

  }
}

//slot 2
void closeMbeOutFileR (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  if (opts->mbe_outR == 1)
  {
    if (opts->mbe_out_fR != NULL)
    {
      fflush (opts->mbe_out_fR);
      fclose (opts->mbe_out_fR);
      opts->mbe_out_fR = NULL;
      opts->mbe_outR = 0;
      fprintf (stderr, "\nClosing MBE out file 2.\n");
    }

  }
}

void openMbeOutFile (dsd_opts * opts, dsd_state * state)
{

  int i, j;
  char ext[5];

  //phase 1 and provoice
  if ( (state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15) )
  {
    sprintf (ext, ".imb");
  }
  //d-star
  else if ( (state->synctype == 6) || (state->synctype == 7) || (state->synctype == 18) || (state->synctype == 19) )
  {
    sprintf (ext, ".dmb"); //new dstar file extension to make it read in and process properly
  }
  //dmr, nxdn, phase 2, x2-tdma
  else sprintf (ext, ".amb"); 

  //reset talkgroup id buffer
  for (i = 0; i < 12; i++)
  {
    for (j = 0; j < 25; j++)
    {
      state->tg[j][i] = 0;
    }
  }

  state->tgcount = 0;

  sprintf (opts->mbe_out_file, "%s %s S1%s", getDateF(), getTimeF(), ext);

  sprintf (opts->mbe_out_path, "%s%s", opts->mbe_out_dir, opts->mbe_out_file);

  opts->mbe_out_f = fopen (opts->mbe_out_path, "w");
  if (opts->mbe_out_f == NULL)
  {
    fprintf (stderr,"\nError, couldn't open %s for slot 1\n", opts->mbe_out_path);
  }
  else opts->mbe_out = 1;

  //
  fprintf (opts->mbe_out_f, "%s", ext);

  fflush (opts->mbe_out_f);
}

void openMbeOutFileR (dsd_opts * opts, dsd_state * state)
{

  int i, j;
  char ext[5];

  //phase 1 and provoice
  if ( (state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15) )
  {
    sprintf (ext, ".imb");
  }
  //d-star
  else if ( (state->synctype == 6) || (state->synctype == 7) || (state->synctype == 18) || (state->synctype == 19) )
  {
    sprintf (ext, ".dmb"); //new dstar file extension to make it read in and process properly
  }
  //dmr, nxdn, phase 2, x2-tdma
  else sprintf (ext, ".amb"); 

  //reset talkgroup id buffer
  for (i = 0; i < 12; i++)
  {
    for (j = 0; j < 25; j++)
    {
      state->tg[j][i] = 0;
    }
  }

  state->tgcount = 0;

  sprintf (opts->mbe_out_fileR, "%s %s S2%s", getDateF(), getTimeF(), ext);

  sprintf (opts->mbe_out_path, "%s%s", opts->mbe_out_dir, opts->mbe_out_fileR);

  opts->mbe_out_fR = fopen (opts->mbe_out_path, "w");
  if (opts->mbe_out_fR == NULL)
  {
    fprintf (stderr,"\nError, couldn't open %s for slot 2\n", opts->mbe_out_path);
  }
  else opts->mbe_outR = 1;

  //
  fprintf (opts->mbe_out_fR, "%s", ext);

  fflush (opts->mbe_out_fR);
}

void openWavOutFile (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }
}

void openWavOutFileL (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  SF_INFO info;
  info.samplerate = 8000; 
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }
}

void openWavOutFileR (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_fR = sf_open (opts->wav_out_fileR, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_fileR);
    return;
  }
}

void openWavOutFileRaw (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  SF_INFO info;
  info.samplerate = 48000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_raw = sf_open (opts->wav_out_file_raw, SFM_WRITE, &info);
  if (opts->wav_out_raw == NULL)
  {
    fprintf (stderr,"Error - could not open raw wav output file %s\n", opts->wav_out_file_raw);
    return;
  }
}

void closeWavOutFile (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  sf_close(opts->wav_out_f);
}

void closeWavOutFileL (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  sf_close(opts->wav_out_f);
}

void closeWavOutFileR (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  sf_close(opts->wav_out_fR);
}

void closeWavOutFileRaw (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  sf_close(opts->wav_out_raw);
}

void openSymbolOutFile (dsd_opts * opts, dsd_state * state)
{
  closeSymbolOutFile(opts, state);
  opts->symbol_out_f = fopen (opts->symbol_out_file, "w");
}

void closeSymbolOutFile (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  if (opts->symbol_out_f)
  {
    fclose(opts->symbol_out_f);
    opts->symbol_out_f = NULL;
  }
}
