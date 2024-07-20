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

//NOTE: This set of functions will be reorganized and simplified (hopefully) or at least
//a more logical flow will be established to jive with the new audio handling

void keyring(dsd_opts * opts, dsd_state * state)
{
  UNUSED(opts);

  if (state->currentslot == 0)
    state->R = state->rkey_array[state->payload_keyid];

  if (state->currentslot == 1)
    state->RR = state->rkey_array[state->payload_keyidR];
}

void RC4(int drop, uint8_t keylength, uint8_t messagelength, uint8_t key[], uint8_t cipher[], uint8_t plain[])
{
  int i, j, count;
  uint8_t t, b;

  //init Sbox
  uint8_t S[256];
  for(int i = 0; i < 256; i++) S[i] = i;

  //Key Scheduling
  j = 0;
  for(i = 0; i < 256; i++)
  {
    j = (j + S[i] + key[i % keylength]) % 256;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
  }

  //Drop Bytes and Cipher Byte XOR
  i = j = 0;
  for(count = 0; count < (messagelength + drop); count++)
  {
    i = (i + 1) % 256;
    j = (j + S[i]) % 256;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
    b = S[(S[i] + S[j]) % 256];

    //return mbe payload byte here
    if (count >= drop)
      plain[count - drop] = b^cipher[count - drop];

  }

}

int Pr[256] = {
0x0000, 0x1F00, 0xE300, 0xFC00, 0x2503, 0x3A03, 0xC603, 0xD903,
0x4A05, 0x5505, 0xA905, 0xB605, 0x6F06, 0x7006, 0x8C06, 0x9306,
0x2618, 0x3918, 0xC518, 0xDA18, 0x031B, 0x1C1B, 0xE01B, 0xFF1B,
0x6C1D, 0x731D, 0x8F1D, 0x901D, 0x491E, 0x561E, 0xAA1E, 0xB51E, //31
0x4B28, 0x5428, 0xA828, 0xB728, 0x6E2B, 0x712B, 0x8D2B, 0x922B,
0x012D, 0x1E2D, 0xE22D, 0xFD2D, 0x242E, 0x3B2E, 0xC72E, 0xD82E,
0x6D30, 0x7230, 0x8E30, 0x9130, 0x4833, 0x5733, 0xAB33, 0xB433,
0x2735, 0x3835, 0xC435, 0xDB35, 0x0236, 0x1D36, 0xE136, 0xFE36, //63
0x2B49, 0x3449, 0xC849, 0xD749, 0x0E4A, 0x114A, 0xED4A, 0xF24A,
0x614C, 0x7E4C, 0x824C, 0x9D4C, 0x444F, 0x5B4F, 0xA74F, 0xB84F,
0x0D51, 0x1251, 0xEE51, 0xF151, 0x2852, 0x3752, 0xCB52, 0xD452,
0x4754, 0x5854, 0xA454, 0xBB54, 0x6257, 0x7D57, 0x8157, 0x9E57, //95
0x6061, 0x7F61, 0x8361, 0x9C61, 0x4562, 0x5A62, 0xA662, 0xB962,
0x2A64, 0x3564, 0xC964, 0xD664, 0x0F67, 0x1067, 0xEC67, 0xF367,
0x4679, 0x5979, 0xA579, 0xBA79, 0x637A, 0x7C7A, 0x807A, 0x9F7A,
0x0C7C, 0x137C, 0xEF7C, 0xF07C, 0x297F, 0x367F, 0xCA7F, 0xD57F, //127
0x4D89, 0x5289, 0xAE89, 0xB189, 0x688A, 0x778A, 0x8B8A, 0x948A,
0x078C, 0x188C, 0xE48C, 0xFB8C, 0x228F, 0x3D8F, 0xC18F, 0xDE8F,
0x6B91, 0x7491, 0x8891, 0x9791, 0x4E92, 0x5192, 0xAD92, 0xB292,
0x2194, 0x3E94, 0xC294, 0xDD94, 0x0497, 0x1B97, 0xE797, 0xF897, //159
0x06A1, 0x19A1, 0xE5A1, 0xFAA1, 0x23A2, 0x3CA2, 0xC0A2, 0xDFA2,
0x4CA4, 0x53A4, 0xAFA4, 0xB0A4, 0x69A7, 0x76A7, 0x8AA7, 0x95A7,
0x20B9, 0x3FB9, 0xC3B9, 0xDCB9, 0x05BA, 0x1ABA, 0xE6BA, 0xF9BA,
0x6ABC, 0x75BC, 0x89BC, 0x96BC, 0x4FBF, 0x50BF, 0xACBF, 0xB3BF, //191
0x66C0, 0x79C0, 0x85C0, 0x9AC0, 0x43C3, 0x5CC3, 0xA0C3, 0xBFC3,
0x2CC5, 0x33C5, 0xCFC5, 0xD0C5, 0x09C6, 0x16C6, 0xEAC6, 0xF5C6,
0x84D0, 0x85DF, 0x8AD3, 0x8BDC, 0xB6D5, 0xB7DA, 0xB8D6, 0xB9D9,
0xD0DA, 0xD1D5, 0xDED9, 0xDFD6, 0xE2DF, 0xE3D0, 0xECDC, 0xEDD3, //223
0x2DE8, 0x32E8, 0xCEE8, 0xD1E8, 0x08EB, 0x17EB, 0xEBEB, 0xF4EB,
0x67ED, 0x78ED, 0x84ED, 0x9BED, 0x42EE, 0x5DEE, 0xA1EE, 0xBEEE,
0x0BF0, 0x14F0, 0xE8F0, 0xF7F0, 0x2EF3, 0x31F3, 0xCDF3, 0xD2F3,
0x41F5, 0x5EF5, 0xA2F5, 0xBDF5, 0x64F6, 0x7BF6, 0x87F6, 0x98F6 //255
};

void playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv)
{

  int i;
  char imbe_d[88];
  char ambe_d[49];

  for (i = state->optind; i < argc; i++)
  {
    sprintf (opts->mbe_in_file, "%s", argv[i]);
    openMbeInFile (opts, state);
    mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
    fprintf (stderr, "playing %s\n", opts->mbe_in_file);
    while (feof (opts->mbe_in_f) == 0)
    {
      if (state->mbe_file_type == 0)
      {
        readImbe4400Data (opts, state, imbe_d);
        mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
        if (opts->audio_out == 1 && opts->floating_point == 0)
        {
          processAudio(opts, state);
        }
        if (opts->wav_out_f != NULL)
        {
          writeSynthesizedVoice (opts, state);
        }

        if (opts->audio_out == 1 && opts->floating_point == 0)
        {
          playSynthesizedVoiceMS (opts, state);
        }
        if (opts->floating_point == 1)
        {
          memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
          playSynthesizedVoiceFM (opts, state);
        }
      }
      else if (state->mbe_file_type > 0) //ambe files
      {
        readAmbe2450Data (opts, state, ambe_d);
        int x;
        unsigned long long int k;
        if (state->K != 0) //apply Pr key
        {
          k = Pr[state->K];
          k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
          for (short int j = 0; j < 48; j++) //49
          {
            x = ( ((k << j) & 0x800000000000) >> 47 );
            ambe_d[j] ^= x;
          }
        }
        
        //ambe+2
        if (state->mbe_file_type == 1) mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
        //dstar ambe
        if (state->mbe_file_type == 2) mbe_processAmbe2400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

        if (opts->audio_out == 1 && opts->floating_point == 0)
        {
          processAudio(opts, state);
        }
        if (opts->wav_out_f != NULL)
        {
          writeSynthesizedVoice (opts, state);
        }

        if (opts->audio_out == 1 && opts->floating_point == 0)
        {
          playSynthesizedVoiceMS (opts, state);
        }
        if (opts->floating_point == 1)
        {
          memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
          playSynthesizedVoiceFM (opts, state);
        }
      }
      if (exitflag == 1)
      {
        cleanupAndExit (opts, state);
      }
    }
  }
}

void
processMbeFrame (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24])
{

  int i;
  char imbe_d[88];
  char ambe_d[49];
  unsigned long long int k;
  int x;

  //these conditions should ensure no clashing with the BP/HBP/Scrambler key loading machanisms already coded in
  if (state->currentslot == 0 && state->payload_algid != 0 && state->payload_algid != 0x80 && state->keyloader == 1)
    keyring (opts, state);

  if (state->currentslot == 1 && state->payload_algidR != 0 && state->payload_algidR != 0x80 && state->keyloader == 1)
    keyring (opts, state);

  //24-bit TG to 16-bit hash
  uint32_t hash = 0;
  uint8_t hash_bits[24];
  memset (hash_bits, 0, sizeof(hash_bits));

  int preempt = 0; //TDMA dual voice slot preemption(when using OSS output)


  for (i = 0; i < 88; i++)
  {
    imbe_d[i] = 0;
  }

  for (i = 0; i < 49; i++)
  {
    ambe_d[i] = 0;
  }

  //set playback mode for this frame
  char mode[8];
  sprintf (mode, "%s", "");

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

  int groupNumber = 0;

  if (state->currentslot == 0) groupNumber = state->lasttg;
  else groupNumber = state->lasttgR;

  for (i = 0; i < state->group_tally; i++)
  {
    if (state->group_array[i].groupNumber == groupNumber)
    {
      strcpy (mode, state->group_array[i].groupMode);
      break;
    }
  }

  //set flag to not play audio this time, but won't prevent writing to wav files -- disabled for now
  // if (strcmp(mode, "B") == 0) opts->audio_out = 0; //causes a buzzing now (probably due to not running processAudio before the SS3 or SS4)

  //end set playback mode for this frame

  if ((state->synctype == 0) || (state->synctype == 1)) 
  {
    //  0 +P25p1
    //  1 -P25p1
    state->errs = mbe_eccImbe7200x4400C0 (imbe_fr);
    state->errs2 = state->errs;
    mbe_demodulateImbe7200x4400Data (imbe_fr);
    state->errs2 += mbe_eccImbe7200x4400Data (imbe_fr, imbe_d);

    //P25p1 RC4 Handling
    if (state->payload_algid == 0xAA && state->R != 0)
    {
      uint8_t cipher[11] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      uint8_t plain[11]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      uint8_t rckey[13]  = {0x00, 0x00, 0x00, 0x00, 0x00, // <- RC4 Key
                            0x00, 0x00, 0x00, 0x00, 0x00, // <- MI
                            0x00, 0x00, 0x00}; // <- MI cont.

      //easier to manually load up rather than make a loop
      rckey[0] = ((state->R & 0xFF00000000) >> 32);
      rckey[1] = ((state->R & 0xFF000000) >> 24);
      rckey[2] = ((state->R & 0xFF0000) >> 16);
      rckey[3] = ((state->R & 0xFF00) >> 8);
      rckey[4] = ((state->R & 0xFF) >> 0);

      // load valid MI from state->payload_miP
      rckey[5]  = ((state->payload_miP & 0xFF00000000000000) >> 56);
      rckey[6]  = ((state->payload_miP & 0xFF000000000000) >> 48);
      rckey[7]  = ((state->payload_miP & 0xFF0000000000) >> 40);
      rckey[8]  = ((state->payload_miP & 0xFF00000000) >> 32);
      rckey[9]  = ((state->payload_miP & 0xFF000000) >> 24);
      rckey[10] = ((state->payload_miP & 0xFF0000) >> 16);
      rckey[11] = ((state->payload_miP & 0xFF00) >> 8);
      rckey[12] = ((state->payload_miP & 0xFF) >> 0);

      // if (state->p25vc == 0)
      // {
      // 	fprintf (stderr, "%s", KYEL);
      // 	fprintf (stderr, "\n RC4K ");
      // 	for (short o = 0; o < 13; o++)
      // 	{
      // 		fprintf (stderr, "%02X", rckey[o]);
      // 	}
      // 	fprintf (stderr, "%s", KNRM);
      // }

      //load imbe_d into imbe_cipher octets
      int z = 0;
      for (i = 0; i < 11; i++)
      {
        cipher[i] = 0;
        plain[i] = 0;
        for (short int j = 0; j < 8; j++)
        {
          cipher[i] = cipher[i] << 1;
          cipher[i] = cipher[i] + imbe_d[z];
          imbe_d[z] = 0;
          z++;
        }
      }

      RC4(state->dropL, 13, 11, rckey, cipher, plain);
      state->dropL += 11;

      z = 0;
      for (short p = 0; p < 11; p++)
      {
        for (short o = 0; o < 8; o++)
        {
          imbe_d[z] = (plain[p] & 0x80) >> 7;
          plain[p] = plain[p] << 1;
          z++;
        }
      }

    }

    mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
      imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

    //mbe_processImbe7200x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe_fr, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    if (opts->payload == 1)
    {
      PrintIMBEData (opts, state, imbe_d);
    }

    //increment vc counter by one.
    state->p25vc++;

    if (opts->mbe_out_f != NULL && state->dmr_encL == 0) //only save if this bit not set
    {
      saveImbe4400Data (opts, state, imbe_d);
    }
  }
  else if ((state->synctype == 14) || (state->synctype == 15)) //pV Sync
  {

    state->errs = mbe_eccImbe7100x4400C0 (imbe7100_fr);
    state->errs2 = state->errs;
    mbe_demodulateImbe7100x4400Data (imbe7100_fr);
    state->errs2 += mbe_eccImbe7100x4400Data (imbe7100_fr, imbe_d);

    if (opts->payload == 1)
    {
      PrintIMBEData (opts, state, imbe_d);
    }

    mbe_convertImbe7100to7200(imbe_d);
    mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
                              imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

    if (opts->mbe_out_f != NULL)
    {
      saveImbe4400Data (opts, state, imbe_d);
    }
  }
  else if ((state->synctype == 6) || (state->synctype == 7))
  {
    mbe_processAmbe3600x2400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    if (opts->payload == 1)
    {
      PrintAMBEData (opts, state, ambe_d);
    }
    if (opts->mbe_out_f != NULL)
    {
      saveAmbe2450Data (opts, state, ambe_d);
    }
  }
  else if ((state->synctype == 28) || (state->synctype == 29)) //was 8 and 9
  {

    state->errs = mbe_eccAmbe3600x2450C0 (ambe_fr);
    state->errs2 = state->errs;
    mbe_demodulateAmbe3600x2450Data (ambe_fr);
    state->errs2 += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);

    if ( (state->nxdn_cipher_type == 0x01 && state->R > 0) ||
          (state->M == 1 && state->R > 0) )
    {

      if (state->payload_miN == 0)
      {
        state->payload_miN = state->R;
      }

      char ambe_temp[49];
      for (short int i = 0; i < 49; i++)
      {
        ambe_temp[i] = ambe_d[i];
        ambe_d[i] = 0;
      }
      LFSRN(ambe_temp, ambe_d, state);

    }

    mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
                              ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

    if (opts->payload == 1)
    {
      PrintAMBEData (opts, state, ambe_d);
    }

    if (opts->mbe_out_f != NULL && (state->dmr_encL == 0 || opts->dmr_mute_encL == 0) )
    {
      saveAmbe2450Data (opts, state, ambe_d);
    }

	}
  else
  {
    //stereo slots and slot 0 (left slot)
    if (state->currentslot == 0) //&& opts->dmr_stereo == 1
    {

      state->errs = mbe_eccAmbe3600x2450C0 (ambe_fr);
      state->errs2 = state->errs;
      mbe_demodulateAmbe3600x2450Data (ambe_fr);
      state->errs2 += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);

      //EXPERIMENTAL!!
      //load basic privacy key number from array by the tg value (if not forced)
      //currently only Moto BP and Hytera 10 Char BP
      if (state->M == 0 && state->payload_algid == 0)
      {
        //see if we need to hash a value larger than 16-bits
        hash = state->lasttg & 0xFFFFFF;
        // fprintf (stderr, "TG: %lld Hash: %ld ", state->lasttg, hash); 
        if (hash > 0xFFFF) //if greater than 16-bits
        {
          for (int i = 0; i < 24; i++)
          {
            hash_bits[i] = ((hash << i) & 0x800000) >> 23; //load into array for CRC16 
          }
          hash = ComputeCrcCCITT16d (hash_bits, 24);
          hash = hash & 0xFFFF; //make sure its no larger than 16-bits
          // fprintf (stderr, "Hash: %d ", hash);
        }
        if (state->rkey_array[hash] != 0)
        {
          state->K = state->rkey_array[hash] & 0xFF; //doesn't exceed 255
          state->K1 = state->H = state->rkey_array[hash] & 0xFFFFFFFFFF; //doesn't exceed 40-bit limit
          opts->dmr_mute_encL = 0;
          // fprintf (stderr, "Key: %X ", state->rkey_array[hash]);
        }
        // else opts->dmr_mute_encL = 1; //may cause issues for manual key entry (non-csv)
      }

      if ( (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x10) ||
            (state->K > 0 && state->M == 1) )
      {
        k = Pr[state->K];
        k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
        for (short int j = 0; j < 48; j++) //49
        {
          x = ( ((k << j) & 0x800000000000) >> 47 );
          ambe_d[j] ^= x;
        }
      }

      if ( (state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68) ||
            (state->K1 > 0 && state->M == 1) )
      {

      int pos = 0;

      unsigned long long int k1 = state->K1;
      unsigned long long int k2 = state->K2;
      unsigned long long int k3 = state->K3;
      unsigned long long int k4 = state->K4;

      int T_Key[256] = {0};
      int pN[882] = {0};

      int len = 0;

      if (k2 == 0)
      {
        len = 39;
        k1 = k1 << 24;
      }
      if (k2 != 0)
      {
        len = 127;
      }
      if (k4 != 0)
      {
        len = 255;
      }

      for (i = 0; i < 64; i++)
      {
        T_Key[i]     = ( ((k1 << i) & 0x8000000000000000) >> 63 );
        T_Key[i+64]  = ( ((k2 << i) & 0x8000000000000000) >> 63 );
        T_Key[i+128] = ( ((k3 << i) & 0x8000000000000000) >> 63 );
        T_Key[i+192] = ( ((k4 << i) & 0x8000000000000000) >> 63 );
      }

      for (i = 0; i < 882; i++)
      {
        pN[i] = T_Key[pos];
        pos++;
        if (pos > len)
        {
          pos = 0;
        }
      }

      //sanity check
      if (state->DMRvcL > 17) //18
      {
        state->DMRvcL = 17; //18
      }

      pos = state->DMRvcL * 49;
      for(i = 0; i < 49; i++)
      {
        ambe_d[i] ^= pN[pos];
        pos++;
      }
      state->DMRvcL++;
      }

      //DMR RC4, Slot 1
      if (state->currentslot == 0 && state->payload_algid == 0x21 && state->R != 0)
      {
        uint8_t cipher[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t plain[7]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t rckey[9]  = {0x00, 0x00, 0x00, 0x00, 0x00, // <- RC4 Key
                             0x00, 0x00, 0x00, 0x00}; // <- MI

        //easier to manually load up rather than make a loop
        rckey[0] = ((state->R & 0xFF00000000) >> 32);
        rckey[1] = ((state->R & 0xFF000000) >> 24);
        rckey[2] = ((state->R & 0xFF0000) >> 16);
        rckey[3] = ((state->R & 0xFF00) >> 8);
        rckey[4] = ((state->R & 0xFF) >> 0);
        rckey[5] = ((state->payload_mi & 0xFF000000) >> 24);
        rckey[6] = ((state->payload_mi & 0xFF0000) >> 16);
        rckey[7] = ((state->payload_mi & 0xFF00) >> 8);
        rckey[8] = ((state->payload_mi & 0xFF) >> 0);

        //pack cipher byte array from ambe_d bit array
        pack_ambe(ambe_d, cipher, 49);

        //only run keystream application if errs < 3 -- this is a fix to the pop sound
        //that may occur on some systems that preempt VC6 voice for a RC opportuninity (TXI)
        //this occurs because we are supposed to either have a a 'repeat' frame, or 'silent' frame play
        //due to the error, but the keystream application makes it random 'pfft pop' sound instead
        if (state->errs < 3)
          RC4(state->dropL, 9, 7, rckey, cipher, plain);
        else memcpy (plain, cipher, sizeof(plain));

        state->dropL += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }

      //P25p2 RC4 Handling, VCH 0
      if (state->currentslot == 0 && state->payload_algid == 0xAA && state->R != 0 && ((state->synctype == 35) || (state->synctype == 36)))
      {
        uint8_t cipher[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t plain[7]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t rckey[13] = {0x00, 0x00, 0x00, 0x00, 0x00, // <- RC4 Key
                             0x00, 0x00, 0x00, 0x00, 0x00, // <- MI
                             0x00, 0x00, 0x00}; // <- MI cont.

        //easier to manually load up rather than make a loop
        rckey[0] = ((state->R & 0xFF00000000) >> 32);
        rckey[1] = ((state->R & 0xFF000000) >> 24);
        rckey[2] = ((state->R & 0xFF0000) >> 16);
        rckey[3] = ((state->R & 0xFF00) >> 8);
        rckey[4] = ((state->R & 0xFF) >> 0);

        // load valid MI from state->payload_miP
        rckey[5]  = ((state->payload_miP & 0xFF00000000000000) >> 56);
        rckey[6]  = ((state->payload_miP & 0xFF000000000000) >> 48);
        rckey[7]  = ((state->payload_miP & 0xFF0000000000) >> 40);
        rckey[8]  = ((state->payload_miP & 0xFF00000000) >> 32);
        rckey[9]  = ((state->payload_miP & 0xFF000000) >> 24);
        rckey[10] = ((state->payload_miP & 0xFF0000) >> 16);
        rckey[11] = ((state->payload_miP & 0xFF00) >> 8);
        rckey[12] = ((state->payload_miP & 0xFF) >> 0);

        //pack cipher byte array from ambe_d bit array
        pack_ambe(ambe_d, cipher, 49);

        RC4(state->dropL, 13, 7, rckey, cipher, plain);
        state->dropL += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, 
        ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);


      //old method for this step below
      //mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      if (opts->payload == 1) // && state->R == 0 this is why slot 1 didn't primt abme, probably had it set during testing
      {
        PrintAMBEData (opts, state, ambe_d);
      }

      //restore MBE file save, slot 1 -- consider saving even if enc
      if (opts->mbe_out_f != NULL && (state->dmr_encL == 0 || opts->dmr_mute_encL == 0) )
      {
        saveAmbe2450Data (opts, state, ambe_d);
      }

    }
    //stereo slots and slot 1 (right slot)
    if (state->currentslot == 1) //&& opts->dmr_stereo == 1
    {

      state->errsR = mbe_eccAmbe3600x2450C0 (ambe_fr);
      state->errs2R = state->errsR;
      mbe_demodulateAmbe3600x2450Data (ambe_fr);
      state->errs2R += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);

      //EXPERIMENTAL!!
      //load basic privacy key number from array by the tg value (if not forced)
      //currently only Moto BP and Hytera 10 Char BP
      if (state->M == 0 && state->payload_algidR == 0)
      {
        //see if we need to hash a value larger than 16-bits
        hash = state->lasttgR & 0xFFFFFF; 
        // fprintf (stderr, "TG: %lld Hash: %ld ", state->lasttgR, hash);
        if (hash > 0xFFFF) //if greater than 16-bits
        {
          for (int i = 0; i < 24; i++)
          {
            hash_bits[i] = ((hash << i) & 0x800000) >> 23; //load into array for CRC16 
          }
          hash = ComputeCrcCCITT16d (hash_bits, 24);
          hash = hash & 0xFFFF; //make sure its no larger than 16-bits
          // fprintf (stderr, "Hash: %d ", hash);
        }
        if (state->rkey_array[hash] != 0)
        {
          state->K = state->rkey_array[hash] & 0xFF; //doesn't exceed 255
          state->K1 = state->H = state->rkey_array[hash] & 0xFFFFFFFFFF; //doesn't exceed 40-bit limit
          opts->dmr_mute_encR = 0;
          // fprintf (stderr, "Key: %X ", state->rkey_array[hash]);
        }
        // else opts->dmr_mute_encR = 1; //may cause issues for manual key entry (non-csv)
      }

      if ( (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fidR == 0x10) ||
            (state->K > 0 && state->M == 1) )
      {
        k = Pr[state->K];
        k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
        for (short int j = 0; j < 48; j++)
        {
          x = ( ((k << j) & 0x800000000000) >> 47 );
          ambe_d[j] ^= x;
        }
      }

      if ( (state->K1 > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fidR == 0x68) ||
            (state->K1 > 0 && state->M == 1))
      {

        int pos = 0;

        unsigned long long int k1 = state->K1;
        unsigned long long int k2 = state->K2;
        unsigned long long int k3 = state->K3;
        unsigned long long int k4 = state->K4;

        int T_Key[256] = {0};
        int pN[882] = {0};

        int len = 0;

        if (k2 == 0)
        {
          len = 39;
          k1 = k1 << 24;
        }
        if (k2 != 0)
        {
          len = 127;
        }
        if (k4 != 0)
        {
          len = 255;
        }

        for (i = 0; i < 64; i++)
        {
          T_Key[i]     = ( ((k1 << i) & 0x8000000000000000) >> 63 );
          T_Key[i+64]  = ( ((k2 << i) & 0x8000000000000000) >> 63 );
          T_Key[i+128] = ( ((k3 << i) & 0x8000000000000000) >> 63 );
          T_Key[i+192] = ( ((k4 << i) & 0x8000000000000000) >> 63 );
        }

        for (i = 0; i < 882; i++)
        {
          pN[i] = T_Key[pos];
          pos++;
          if (pos > len)
          {
            pos = 0;
          }
        }

        //sanity check
        if (state->DMRvcR > 17) //18
        {
          state->DMRvcR = 17; //18
        }

        pos = state->DMRvcR * 49;
        for(i = 0; i < 49; i++)
        {
          ambe_d[i] ^= pN[pos];
          pos++;
        }
        state->DMRvcR++;
      }

      //DMR RC4, Slot 2
      if (state->currentslot == 1 && state->payload_algidR == 0x21 && state->RR != 0)
      {
        uint8_t cipher[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t plain[7]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t rckey[9]  = {0x00, 0x00, 0x00, 0x00, 0x00, // <- RC4 Key
                             0x00, 0x00, 0x00, 0x00}; // <- MI

        //easier to manually load up rather than make a loop
        rckey[0] = ((state->RR & 0xFF00000000) >> 32);
        rckey[1] = ((state->RR & 0xFF000000) >> 24);
        rckey[2] = ((state->RR & 0xFF0000) >> 16);
        rckey[3] = ((state->RR & 0xFF00) >> 8);
        rckey[4] = ((state->RR & 0xFF) >> 0);
        rckey[5] = ((state->payload_miR & 0xFF000000) >> 24);
        rckey[6] = ((state->payload_miR & 0xFF0000) >> 16);
        rckey[7] = ((state->payload_miR & 0xFF00) >> 8);
        rckey[8] = ((state->payload_miR & 0xFF) >> 0);

        //pack cipher byte array from ambe_d bit array
        pack_ambe(ambe_d, cipher, 49);

        //only run keystream application if errs < 3 -- this is a fix to the pop sound
        //that may occur on some systems that preempt VC6 voice for a RC opportuninity (TXI)
        //this occurs because we are supposed to either have a a 'repeat' frame, or 'silent' frame play
        //due to the error, but the keystream application makes it random 'pfft pop' sound instead
        if (state->errsR < 3)
          RC4(state->dropR, 9, 7, rckey, cipher, plain);
        else memcpy (plain, cipher, sizeof(plain));
        state->dropR += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }

      //P25p2 RC4 Handling, VCH 1
      if (state->currentslot == 1 && state->payload_algidR == 0xAA && state->RR != 0 && ((state->synctype == 35) || (state->synctype == 36)))
      {
        uint8_t cipher[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t plain[7]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t rckey[13] = {0x00, 0x00, 0x00, 0x00, 0x00, // <- RC4 Key
                             0x00, 0x00, 0x00, 0x00, 0x00, // <- MI
                             0x00, 0x00, 0x00}; // <- MI cont.

        //easier to manually load up rather than make a loop
        rckey[0] = ((state->RR & 0xFF00000000) >> 32);
        rckey[1] = ((state->RR & 0xFF000000) >> 24);
        rckey[2] = ((state->RR & 0xFF0000) >> 16);
        rckey[3] = ((state->RR & 0xFF00) >> 8);
        rckey[4] = ((state->RR & 0xFF) >> 0);

        //state->payload_miN for VCH1/slot 2
        rckey[5]  = ((state->payload_miN & 0xFF00000000000000) >> 56);
        rckey[6]  = ((state->payload_miN & 0xFF000000000000) >> 48);
        rckey[7]  = ((state->payload_miN & 0xFF0000000000) >> 40);
        rckey[8]  = ((state->payload_miN & 0xFF00000000) >> 32);
        rckey[9]  = ((state->payload_miN & 0xFF000000) >> 24);
        rckey[10] = ((state->payload_miN & 0xFF0000) >> 16);
        rckey[11] = ((state->payload_miN & 0xFF00) >> 8);
        rckey[12] = ((state->payload_miN & 0xFF) >> 0);

        //pack cipher byte array from ambe_d bit array
        pack_ambe(ambe_d, cipher, 49);

        RC4(state->dropR, 13, 7, rckey, cipher, plain);
        state->dropR += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }

      mbe_processAmbe2450Dataf (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR, 
        ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);

      //old method for this step below
      //mbe_processAmbe3600x2450Framef (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR, ambe_fr, ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);
      if (opts->payload == 1)
      {
        PrintAMBEData (opts, state, ambe_d);
      }

      //restore MBE file save, slot 2 -- consider saving even if enc
      if (opts->mbe_out_fR != NULL && (state->dmr_encR == 0 || opts->dmr_mute_encR == 0) )
      {
        saveAmbe2450DataR (opts, state, ambe_d);
      }
    }

    //X2-TDMA? Not sure what still makes it this far to run under Framef
    // if (opts->dmr_stereo == 0)
    // {
    //   mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
    //   if (opts->payload == 1)
    //   {
    //     PrintAMBEData (opts, state, ambe_d);
    //   }
    //   //only save MBE files if not enc or unmuted, THIS does not seem to work for some reason
    //   if (opts->mbe_out_f != NULL && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0) )
    //   {
    //     saveAmbe2450Data (opts, state, ambe_d);
    //   }
    // }


  }

  //quick enc check to determine whether or not to play enc traffic
  int enc_bit = 0;
  //end enc check

  if ( (opts->dmr_mono == 1 || opts->dmr_stereo == 1) && state->currentslot == 0) //all mono traffic routed through 'left'
  {
    enc_bit = (state->dmr_so >> 6) & 0x1;
    if (enc_bit == 1)
    {
      state->dmr_encL = 1;
    }

    //checkdown for P25 1 and 2
    else if (state->payload_algid != 0 && state->payload_algid != 0x80)
    {
      state->dmr_encL = 1;
    }
    else state->dmr_encL = 0;

    //check for available R key
    if (state->R != 0) state->dmr_encL = 0;

    //second checkdown for P25p2 WACN, SYSID, and CC set
    if (state->synctype == 35 || state->synctype == 36)
    {
      if (state->p2_wacn == 0 || state->p2_sysid == 0 || state->p2_cc == 0)
      {
        state->dmr_encL = 1;
      }
    }

    //reverse mute testing, only mute unencrypted traffic (slave piggyback dsd+ method)
    if (opts->reverse_mute == 1)
    {
      if (state->dmr_encL == 0)
      {
        state->dmr_encL = 1;
        opts->unmute_encrypted_p25 = 0;
        opts->dmr_mute_encL = 1;
      } 
      else
      {
        state->dmr_encL = 0;
        opts->unmute_encrypted_p25 = 1;
        opts->dmr_mute_encL = 0;
      } 
    }
    //end reverse mute test

    //OSS 48k/1 Specific Voice Preemption if dual voices on TDMA and one slot has preference over the other
    if (opts->slot_preference == 1 && opts->audio_out_type == 5 && opts->audio_out == 1 && (state->dmrburstR == 16 || state->dmrburstR == 21) ) 
    {
      opts->audio_out = 0;
      preempt = 1;
      if (opts->payload == 0 && opts->slot1_on == 1) 
        fprintf (stderr, " *MUTED*"); 
      else if (opts->payload == 0 && opts->slot1_on == 0) 
        fprintf (stderr, " *OFF*"); 
    }

    state->debug_audio_errors += state->errs2;

    if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0)
    {
      if ( opts->floating_point == 0 ) //opts->audio_out == 1 && //needed to remove for AERO OSS so we could still save wav files during dual voices
      {
        #ifdef AERO_BUILD
        if(opts->audio_out == 1 && opts->slot1_on == 1) //add conditional check here, otherwise some lag occurs on dual voices with OSS48k/1 input due to buffered audio
        #endif
        processAudio(opts, state);
      }
      if (opts->audio_out == 1 && opts->floating_point == 0 && opts->audio_out_type == 5 && opts->slot1_on == 1) //for OSS 48k 1 channel configs -- relocate later if possible
      {
        playSynthesizedVoiceMS (opts, state); //it may be more beneficial to move this to each individual decoding type to handle, but ultimately, let's just simpifly mbe handling instead 
      }
    }

    memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l)); //these are for mono or FDMA where we don't need to buffer and wait for a stereo mix 

  }

  if (opts->dmr_stereo == 1 && state->currentslot == 1) 
  {
    enc_bit = (state->dmr_soR >> 6) & 0x1;
    if (enc_bit == 0x1)
    {
      state->dmr_encR = 1;
    }

    //checkdown for P25 1 and 2
    else if (state->payload_algidR != 0 && state->payload_algidR != 0x80)
    {
      state->dmr_encR = 1;
    }
    else state->dmr_encR = 0;

    //check for available RR key
    if (state->RR != 0) state->dmr_encR = 0;

    //second checkdown for P25p2 WACN, SYSID, and CC set
    if (state->synctype == 35 || state->synctype == 36)
    {
      if (state->p2_wacn == 0 || state->p2_sysid == 0 || state->p2_cc == 0)
      {
        state->dmr_encR = 1;
      }
    }

    //reverse mute testing, only mute unencrypted traffic (slave piggyback dsd+ method)
    if (opts->reverse_mute == 1)
    {
      if (state->dmr_encR == 0)
      {
        state->dmr_encR = 1;
        opts->unmute_encrypted_p25 = 0;
        opts->dmr_mute_encR = 1;
      } 
      else
      {
        state->dmr_encR = 0;
        opts->unmute_encrypted_p25 = 1;
        opts->dmr_mute_encR = 0;
      } 
    }
    //end reverse mute test

    //OSS 48k/1 Specific Voice Preemption if dual voices on TDMA and one slot has preference over the other
    if (opts->slot_preference == 0 && opts->audio_out_type == 5 && opts->audio_out == 1 && (state->dmrburstL == 16 || state->dmrburstL == 21) ) 
    {
      opts->audio_out = 0;
      preempt = 1;
      if (opts->payload == 0 && opts->slot2_on == 1) 
        fprintf (stderr, " *MUTED*"); 
      else if (opts->payload == 0 && opts->slot2_on == 0) 
        fprintf (stderr, " *OFF*");
    }

    state->debug_audio_errorsR += state->errs2R;

    if (state->dmr_encR == 0 || opts->dmr_mute_encR == 0)
    {
      if ( opts->floating_point == 0) //opts->audio_out == 1 && //needed to remove for AERO OSS so we could still save wav files during dual voices
      {
        #ifdef AERO_BUILD
        if(opts->audio_out == 1 && opts->slot2_on == 1) //add conditional check here, otherwise some lag occurs on dual voices with OSS48k/1 input due to buffered audio
        #endif
        processAudioR(opts, state);
      }
      if (opts->audio_out == 1 && opts->floating_point == 0 && opts->audio_out_type == 5 && opts->slot2_on == 1) //for OSS 48k 1 channel configs -- relocate later if possible
      {
        playSynthesizedVoiceMSR (opts, state);
      }
    }

    memcpy (state->f_r, state->audio_out_temp_bufR, sizeof(state->f_r));

  }

  //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
  if (opts->dmr_mono == 0 && opts->dmr_stereo == 0 && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0) )
  {
    state->debug_audio_errors += state->errs2;
    if (opts->audio_out == 1 && opts->floating_point == 0 ) //&& opts->pulse_digi_rate_out == 8000
    {
      processAudio(opts, state);
    }
  //   if (opts->audio_out == 1)
  //   {
  //     playSynthesizedVoice (opts, state);
  //   }

      memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l)); //P25p1 FDMA 8k/1 channel -f1 switch
  }

  //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
  if (opts->wav_out_f != NULL && opts->dmr_stereo == 0 && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0))
  {
    writeSynthesizedVoice (opts, state);
  }

  //per call can only be used when ncurses terminal is active since we use its call history matrix for when to record
  if (opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 0) //opts->use_ncurses_terminal == 1 &&
  {
    if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0)
    {
      //write wav to per call on left channel Slot 1
      writeSynthesizedVoice (opts, state);
    }
  }

  //per call can only be used when ncurses terminal is active since we use its call history matrix for when to record
  if (opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 1) //opts->use_ncurses_terminal == 1 &&
  {
    if (state->dmr_encR == 0 || opts->dmr_mute_encR == 0)
    {
      //write wav to per call on right channel Slot 2
      writeSynthesizedVoiceR (opts, state);
    }
  }

  if (preempt == 1)
  {
    opts->audio_out = 1;
    preempt = 0; 
  }

  //reset audio out flag for next repitition --disabled for now
  // if (strcmp(mode, "B") == 0) opts->audio_out = 1;

  //restore flag for null output type
  if (opts->audio_out_type == 9) opts->audio_out = 0;
}
