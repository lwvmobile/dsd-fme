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
#include "p25p1_const.h" //for imbe fr (7200)
#include "dmr_const.h" //for ambe+2 fr
#include "bp.h" //for bp key table

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
    fprintf(stderr, "\n IMBE ");
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
    fprintf(stderr, "\n AMBE ");
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
    fprintf (stderr,"Error: could not open %s\n", opts->mbe_in_file);

  //this will check the last 4 characters of the opts->mbe_in_file string
  char ext[5]; memset(ext, 0, sizeof(ext));
  uint16_t str_len = strlen((const char*)opts->mbe_in_file);
  uint16_t ext_ptr = str_len - 4;
  strncpy (ext, opts->mbe_in_file+ext_ptr, 4);

  //debug
  // fprintf (stderr, "EXT: %s;", ext);


  // read cookie
  cookie[0] = fgetc (opts->mbe_in_f);
  cookie[1] = fgetc (opts->mbe_in_f);
  cookie[2] = fgetc (opts->mbe_in_f);
  cookie[3] = fgetc (opts->mbe_in_f);
  cookie[4] = 0;

  //ambe+2
  if (strstr (cookie, ".amb") != NULL)
    state->mbe_file_type = 1;
  //p1 and pv
  else if (strstr (cookie, ".imb") != NULL)
    state->mbe_file_type = 0;
  //d-star ambe
  else if (strstr (cookie, ".dmb") != NULL)
    state->mbe_file_type = 2;
  //sdrtrunk formated mbe json file
  else if (strncmp (".mbe", ext, 4) == 0)
    state->mbe_file_type = 3;
  else
  {
    state->mbe_file_type = -1;
    fprintf (stderr,"Error - unrecognized file type\n");

    //try SDRTrunk JSON format as last resort
    state->mbe_file_type = 3;
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

  //random element of filename, so two files won't overwrite one another
  uint16_t random_number = rand() & 0xFFFF;

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

  sprintf (opts->mbe_out_file, "%s_%s_%04X_S1%s", datestr, timestr, random_number, ext);

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

  //random element of filename, so two files won't overwrite one another
  uint16_t random_number = rand() & 0xFFFF;

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

  sprintf (opts->mbe_out_fileR, "%s_%s_%04X_S2%s", datestr, timestr, random_number, ext);

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

  time_t event_time = event_struct->Event_History_Items[0].event_time;
  char * datestr = getDateF(event_time);
  char * timestr = getTimeF(event_time);
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

void openWavOutFileLR (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);

  SF_INFO info;
  info.samplerate = 8000; //8000
  info.channels = 2; //2 channel for stereo output
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info); //RDWR will append to file instead of overwrite file

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
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

void rotate_symbol_out_file (dsd_opts * opts, dsd_state * state)
{
  if (opts->symbol_out_f && opts->symbol_out_file_is_auto == 1)
  {
    if ( (time(NULL) - opts->symbol_out_file_creation_time) >= 3600 ) //3600 is one hour in seconds
    {
      //basically just lift the close and open from ncurses handler for 'r' and then 'R'
      // closeSymbolOutFile (opts, state); //open also does this, so don't need to do it twice
      char * timestr = getTime();
      char * datestr = getDate();
      sprintf (opts->symbol_out_file, "%s_%s_dibit_capture.bin", datestr, timestr);
      openSymbolOutFile (opts, state);

      //add a system event to echo in the event history
      state->event_history_s[0].Event_History_Items[0].color_pair = 4;
      char event_str[2000]; memset (event_str, 0, sizeof(event_str));
      sprintf (event_str, "DSD-FME Dibit Capture File Rotated: %s;", opts->symbol_out_file);
      watchdog_event_datacall (opts, state, 0xFFFFFF, 0xFFFFFF, event_str, 0);
      state->lastsrc = 0; //this could wipe a call, but usually on TDMA cc's, slot 1 is the control channel, so may never be set when this is run
      watchdog_event_history(opts, state, 0);
      watchdog_event_current(opts, state, 0);

      //allocated memory pointer needs to be free'd
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
      opts->symbol_out_file_creation_time = time(NULL);
      // opts->symbol_out_file_is_auto = 1;
    }
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

//recover previous IV for SDRTrunk .mbe files when P25p1
uint64_t reverse_lfsr_64_to_len(dsd_opts * opts, uint8_t * iv, int16_t len)
{

  uint64_t lfsr = 0, bit1 = 0, bit2 = 0;

  lfsr = ((uint64_t)iv[0] << 56ULL) + ((uint64_t)iv[1] << 48ULL) + ((uint64_t)iv[2] << 40ULL) + ((uint64_t)iv[3] << 32ULL) + 
         ((uint64_t)iv[4] << 24ULL) + ((uint64_t)iv[5] << 16ULL) + ((uint64_t)iv[6] << 8ULL)  + ((uint64_t)iv[7] << 0ULL);

  memset (iv, 0, 8*sizeof(uint8_t));

  for(int16_t cnt = 0; cnt < len; cnt++)
  {
    //63,61,45,37,27,14
    // Polynomial is C(x) = x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1

    //basically, just get the taps at the +1 position on all but MSB, then check the LSB and configure bit as required
    bit1 = ((lfsr >> 62) ^ (lfsr >> 46) ^ (lfsr >> 38) ^ (lfsr >> 27) ^ (lfsr >> 15)) & 0x1;
    bit2 = lfsr & 1;
    if (bit1 == bit2)
      bit2 = 0;
    else bit2 = 1;

    //just run this in reverse of normal LFSR
    lfsr = (lfsr >> 1) | (bit2 << 63);
  }

  for (int16_t i = 0; i < 8; i++)
    iv[i] = (lfsr >> (56-(i*8))) & 0xFF;

  if (opts->payload == 1)
  {
    fprintf (stderr, " RV LFSR(%02d): ", len);
    for (int16_t i = 0; i < 8; i++)
      fprintf (stderr, "%02X", iv[i]);
    fprintf (stderr, ";");
  }

  return bit2;

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

//DMRA late entry items
static int ambe2_counter;
static int dmra_le;

uint16_t ambe2_str_to_decode(dsd_opts * opts, dsd_state * state, char * ambe_str, uint8_t * ks, uint16_t ks_idx, uint8_t dmra, uint8_t is_enc, uint8_t ks_available)
{

  char ambe_fr[4][24]; memset (ambe_fr, 0, sizeof(ambe_fr));
  uint8_t dibit_pair = 0;
  uint8_t dibit1 = 0, dibit2 = 0;
  const int *w, *x, *y, *z;
  w = rW;
  x = rX;
  y = rY;
  z = rZ;
  for (size_t i = 0; i < 18; i++)
  {

    char octet_char[2];
    octet_char[1] = 0;

    strncpy (octet_char, ambe_str+i, 1);
    sscanf (octet_char, "%hhX", &dibit_pair);

    dibit1 = (dibit_pair >> 2) & 0x3;
    dibit2 = (dibit_pair >> 0) & 0x3;

    //debug
    // fprintf (stderr, "\n dibit_pair: %X = %d, %d;", dibit_pair, dibit1, dibit2);

    //load into ambe_fr
    ambe_fr[*w][*x] = (1 & (dibit1 >> 1)); // bit 1
    ambe_fr[*y][*z] = (1 & (dibit1 >> 0)); // bit 0

    w++;
    x++;
    y++;
    z++;

    ambe_fr[*w][*x] = (1 & (dibit2 >> 1)); // bit 1
    ambe_fr[*y][*z] = (1 & (dibit2 >> 0)); // bit 0

    w++;
    x++;
    y++;
    z++;

    //working now!

  }

  //if using DMRA Late Entry IV mechanics, evaluate for IV
  if (dmra_le)
  {

    //make a copy of ambe_fr codeword 3 (MI fragment)
    uint8_t c3[24]; memset (c3, 0, sizeof(c3));
    for (int i = 0; i < 24; i++)
      c3[i] = ambe_fr[3][i];

    //force slot to 0
    state->currentslot = 0;

    uint8_t c3_hex = (uint64_t)ConvertBitIntoBytes(c3, 4);

    //debug
    // fprintf (stderr, "\n AMBE#: %02d / %d / %d; F: %X;", ambe2_counter, ambe2_counter/3, ambe2_counter%3, c3_hex);

    //use div and mod 3 to set current storage fragment
    state->late_entry_mi_fragment[0][(ambe2_counter/3)+1][ambe2_counter%3] = c3_hex;

    //run LFSR evaluate stored codewords for matching IV or replace if not match or not set
    if (ambe2_counter == 17)
    {
      if (state->payload_mi != 0)
      {
        fprintf (stderr, "\n");
        LFSR(state);
      }
      fprintf (stderr, "\n");
      dmr_late_entry_mi (opts, state);
    }
  }

  char ambe_d[49]; memset(ambe_d, 0, sizeof(ambe_d));
  state->errs = mbe_eccAmbe3600x2450C0 (ambe_fr);
  state->errs2 = state->errs;
  mbe_demodulateAmbe3600x2450Data (ambe_fr);
  state->errs2 += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);
  state->debug_audio_errors += state->errs2;

  //keystream application
  for (uint8_t i = 0; i < 49; i++)
    ambe_d[i] ^= ks[(ks_idx++)%3000];

  //DMRA or P25 KS, skip the left over 7 bits from a byte
  if (dmra == 1)
    ks_idx += 7;

  mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
    ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

  if (opts->payload == 1)
    PrintAMBEData (opts, state, ambe_d);

  if (is_enc == 0 || ks_available == 1)
  {

    //convert and save to .amb file if desired
    if (opts->mbe_out_f != NULL)
      saveAmbe2450Data (opts, state, ambe_d);

    //audio out stack
    if (opts->floating_point == 0)
      processAudio(opts, state);

    //per call wav files
    if (opts->wav_out_f != NULL && opts->dmr_stereo_wav == 1)
      writeSynthesizedVoice (opts, state);

    //static wav file
    if (opts->wav_out_f != NULL && opts->static_wav_file == 1)
      writeSynthesizedVoiceMS (opts, state);

    //to make the static wav file work, I had to write a work around
    //to either play audio from left only when writing wav files,
    //or to play from both speakers if not doing either per-call or static wav
    if (opts->audio_out == 1 && opts->floating_point == 0)
    {
      if (opts->static_wav_file == 1 || opts->dmr_stereo_wav == 1)
        playSynthesizedVoice (opts, state);
      else playSynthesizedVoiceMS (opts, state);
    }

    if (opts->audio_out == 1 && opts->floating_point == 1)
    {
      memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
      playSynthesizedVoiceFM (opts, state);
    }
    //else if not floating point audio or audio out, then purge the audio buffers before they overflow and segfault
    else if (opts->audio_out == 0)
    {
      if (state->audio_out_idx2 >= 800000)
      {
        state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
        state->audio_out_buf_p = state->audio_out_buf + 100;
        memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
        memset (state->audio_out_buf, 0, 100 * sizeof (short));
        state->audio_out_idx2 = 0;
      }
    }

  }

  return ks_idx; //return current ks_idx

}

uint16_t imbe_str_to_decode(dsd_opts * opts, dsd_state * state, char * imbe_str, uint8_t * ks, uint16_t ks_idx, uint8_t is_enc, uint8_t ks_available)
{

  char imbe_fr[8][23]; memset (imbe_fr, 0, sizeof(imbe_fr));
  uint8_t dibit_pair = 0;
  uint8_t dibit1 = 0, dibit2 = 0;
  const int *w, *x, *y, *z;
  w = iW;
  x = iX;
  y = iY;
  z = iZ;
  for (size_t i = 0; i < 36; i++)
  {

    char octet_char[2];
    octet_char[1] = 0;

    strncpy (octet_char, imbe_str+i, 1);
    sscanf (octet_char, "%hhX", &dibit_pair);

    dibit1 = (dibit_pair >> 2) & 0x3;
    dibit2 = (dibit_pair >> 0) & 0x3;

    //debug
    // fprintf (stderr, "\n dibit_pair: %X = %d, %d;", dibit_pair, dibit1, dibit2);

    //load into imbe_fr
    imbe_fr[*w][*x] = (1 & (dibit1 >> 1)); // bit 1
    imbe_fr[*y][*z] = (1 & (dibit1 >> 0)); // bit 0

    w++;
    x++;
    y++;
    z++;

    imbe_fr[*w][*x] = (1 & (dibit2 >> 1)); // bit 1
    imbe_fr[*y][*z] = (1 & (dibit2 >> 0)); // bit 0

    w++;
    x++;
    y++;
    z++;

    //working now!

  }

  char imbe_d[88]; memset(imbe_d, 0, sizeof(imbe_d));
  state->errs = mbe_eccImbe7200x4400C0 (imbe_fr);
  state->errs2 = state->errs;
  mbe_demodulateImbe7200x4400Data (imbe_fr);
  state->errs2 += mbe_eccImbe7200x4400Data (imbe_fr, imbe_d);
  state->debug_audio_errors += state->errs2;

  //keystream application
  for (uint8_t i = 0; i < 88; i++)
    imbe_d[i] ^= ks[(ks_idx++)%3000];

  mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
    imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

  if (opts->payload == 1)
    PrintIMBEData (opts, state, imbe_d);

  if (is_enc == 0 || ks_available == 1)
  {

    //convert and save to .imb file if desired
    if (opts->mbe_out_f != NULL)
      saveImbe4400Data (opts, state, imbe_d);

    //audio out stack
    if (opts->floating_point == 0)
      processAudio(opts, state);

    //per call wav files
    if (opts->wav_out_f != NULL && opts->dmr_stereo_wav == 1)
      writeSynthesizedVoice (opts, state);

    //static wav file
    if (opts->wav_out_f != NULL && opts->static_wav_file == 1)
      writeSynthesizedVoiceMS (opts, state);

    //to make the static wav file work, I had to write a work around
    //to either play audio from left only when writing wav files,
    //or to play from both speakers if not doing either per-call or static wav
    if (opts->audio_out == 1 && opts->floating_point == 0)
    {
      if (opts->static_wav_file == 1 || opts->dmr_stereo_wav == 1)
        playSynthesizedVoice (opts, state);
      else playSynthesizedVoiceMS (opts, state);
    }

    if (opts->audio_out == 1 && opts->floating_point == 1)
    {
      memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
      playSynthesizedVoiceFM (opts, state);
    }
    //else if not floating point audio or audio out, then purge the audio buffers before they overflow and segfault
    else if (opts->audio_out == 0)
    {
      if (state->audio_out_idx2 >= 800000)
      {
        state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
        state->audio_out_buf_p = state->audio_out_buf + 100;
        memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
        memset (state->audio_out_buf, 0, 100 * sizeof (short));
        state->audio_out_idx2 = 0;
      }
    }

  }

  return ks_idx; //return current ks_idx

}

void read_sdrtrunk_json_format (dsd_opts * opts, dsd_state * state)
{

  char * source_str = calloc(0x100000, sizeof(char));
  size_t source_size;

  int8_t protocol = -1;
  uint16_t version = 1; //any .mbe file that does not have a version field should be considered version 1
  uint32_t source = 0;
  uint32_t target = 0;
  int8_t gi = -1;
  uint8_t is_enc = 0;
  uint8_t ks_available = 0; //if encrypted, this signals if a keystream was created for enc muting / unmuting
  uint8_t is_dmra = 1; //Denny, we need an MFID in the JSON file plz
  uint8_t show_time = 1; //if this has already ran once, don't keep showing the time
  uint8_t alg_id = 0;
  uint16_t key_id = 0;
  unsigned long long int iv_hex = 0;
  int rc4_db = 256;
  int rc4_mod = 13;

  time_t event_time = time(NULL);

  //for event history items
  state->dmr_color_code = 0;
  state->lastsrc = 0;
  state->lasttg = 0;
  state->gi[0] = -1;
  state->synctype = -1;
  state->lastsynctype = -1;

  //reset encryption variables
  state->payload_mi = 0;
  state->payload_algid = 0;
  state->payload_keyid = 0;
  if (state->keyloader == 1)
  {
    state->R = 0;
    state->aes_key_loaded[0] = 0;
  }

  //watchdog for event history
  watchdog_event_history(opts, state, 0);
  watchdog_event_current(opts, state, 0);

  uint8_t ks[3000]; memset (ks, 0, sizeof(ks));
  uint16_t ks_idx = 0; //keystream index value

  //P25p1 IMBE / IV out of order execution on .mbe files (fixed in version 2 of .mbe file)
  uint8_t ks_i[3000]; memset (ks_i, 0, sizeof(ks_i));
  uint16_t ks_idx_i = 808; //keystream index value IMBE (start at 808 for out of order ESS)
  int imbe_counter = 0; //count IMBE frames for when to skip 2 bytes of ks and juggle keystreams
  
  //Used in conjunction with DMR's DMRA Late Entry IV
  ambe2_counter = 0; //count AMBE+2 frames for DMRA late entry IV mechanics (static)
  dmra_le = 0;       //whether or not to attempt looking for the DMRA Late Entry
  if (state->forced_alg_id != 0)
    state->payload_mi = 0;

  source_size = fread (source_str, 1, 0x100000, opts->mbe_in_f);

  //debug
  // fprintf (stderr, " Source Size: %d.\n", source_size);
  // fprintf (stderr, "\n");

  char * str_buffer = calloc(source_size, sizeof(char));

  str_buffer = strtok(source_str, "{ \""); //value after initial { open bracket

  //debug print current str_buffer
  // fprintf (stderr, "%s", str_buffer);

  for (size_t i = 0; i < source_size; i++)
  {

    //debug print current str_buffer
    // fprintf (stderr, "%s", str_buffer);

    if (strncmp ("version", str_buffer, 7) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      version = strtol (str_buffer, NULL, 10);

      //debug set value
      if (opts->payload == 1)
        fprintf (stderr, "\n Version: %d;", version);

      //debug print current str_buffer
      // fprintf (stderr, "\n Version: %s", str_buffer);

    }

    //compare and set items accordingly
    if (strncmp ("protocol", str_buffer, 8) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      //debug print current str_buffer
      fprintf (stderr, "\n Protocol: %s", str_buffer);

      //compare protocol and set to proper codec etc
      if (strncmp ("APCO25-PHASE1", str_buffer, 13) == 0)
      {
        //set IMBE protocol here
        protocol = 1;

        //rc4 dropbyte and key len mod
        rc4_db = 267;
        rc4_mod = 13;

        state->synctype = 0;
        state->lastsynctype = 0;
      }

      if (strncmp ("APCO25-PHASE2", str_buffer, 13) == 0)
      {
        //set AMBE+2 protocol here
        protocol = 2;

        //rc4 dropbyte and key len mod
        rc4_db = 256;
        rc4_mod = 13;

        state->synctype = 35;
        state->lastsynctype = 35;
      }

      if (strncmp ("DMR", str_buffer, 3) == 0)
      {
        //set AMBE+2 protocol here
        protocol = 2;

        //rc4 dropbyte and key len mod
        rc4_db = 256;
        rc4_mod = 9;

        state->synctype = 10;
        state->lastsynctype = 10;
      }

      //open .imb or .amb file, if desired, but only after setting a synctype
      if (state->synctype != -1)
      {
        //if converting to .amb or .imb, open that file format as well
        if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
          openMbeOutFile (opts, state);
      }

    }

    if (strncmp ("call_type", str_buffer, 9) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      //set gi value based on this
      if (strncmp ("GROUP", str_buffer, 5) == 0)
        gi = 0;
      else gi = 1;

      state->gi[0] = gi;

      //debug set value
      // fprintf (stderr, " GI: %d;", gi);

      //debug print current str_buffer
      fprintf (stderr, "\n Call Type: %s", str_buffer);

    }

    if (strncmp ("encrypted", str_buffer, 9) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      //set enc value based on this
      if (strncmp ("true", str_buffer, 4) == 0)
        is_enc = 1;
      else is_enc = 0;

      //reset other enc variables (filled in later on if available)
      alg_id = 0;
      key_id = 0;
      iv_hex = 0;

      //debug set value
      // fprintf (stderr, " ENC: %d;", is_enc);

      //debug print current str_buffer
      fprintf (stderr, "\n Encryption: %s", str_buffer);

    }

    //if forcing a DMRA EP mode, turn on dmra_le and set alg here
    if (state->forced_alg_id >= 0x21 &&  state->forced_alg_id <= 0x25)
    {
      is_dmra = 1;
      dmra_le = 1;
      is_enc = 1;
      alg_id = state->forced_alg_id;
      state->payload_algid = alg_id;
      rc4_db = 256;
      rc4_mod = 9;

      //since we can't keyring on a forced alg, we can perhaps look it up
      //via the TO field for the TG value, as long as it isn't greated than 0x1FFFF (upper bound of the array)
      if (state->keyloader == 1 && state->lasttg != 0 && state->lasttg < 0x1FFFF && state->rkey_array[state->lasttg] != 0)
        state->R = state->rkey_array[state->lasttg];

      if (alg_id == 0x21 && state->R != 0 && state->payload_mi != 0)
      {

        //test when we don't have an encryption_mi in the .mbe file for DMR
        //TODO: Handle multi keystream creation with a new function
        uint8_t ks_bytes[375]; memset(ks_bytes, 0, sizeof(ks_bytes));
        uint8_t kiv[32]; memset(kiv, 0, sizeof(kiv));

        //load key into key portion of kiv
        kiv[0] = ((state->R & 0xFF00000000) >> 32);
        kiv[1] = ((state->R & 0xFF000000) >> 24);
        kiv[2] = ((state->R & 0xFF0000) >> 16);
        kiv[3] = ((state->R & 0xFF00) >> 8);
        kiv[4] = ((state->R & 0xFF) >> 0);

        //load IV from late entry state->payload_mi
        kiv[5] = ((state->payload_mi & 0xFF000000) >> 24);
        kiv[6] = ((state->payload_mi & 0xFF0000) >> 16);
        kiv[7] = ((state->payload_mi & 0xFF00) >> 8);
        kiv[8] = ((state->payload_mi & 0xFF) >> 0);

        rc4_block_output (rc4_db, rc4_mod, 200, kiv, ks_bytes);
        unpack_byte_array_into_bit_array(ks_bytes, ks, 200);

        ks_available = 1;

      }

    }
    //TODO: This
    else if (state->forced_alg_id == 1) //Basic Privacy
    {
      //This is where the key loaded comes into play for moto vs hytera vs scrambler
      if (state->K != 0)
      {
        uint64_t k = 0;
        k = BPK[state->K];
        k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
        k <<= 1;
        k += (k >> 48) & 1;

        //load that into ks
        for (int i = 0; i < 18*49; i++)
          ks[i] = (k >> (i%49)) & 1;
      }
      else if (state->H != 0)
      {
        //Hytera BP
      }
      else if (state->R != 0)
      {
        //NXDN Scrambler
      }
    }

    if (strncmp ("to", str_buffer, 2) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      target = strtol (str_buffer, NULL, 10);

      state->lasttg = target;

      //debug set value
      // fprintf (stderr, " Target: %d;", target);

      //debug print current str_buffer
      fprintf (stderr, "\n To: %s", str_buffer);

    }

    if (strncmp ("from", str_buffer, 4) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      source = strtol (str_buffer, NULL, 10);

      state->lastsrc = source;

      //debug set value
      // fprintf (stderr, " Source: %d;", source);

      //debug print current str_buffer
      fprintf (stderr, "\n From: %s", str_buffer);

    }

    if (strncmp ("encryption_algorithm", str_buffer, 20) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      alg_id = strtol (str_buffer, NULL, 10);

      //debug set value
      if (opts->payload == 1)
        fprintf (stderr, "\n Alg ID: %02X;", alg_id);

      //set just in case needed
      is_enc = 1;

      //debug print current str_buffer
      // fprintf (stderr, "\n Encryption Alg: %s", str_buffer);

    }

    if (strncmp ("encryption_key_id", str_buffer, 17) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      key_id = strtol (str_buffer, NULL, 10);

      //debug set value
      if (opts->payload == 1)
        fprintf (stderr, "\n Key ID: %04X;", key_id);

      //set just in case needed
      is_enc = 1;

      //debug print current str_buffer
      // fprintf (stderr, "\n Encryption KID: %s", str_buffer);

    }

    if (strncmp ("encryption_mi", str_buffer, 13) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      uint16_t iv_len = strlen((const char*)str_buffer);
      char iv_str[20]; memset(iv_str, 0, sizeof(iv_str));

      //debug this str_buffer len
      // fprintf (stderr, " IV STR LEN: %d;", iv_len);

      if (iv_len == 18) //P25 MI has an extra zero byte (two zeroes) appended to MI, remove those
        iv_len = 16;
      strncpy(iv_str, str_buffer, iv_len); //copy out final IV value from the MI

      iv_hex = strtoull (iv_str, NULL, 16); //Note: The 16 here is for base 16 (hex), not 16 chars

      //debug set value
      if (opts->payload == 1)
        fprintf (stderr, "\n IV: %016llX;", iv_hex); //not really needed if loaded into array

      //debug print current str_buffer
      // fprintf (stderr, "\n Encryption MI/IV: %s", str_buffer);

      //WIP: This is the last field of enc, so we create a new keystream here, if needed
      state->currentslot = 0;
      state->payload_algid = alg_id;
      state->payload_mi = iv_hex;
      state->payload_keyid = key_id;
      if (state->keyloader == 1)
        keyring(opts, state);

      //TODO: Handle multi keystream creation with a new function
      uint8_t ks_bytes[375]; memset(ks_bytes, 0, sizeof(ks_bytes));
      uint8_t kiv[32]; memset(kiv, 0, sizeof(kiv));

      //RC4 (v1 tested and working on P25, v2 should be okay now)
      if ( (alg_id == 0xAA || alg_id == 0x21) && state->R != 0 )
      {

        //load key into key portion of kiv
        kiv[0] = ((state->R & 0xFF00000000) >> 32);
        kiv[1] = ((state->R & 0xFF000000) >> 24);
        kiv[2] = ((state->R & 0xFF0000) >> 16);
        kiv[3] = ((state->R & 0xFF00) >> 8);
        kiv[4] = ((state->R & 0xFF) >> 0); 

        //load the str_buffer into the IV portion of kiv
        parse_raw_user_string(str_buffer, kiv+5);

        rc4_block_output (rc4_db, rc4_mod, 200, kiv, ks_bytes);

        unpack_byte_array_into_bit_array(ks_bytes, ks, 200);

        //reverse lfsr on IV and create keystream with that as well
        //due to out of order execution on P25p1 ESS sync.
        if (protocol == 1 && version == 1)
        {
          reverse_lfsr_64_to_len (opts, kiv+5, 64);

          memset(ks_bytes, 0, sizeof(ks_bytes));

          rc4_block_output (rc4_db, rc4_mod, 200, kiv, ks_bytes);

          unpack_byte_array_into_bit_array(ks_bytes, ks_i, 200);
        }

        ks_available = 1;

      }

      //P25 DES (v1 tested and working on P1, v2 should be okay now)
      else if (alg_id == 0x81 && state->R != 0)
      {

        des_multi_keystream_output(iv_hex, state->R, ks_bytes, 1, 32); //32*8=256

        if (protocol == 1) //Phase 1 IMBE start on 19 for DES-OFB (8 discard + 9 LCW + 2 reserved)
          unpack_byte_array_into_bit_array(ks_bytes+19, ks, 256-19); //unpack starting after discard
        else if (protocol == 2) //Phase 2 AMBE+2 start on 8 after discard round for DES-OFB
          unpack_byte_array_into_bit_array(ks_bytes+8, ks, 256-8); //unpack starting after discard

        //reverse lfsr on IV and create keystream with that as well
        //due to out of order execution on P25p1 ESS sync.
        if (protocol == 1 && version == 1)
        {

          //load the str_buffer into the IV portion of kiv (just borrowing for DES)
          parse_raw_user_string(str_buffer, kiv+5);

          reverse_lfsr_64_to_len (opts, kiv+5, 64);

          //convert bytes back into value
          iv_hex = 0;
          for (int i = 0; i < 8; i++)
          {
            iv_hex <<= 8;
            iv_hex |= kiv[i+5];
          }

          memset(ks_bytes, 0, sizeof(ks_bytes));

          des_multi_keystream_output(iv_hex, state->R, ks_bytes, 1, 32); //32*8=256

          unpack_byte_array_into_bit_array(ks_bytes+19, ks_i, 256-19); //unpack starting after discard
        }

        ks_available = 1;

      }

      //P25 AES (v1 and v2 tested working on P1)
      else if ( (alg_id == 0x84 || alg_id == 0x89) && state->aes_key_loaded[0] == 1 )
      {

        uint8_t aes_key[32];
        memset(aes_key, 0, sizeof(aes_key));

        //Load key from A1 - A4
        for (int i = 0; i < 8; i++)
        {
          aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
        }

        uint8_t aes_iv[16];
        memset(aes_iv, 0, sizeof(aes_iv));

        //load the str_buffer into the IV portion of aes_iv
        parse_raw_user_string(str_buffer, aes_iv);

        //backup copy of current IV to reverse and expand, if needed
        uint8_t aes_last_iv[16];
        memset(aes_last_iv, 0, sizeof(aes_last_iv));
        for (int i = 0; i < 8; i++)
          aes_last_iv[i] = aes_iv[i];

        //Resolve the longer IV from the shorter one
        lfsr_64_to_128(aes_iv);

        if (alg_id == 0x89) //128, or 256
          aes_ofb_keystream_output(aes_iv, aes_key, ks_bytes, 0, 16); //16*16=256
        else aes_ofb_keystream_output(aes_iv, aes_key, ks_bytes, 2, 16); //16*16=256

        if (protocol == 1) //Phase 1 IMBE start on 27 for AES-OFB (16 discard + 9 LCW + 2 reserved)
          unpack_byte_array_into_bit_array(ks_bytes+27, ks, 256-27); //unpack starting after discard
        else if (protocol == 2) //Phase 2 AMBE+2 start on 16 after discard round for AES-OFB
          unpack_byte_array_into_bit_array(ks_bytes+16, ks, 256-16); //unpack starting after discard

        //reverse lfsr on IV and create keystream with that as well
        //due to out of order execution on P25p1 ESS sync.
        if (protocol == 1 && version == 1)
        {

          reverse_lfsr_64_to_len (opts, aes_last_iv, 64);

          //Resolve the longer IV from the shorter one
          lfsr_64_to_128(aes_last_iv);

          memset(ks_bytes, 0, sizeof(ks_bytes));

          if (alg_id == 0x89) //128, or 256
            aes_ofb_keystream_output(aes_last_iv, aes_key, ks_bytes, 0, 16); //16*16=256
          else aes_ofb_keystream_output(aes_last_iv, aes_key, ks_bytes, 2, 16); //16*16=256

          unpack_byte_array_into_bit_array(ks_bytes+27, ks_i, 256-27); //unpack starting after discard
        }

        ks_available = 1;

      }

      //Pull request: https://github.com/DSheirer/sdrtrunk/pull/2273
      //Merged into SDRTrunk nightly so should be able to handle this better now

      //reset ks_idx to 0
      ks_idx = 0;

      //reset frame counter
      imbe_counter = 0;

      //set just in case needed
      is_enc = 1;

    }

    if (strncmp ("hex", str_buffer, 3) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      if (protocol == 1) //P25p1 IMBE
      {
        //debug print current str_buffer
        // fprintf (stderr, "\n IMBE HEX: %s", str_buffer);

        imbe_counter++;

        //debug
        // fprintf (stderr, "\n IMBE# %02d; KS_IDX: %d; KS_IDX_I: %d; ", imbe_counter, ks_idx, ks_idx_i);

        //36 hex characters on 'hex' which is the IMBE interleaved C codewords
        if (version == 1)
          ks_idx_i = imbe_str_to_decode(opts, state, str_buffer, ks_i, ks_idx_i, is_enc, ks_available);
        else ks_idx = imbe_str_to_decode(opts, state, str_buffer, ks, ks_idx, is_enc, ks_available);

        //skip LSD bits in-between these two IMBE voice frames
        if (imbe_counter == 8 || imbe_counter == 17)
        {
          ks_idx_i += 16;
          ks_idx   += 16;
        }

        //juggle keystreams and reset the I counter (if version 1)
        if (imbe_counter == 9 && version == 1)
        {
          memcpy (ks_i, ks, sizeof(ks_i));
          ks_idx_i = 0;

          //debug
          // fprintf (stderr, " LDU2;");
        }
        //reset keystream idx after frame 18
        else if (imbe_counter == 18 && version == 2)
        {
          ks_idx = 0;

          //debug
          // fprintf (stderr, " LDU2;");
        }
          
        //debug
        // fprintf (stderr, "\n IMBE# %02d; KS_IDX: %d; KS_IDX_I: %d; ", imbe_counter, ks_idx, ks_idx_i);

        //debug
        // if (is_enc == 1 && ks_available == 0)
        //   fprintf (stderr, " Enc Mute;");
        // else if (is_enc == 1 && ks_available == 1)
        //   fprintf (stderr, " Enc Play;");

      }
      else if (protocol == 2) //P25p2 AMBE
      {
        //debug print current str_buffer
        // fprintf (stderr, "\n AMBE HEX: %s", str_buffer);

        //18 hex characters on 'hex' which is the AMBE interleaved C codewords
        ks_idx = ambe2_str_to_decode(opts, state, str_buffer, ks, ks_idx, is_dmra, is_enc, ks_available);

        //debug
        // if (is_enc == 1 && ks_available == 0)
        //   fprintf (stderr, " Enc Mute;");
        // else if (is_enc == 1 && ks_available == 1)
        //   fprintf (stderr, " Enc Play;");

        //increment AMBE+2 counter
        ambe2_counter++;

        //reset if over for DMR 18 AMBE+2 frames
        if (dmra_le && ambe2_counter == 18)
        {
          ambe2_counter = 0;
          ks_idx = 0;
        } 

      }
    }

    if (strncmp ("time", str_buffer, 4) == 0)
    {
      str_buffer = strtok(NULL, " : \""); //next value after any : "" string

      char time_str[20]; memset(time_str, 0, sizeof(time_str));
      strncpy(time_str, str_buffer, 10); //full string is 13, but not copying milliseconds to match time(NULL)

      event_time = strtol (time_str, NULL, 10);

      //working now with tweak
      state->event_history_s[0].Event_History_Items[0].event_time = event_time;

      //debug set value
      // fprintf (stderr, " Time: %ld;", event_time);

      //what is actual time_t for time(NULL);
      // fprintf (stderr, " Time(NULL): %ld;", time(NULL));

      //convert to legible time and date format
      char * timestr = getTimeN(event_time);
      char * datestr = getDateN(event_time);

      //user legible time
      if (show_time == 1)
        fprintf (stderr, " Date: %s Time: %s", datestr, timestr);

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

      show_time = 0;

      //debug print current str_buffer
      // fprintf (stderr, "\n Time: %s", str_buffer);

    }

    //reset ks_idx if this isn't encrypted (ambe only)
    if (is_enc == 0)
      ks_idx = 0;

    str_buffer = strtok(NULL, " : \""); //next value after any : "" string

    if (str_buffer == NULL)
      break;

    //exit loop if signal
    if (exitflag == 1)
      break;
  }

  //free allocated memory from the source string
  if (source_str != NULL)
  {
    free (source_str);
    source_str = NULL;
  }

  //watchdog for event history
  watchdog_event_history(opts, state, 0);
  watchdog_event_current(opts, state, 0);

  //if .imb or .amb file open, close it now
  if (opts->mbe_out_f != NULL)
    closeMbeOutFile (opts, state);

  //end line break
  fprintf (stderr, "\n");

}