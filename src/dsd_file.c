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
  char datename[120]; //bug in 32-bit Ubuntu when using date in filename, date is garbage text
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y-%m-%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

void
saveImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  if (opts->payload == 1) //make opt variable later on to toggle this
  {
    //fprintf(stderr, "\n IMBE ");
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
        if (opts->payload == 1) //make opt variable later on to toggle this
        {
          //fprintf (stderr, "%02X", b);
        }
      fputc (b, opts->mbe_out_f);
    }
    if (opts->payload == 1)
    {
      //fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
    }
  fflush (opts->mbe_out_f);
}

void
saveAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  if (opts->payload == 1) //make opt variable later on to toggle this
  {
    //fprintf(stderr, "\n AMBE ");
  }
  //for (i = 0; i < 6; i++)
  for (i = 0; i < 6; i++) //using 7 seems to break older amb files where it was 6, need to test 7 on 7 some more
    {
      b = 0;
      for (j = 0; j < 8; j++)
        {
          b = b << 1;
          b = b + ambe_d[k];
          k++;
        }
        if (opts->payload == 1 && i < 6) //make opt variable later on to toggle this
        {
          //fprintf (stderr, "%02X", b);
        }
        if (opts->payload == 1 && i == 6) //7th octet should only contain 1 bit? value will be either 0x00 or 0x80?
        {
          //fprintf (stderr, "%02X", b & 0x80); //7th octet should only contain 1 bit?
        }

      fputc (b, opts->mbe_out_f);
    }
    if (opts->payload == 1)
    {
      //fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
    }
  b = ambe_d[48];
  fputc (b, opts->mbe_out_f);
  fflush (opts->mbe_out_f);
}

void
PrintIMBEData (dsd_opts * opts, dsd_state * state, char *imbe_d) //for P25P1 and ProVoice
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

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

void
PrintAMBEData (dsd_opts * opts, dsd_state * state, char *ambe_d) //For DMR Stereo, may use this for NXDN too
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  k = 0;
  if (opts->dmr_stereo == 0)
  {
    fprintf (stderr, "\n");
  }
  if (opts->payload == 1) //print AMBE info from DMR Stereo method
  {
    fprintf(stderr, " AMBE ");
  }

  for (i = 0; i < 7; i++) //using 7 seems to break older amb files where it was 6, need to test 7 on 7 some more
    {
      b = 0;
      for (j = 0; j < 8; j++)
        {
          b = b << 1;
          b = b + ambe_d[k];
          k++;
        }
        if (opts->payload == 1 && i < 6) //make opt variable later on to toggle this
        {
          fprintf (stderr, "%02X", b);
        }
        if (opts->payload == 1 && i == 6) //7th octet should only contain 1 bit, value will be either 0x00 or 0x80?
        {
          fprintf (stderr, "%02X", b & 0x80); //7th octet should only contain 1 bit
        }
    }
    if (state->currentslot == 0)
    {
      fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
    }
    else fprintf(stderr, " err = [%X] [%X] ", state->errsR, state->errs2R);
  b = ambe_d[48];
  if (opts->dmr_stereo == 1) //need to fix the printouts again
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
  if (opts->payload == 1) //make opt variable later on to toggle this
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
        //new printout on reading MBE files back in
        if (opts->payload == 1) //make opt variable later on to toggle this
        {
          fprintf (stderr, "%02X", x);
        }
        //
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
  if (opts->payload == 1) //make opt variable later on to toggle this
  {
    fprintf(stderr, "\n");
  }
  //for (i = 0; i < 6; i++)
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
        if (opts->payload == 1 && i < 6) //make opt variable later on to toggle this
        {
          fprintf (stderr, "%02X", x);
        }
        if (opts->payload == 1 && i == 6) //7th octet should only contain 1 bit? value will be either 0x00 or 0x80?
        {
          fprintf (stderr, "%02X", x & 0x80); //7th octet should only contain 1 bit?
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
  if (strstr (cookie, ".amb") != NULL)
    {
      state->mbe_file_type = 1;
    }
  else if (strstr (cookie, ".imb") != NULL)
    {
      state->mbe_file_type = 0;
    }
  else
    {
      state->mbe_file_type = -1;
      fprintf (stderr,"Error - unrecognized file type\n");
    }

}

//temporarily disabled until the compiler warnings can be fixed, use symbol capture  or per call instead!
//probably the easiest thing is to just have this run fclose, and have name assignment on open instead.
// void
// closeMbeOutFile (dsd_opts * opts, dsd_state * state)
// {
//
//   char shell[255], newfilename[64], ext[5], datestr[32], new_path[1024];
//   char tgid[17];
//   int sum, i, j;
//   int talkgroup;
//   struct tm timep;
//   int result;
//
//   if (opts->mbe_out_f != NULL)
//     {
//       if ((state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15))
//         {
//           sprintf (ext, ".imb");
//           //strptime (opts->mbe_out_file, "%s.imb", &timep);
//         }
//       else
//         {
//           sprintf (ext, ".amb");
//           //strptime (opts->mbe_out_file, "%s.amb", &timep);
//         }
//
//       if (state->tgcount > 0)
//         {
//           for (i = 0; i < 16; i++)
//             {
//               sum = 0;
//               for (j = 0; j < state->tgcount; j++)
//                 {
//                   sum = sum + state->tg[j][i] - 48;
//                 }
//               tgid[i] = (char) (((float) sum / (float) state->tgcount) + 48.5);
//             }
//           tgid[16] = 0;
//           talkgroup = (int) strtol (tgid, NULL, 2);
//         }
//       else
//         {
//           talkgroup = 0;
//         }
//
//       fflush (opts->mbe_out_f);
//       fclose (opts->mbe_out_f);
//       opts->mbe_out_f = NULL;
//       strftime (datestr, 31, "%Y-%m-%d-%H%M%S", &timep);
//
//       sprintf (newfilename, "%s%s", datestr, ext); //this one
//
//       if (state->lastsynctype == 0 || state->lastsynctype == 1)
//       {
//         sprintf (newfilename, "%s-nac%X-tg%i%s", datestr, state->nac, talkgroup, ext);
//       }
//
//       //sprintf (new_path, "%s%s", opts->mbe_out_dir, newfilename);
// #ifdef _WIN32
//       //sprintf (shell, "move %s %s", opts->mbe_out_path, new_path);
// #else
//       //sprintf (shell, "mv %s %s", opts->mbe_out_path, new_path);
// #endif
//       //result = system (shell);
//
//       state->tgcount = 0;
//       for (i = 0; i < 25; i++)
//         {
//           for (j = 0; j < 16; j++)
//             {
//               state->tg[i][j] = 48;
//             }
//         }
//     }
// }

//much simpler version
void closeMbeOutFile (dsd_opts * opts, dsd_state * state)
{
  if (opts->mbe_out == 1)
  {
    if (opts->mbe_out_f != NULL)
    {
      fflush (opts->mbe_out_f);
      fclose (opts->mbe_out_f);
      opts->mbe_out_f = NULL;
      opts->mbe_out = 0;
      fprintf (stderr, "\nClosing MBE out file.");
    }

  }

}

void
openMbeOutFile (dsd_opts * opts, dsd_state * state)
{

  //struct timeval tv;
  int i, j;
  char ext[5];

  if ((state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15))
    {
      sprintf (ext, ".imb");
    }
  else
    {
      sprintf (ext, ".amb");
    }

  //  reset talkgroup id buffer
  for (i = 0; i < 12; i++)
    {
      for (j = 0; j < 25; j++)
        {
          state->tg[j][i] = 0;
        }
    }

  state->tgcount = 0;

  //gettimeofday (&tv, NULL);
  //rewrite below line to take getTime function instead?
  // sprintf (opts->mbe_out_file, "%i%s", (int) tv.tv_sec, ext);

  sprintf (opts->mbe_out_file, "%s %s%s", getDateF(), getTimeF(), ext);

  sprintf (opts->mbe_out_path, "%s%s", opts->mbe_out_dir, opts->mbe_out_file);

  opts->mbe_out_f = fopen (opts->mbe_out_path, "w");
  if (opts->mbe_out_f == NULL)
  {
    fprintf (stderr,"Error, couldn't open %s\n", opts->mbe_out_path);
  }
  else opts->mbe_out = 1;

  //
  fprintf (opts->mbe_out_f, "%s", ext);

  fflush (opts->mbe_out_f);
}

void
openWavOutFile (dsd_opts * opts, dsd_state * state)
{

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  //opts->wav_out_f = sf_open (opts->wav_out_file, SFM_WRITE, &info);
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }

//  state->wav_out_bytes = 0;

}
void openWavOutFileL (dsd_opts * opts, dsd_state * state)
{

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  //opts->wav_out_f = sf_open (opts->wav_out_file, SFM_WRITE, &info);
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }

//  state->wav_out_bytes = 0;

}

void openWavOutFileR (dsd_opts * opts, dsd_state * state)
{

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  //opts->wav_out_f = sf_open (opts->wav_out_file, SFM_WRITE, &info);
  opts->wav_out_fR = sf_open (opts->wav_out_fileR, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_fileR);
    return;
  }

//  state->wav_out_bytes = 0;

}

void openWavOutFileRaw (dsd_opts * opts, dsd_state * state)
{

  SF_INFO info;
  info.samplerate = 48000; //8000
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  //opts->wav_out_f = sf_open (opts->wav_out_file, SFM_WRITE, &info);
  //opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file
  opts->wav_out_raw = sf_open (opts->wav_out_file_raw, SFM_RDWR, &info);
  if (opts->wav_out_raw == NULL)
  {
    fprintf (stderr,"Error - could not open raw wav output file %s\n", opts->wav_out_file_raw);
    return;
  }

//  state->wav_out_bytes = 0;

}

void closeWavOutFile (dsd_opts * opts, dsd_state * state)
{
  sf_close(opts->wav_out_f);
}

void closeWavOutFileL (dsd_opts * opts, dsd_state * state)
{
  sf_close(opts->wav_out_f);
}

void closeWavOutFileR (dsd_opts * opts, dsd_state * state)
{
  sf_close(opts->wav_out_fR);
}

void closeWavOutFileRaw (dsd_opts * opts, dsd_state * state)
{
  sf_close(opts->wav_out_raw);
}

void openSymbolOutFile (dsd_opts * opts, dsd_state * state)
{
  //Do something
  opts->symbol_out_f = fopen (opts->symbol_out_file, "w");
}

void writeSymbolOutFile (dsd_opts * opts, dsd_state * state)
{
  //Do something

}

void closeSymbolOutFile (dsd_opts * opts, dsd_state * state)
{
  //Do something
  if (opts->symbol_out == 1)
  {
    if (opts->symbol_out_file != NULL) //check first, or issuing a second fclose will crash the SOFTWARE
    {
      fclose(opts->symbol_out_f); //free(): double free detected in tcache 2 (this is a new one) happens when closing more than once
      //noCarrier(opts, state); //free(): double free detected in tcache 2 (this is a new one)
    }

    opts->symbol_out = 0; //set flag to 1
    //sprintf (opts->symbol_out_file, "fme-capture.bin"); //use fme for the symbol bin ext
    //openSymbolOutFile (opts, state);

  }

}
