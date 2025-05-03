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
  fprintf(stderr, "\n IMBE ");
  uint8_t imbe[88];
  for (int i = 0; i < 11; i++)
  {
    imbe[i] =  convert_bits_into_output((uint8_t *)imbe_d+(i*8), 8);
    fprintf(stderr, "%02X", imbe[i]);
  }


  fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
  UNUSED(opts);
}

void PrintAMBEData (dsd_opts * opts, dsd_state * state, char *ambe_d)
{

  //cast as unsigned long long int and not uint64_t
  //to avoid the %lx vs %llx warning on 32 or 64 bit
  unsigned long long int ambe = 0;

  //preceeding line break, if required
  if (opts->dmr_stereo == 0 && opts->dmr_mono == 0)
    fprintf (stderr, "\n");

  ambe = convert_bits_into_output ((uint8_t *)ambe_d, 49);
  ambe = ambe << 7; //shift to final position

  fprintf(stderr, " AMBE %014llX", ambe);

  if (state->currentslot == 0)
    fprintf(stderr, " err = [%X] [%X] ", state->errs, state->errs2);
  else fprintf(stderr, " err = [%X] [%X] ", state->errsR, state->errs2R);

  //trailing line break, if required
  if (opts->dmr_stereo == 1 || opts->dmr_mono == 1)
    fprintf (stderr, "\n");

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
  char * timestr; //add timestr here, so we can assign it and also free it to prevent memory leak
  char * datestr;

  timestr = getTime();
  datestr = getDate();

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

  sprintf (opts->mbe_out_file, "%s %s S1%s", datestr, timestr, ext);

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
  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }
  if (datestr != NULL)
  {
    free (datestr);
    datestr = NULL;
  }
}

void openMbeOutFileR (dsd_opts * opts, dsd_state * state)
{

  int i, j;
  char ext[5];
  char * timestr; //add timestr here, so we can assign it and also free it to prevent memory leak
  char * datestr;

  timestr = getTime();
  datestr = getDate();

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

  sprintf (opts->mbe_out_fileR, "%s %s S2%s", datestr, timestr, ext);

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
  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }
  if (datestr != NULL)
  {
    free (datestr);
    datestr = NULL;
  }
}

//temp filename should not have the .wav extension, will be renamed with one after event is closed
SNDFILE * open_wav_file (char * dir, char * temp_filename, uint16_t sample_rate, uint8_t ext)
{
  uint16_t random_number = rand();
  char * datestr = getDate();
  char * timestr = getTime();

  if (ext == 0)
    sprintf (temp_filename, "%s/TEMP_%s_%s_%04X", dir, datestr, timestr, random_number);
  else sprintf (temp_filename, "%s/TEMP_%s_%s_%04X.wav", dir, datestr, timestr, random_number);

  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }
  if (datestr != NULL)
  {
    free (datestr);
    datestr = NULL;
  }

  SNDFILE * wav;
  SF_INFO info;
  info.samplerate = sample_rate;
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  wav = sf_open (temp_filename, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (wav == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", temp_filename);
    return NULL;
  }

  return wav;
}

SNDFILE * close_wav_file(SNDFILE * wav_file)
{
  sf_close(wav_file);
  wav_file = NULL;
  return wav_file;
}

SNDFILE * close_and_rename_wav_file(SNDFILE * wav_file, char * wav_out_filename, char * dir, Event_History_I * event_struct)
{
  sf_close(wav_file);

  char * datestr = getDate();
  char * timestr = getTime();
  uint16_t random_number = rand();

  uint32_t source_id = event_struct->Event_History_Items[0].source_id;
  uint32_t target_id = event_struct->Event_History_Items[0].target_id;
  int8_t gi = event_struct->Event_History_Items[0].gi;

  char sys_str[200]; memset (sys_str, 0, sizeof(sys_str));
  char src_str[200]; memset (src_str, 0, sizeof(src_str));
  char tgt_str[200]; memset (tgt_str, 0, sizeof(tgt_str));
  char gi_str[10];   memset (gi_str, 0, sizeof(gi_str));

  sprintf (sys_str, "%s", event_struct->Event_History_Items[0].sysid_string);
  sprintf (src_str, "%s", event_struct->Event_History_Items[0].src_str);
  sprintf (tgt_str, "%s", event_struct->Event_History_Items[0].tgt_str);

  sprintf (gi_str, "%s", "");
  if (gi == 0) sprintf (gi_str, "%s", "GROUP");
  else if (gi == 1) sprintf (gi_str, "%s", "PRIVATE");

  uint8_t is_string = 0;
  char emp_str[200]; memset (emp_str, 0, sizeof(emp_str));
  sprintf (emp_str, "%s", "BUMBLEBEETUNA");
  if (strncmp(emp_str, src_str, 13) != 0)
    is_string = 1;

  //rename and give extension .wav after closing
  char new_filename[2000];
  memset (new_filename, 0, sizeof(new_filename));

  //check for String based TGT and SRC values (M17, YSF, DSTAR)
  if (is_string == 1)
    sprintf (new_filename, "%s/%s_%s_%05d_%s_%s_TGT_%s_SRC_%s.wav", dir, datestr, timestr, random_number, sys_str, gi_str, tgt_str, src_str);
  else //is a numerical value
    sprintf (new_filename, "%s/%s_%s_%05d_%s_%s_TGT_%d_SRC_%d.wav", dir, datestr, timestr, random_number, sys_str, gi_str, target_id, source_id);

  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }
  if (datestr != NULL)
  {
    free (datestr);
    datestr = NULL;
  }

  rename (wav_out_filename, new_filename);

  //WIP: Open File, seek, and if 44 bytes, delete it (empty wav file)
  //TODO: May need to move the deletion ahead of renaming, and do a NULL check,
  //some software may attempt to injest an empty .wav file first, but this should
  //occur so quickly, the watchdog on rdio or similar shouldn't even realize it
  //may also consider checking for a larger size, something that's more than a blip
  //sometimes if encrypted and no key provided, if signal is marginal, short garbled audio
  //could be written to a wav file, so may look into a value >= xx kb in size minimum
  FILE *file = fopen(new_filename, "r");
  if (file != NULL)
  {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET); // Rewind to beginning
    fclose(file);

    //debug
    // fprintf (stderr, " Closed Wav File %s; Size: %d; \n", new_filename, size);

    if (size == 44)
      remove (new_filename);
  }

  wav_file = NULL;
  return wav_file;
}

SNDFILE * close_and_delete_wav_file(SNDFILE * wav_file, char * wav_out_filename)
{
  sf_close(wav_file);
  wav_file = NULL;
  remove (wav_out_filename);
  return wav_file;
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

//input bit array, return output as up to a 64-bit value
uint64_t convert_bits_into_output(uint8_t * input, int len)
{
  int i;
  uint64_t output = 0;
  for(i = 0; i < len; i++)
  {
    output <<= 1;
    output |= (uint64_t)(input[i] & 1);
  }
  return output;
}

void pack_bit_array_into_byte_array (uint8_t * input, uint8_t * output, int len)
{
  int i;
  for (i = 0; i < len; i++)
    output[i] = (uint8_t)convert_bits_into_output(&input[i*8], 8);
}

//take len amount of bits and pack into x amount of bytes (asymmetrical)
void pack_bit_array_into_byte_array_asym (uint8_t * input, uint8_t * output, int len)
{
  int i = 0; int k = len % 8;
  for (i = 0; i < len; i++)
  {
    output[i/8] <<= 1;
    output[i/8] |= input[i];
  }
  //if any leftover bits that don't flush the last byte fully packed, shift them over left
  if (k)
    output[i/8] <<= 8-k;
}

//take len amount of bytes and unpack back into a bit array
void unpack_byte_array_into_bit_array (uint8_t * input, uint8_t * output, int len)
{
  int i = 0, k = 0;
  for (i = 0; i < len; i++)
  {
    output[k++] = (input[i] >> 7) & 1;
    output[k++] = (input[i] >> 6) & 1;
    output[k++] = (input[i] >> 5) & 1;
    output[k++] = (input[i] >> 4) & 1;
    output[k++] = (input[i] >> 3) & 1;
    output[k++] = (input[i] >> 2) & 1;
    output[k++] = (input[i] >> 1) & 1;
    output[k++] = (input[i] >> 0) & 1;
  }
}

//take len amount of bits and pack into x amount of bytes (asymmetrical)
void pack_ambe (char * input, uint8_t * output, int len)
{
  int i = 0; int k = len % 8;
  for (i = 0; i < len; i++)
  {
    output[i/8] <<= 1;
    output[i/8] |= (uint8_t)input[i];
  }
  //if any leftover bits that don't flush the last byte fully packed, shift them over left
  if (k)
    output[i/8] <<= 8-k;
}

//unpack byte array with ambe data into a 49-bit bitwise array
void unpack_ambe (uint8_t * input, char * ambe)
{
  int i = 0, k = 0;
  for (i = 0; i < 6; i++)
  {
    ambe[k++] = (input[i] >> 7) & 1;
    ambe[k++] = (input[i] >> 6) & 1;
    ambe[k++] = (input[i] >> 5) & 1;
    ambe[k++] = (input[i] >> 4) & 1;
    ambe[k++] = (input[i] >> 3) & 1;
    ambe[k++] = (input[i] >> 2) & 1;
    ambe[k++] = (input[i] >> 1) & 1;
    ambe[k++] = (input[i] >> 0) & 1;
  }
  ambe[48] = input[6] >> 7;
}

//convert a user string into a uint8_t array
uint16_t parse_raw_user_string (char * input, uint8_t * output)
{
  //since we want this as octets, get strlen value, then divide by two
  uint16_t len = strlen((const char*)input);

  uint8_t shift = 0;

  //if zero is returned, just do two
  // if (len == 0) len = 2;

  //if zero, return as 0 len string
  if (len == 0) return 0;

  //if odd number, then user didn't pass complete octets,
  //add one to len value and set the shift flag to left shift
  if (len&1)
  {
    shift = 1;
    len++;
  }

  //divide by two to get octet len
  len /= 2;

  char octet_char[3];
  octet_char[2] = 0;
  uint16_t k = 0;
  uint16_t i = 0;

  for (i = 0; i < len; i++)
  {
    strncpy (octet_char, input+k, 2);
    octet_char[2] = 0;
    sscanf (octet_char, "%hhX", &output[i]);

    k += 2;
  }

  //if we had an odd input value, then left shift the last octet 4 to make it flush
  if (shift) output[len-1] <<= 4;

  return len;
}