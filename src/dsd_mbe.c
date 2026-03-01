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
#include "bp.h"
#include "pc4.h"
#include "rc2.h"
#include "pc5.h"

//NOTE: This set of functions will be reorganized and simplified (hopefully) or at least
//a more logical flow will be established to jive with the new audio handling

void keyring(dsd_opts * opts, dsd_state * state)
{
  UNUSED(opts);

  if (state->currentslot == 0)
    state->R = state->rkey_array[state->payload_keyid];

  if (state->currentslot == 1)
    state->RR = state->rkey_array[state->payload_keyidR];

  //load any large keys (AES)
  if (state->currentslot == 0)
  {
    state->A1[0] = state->rkey_array[state->payload_keyid+0x000];
    state->A2[0] = state->rkey_array[state->payload_keyid+0x101];
    state->A3[0] = state->rkey_array[state->payload_keyid+0x201];
    state->A4[0] = state->rkey_array[state->payload_keyid+0x301];

    //check to see if there is a value loaded or not
    if (state->A1[0] == 0 && state->A2[0] == 0 && state->A3[0] == 0 && state->A4[0] == 0)
      state->aes_key_loaded[0] = 0;
    else state->aes_key_loaded[0] = 1;

  }

  if (state->currentslot == 1)
  {
    state->A1[1] = state->rkey_array[state->payload_keyidR+0x000];
    state->A2[1] = state->rkey_array[state->payload_keyidR+0x101];
    state->A3[1] = state->rkey_array[state->payload_keyidR+0x201];
    state->A4[1] = state->rkey_array[state->payload_keyidR+0x301];

    //check to see if there is a value loaded or not
    if (state->A1[1] == 0 && state->A2[1] == 0 && state->A3[1] == 0 && state->A4[1] == 0)
      state->aes_key_loaded[1] = 0;
    else state->aes_key_loaded[1] = 1;

  }

}

void playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv)
{

  int i;
  char imbe_d[88];
  char ambe_d[49];
  srand(time(NULL)); //random seed for some file names using random numbers in file name

  for (i = state->optind; i < argc; i++)
  {
    sprintf (opts->mbe_in_file, "%s", argv[i]);
    openMbeInFile (opts, state);
    mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
    fprintf (stderr, "\n playing %s\n", opts->mbe_in_file);
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

        //static wav file only, handled by playSynthesizedVoiceMS
        //NOTE: if using -o null, playSynthesizedVoiceMS will not write to static wav file
        //Per call will work, but will end up with a single file with no meta info
        if (opts->wav_out_f != NULL && (opts->dmr_stereo_wav == 1 || opts->static_wav_file == 1))
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
      else if (state->mbe_file_type == 3)
      {
        read_sdrtrunk_json_format (opts, state);
      }
      else if (state->mbe_file_type > 0) //ambe files
      {
        readAmbe2450Data (opts, state, ambe_d);
        int x;
        unsigned long long int k;
        if (state->K != 0) //apply Pr key
        {
          k = BPK[state->K];
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

        //static wav file only, handled by playSynthesizedVoiceMS
        //NOTE: if using -o null, playSynthesizedVoiceMS will not write to static wav file
        //Per call will work, but will end up with a single file with no meta info
        if (opts->wav_out_f != NULL && (opts->dmr_stereo_wav == 1 || opts->static_wav_file == 1))
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
    fclose(opts->mbe_in_f); //close file after playing it
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

  //keystream and silence conditional items
  uint64_t silence = 0xF801A99F8CE080; //AMBE+2 default silence vector expressed as a 56-bit hex value.
  char ambe_silence[49];
  for (i = 0; i < 49; i++)
    ambe_silence[i] = (silence >> (55-i)) & 1;
  //zeroed ambe_d can look like 00000000000580(w/ DMRA IV), 000D2C00000000, 
  //or 00000000000000 look at +24 position for 20 bits (fits all these scenarios)
  char zeroes[49]; memset(zeroes, 0, sizeof(zeroes));
  size_t zeroes_threshold = 20;

  if (state->forced_alg_id > 1 && state->forced_alg_id != 0x16) //1 and 0x16 is saved for BP stuff, so anything higher than that (Kirisun, requires svc opts set as well)
  {
    if (state->currentslot == 0 && state->dmr_so & 0x40)
    {
      state->payload_algid = state->forced_alg_id;
      state->payload_keyid = 0xFF;
    }
    if (state->currentslot == 1 && state->dmr_soR & 0x40)
    {
      state->payload_algidR = state->forced_alg_id;
      state->payload_keyidR = 0xFF;
    }
  }

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

    //P25p1 Multi Crypt Handler (DES1, DES3, DES-XL and AES)
    if ( (state->payload_algid == 0x81 && state->R != 0) || //DES-56
         (state->payload_algid == 0x9F && state->R != 0) || //DES-XL
         (state->payload_algid == 0x84 && state->aes_key_loaded[0] == 1) ||
         (state->payload_algid == 0x89 && state->aes_key_loaded[0] == 1) ||
         (state->payload_algid == 0x83 && state->aes_key_loaded[0] == 1) ) //3DES
    {
      uint8_t cipher[11];
      uint8_t plain[11];
      memset (cipher, 0, sizeof(cipher));
      memset (plain, 0, sizeof(plain));

      uint8_t aes_key[32];
      memset (aes_key, 0, sizeof(aes_key));

      //Load key from A1 - A4
      for (i = 0; i < 8; i++)
      {
        aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
        aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
        aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
        aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
      }

      if (state->p25vc == 0)
      {
        if (state->payload_algid == 0x81 || state->payload_algid == 0x83) //DES1 and DES3
          state->octet_counter = 11+8; //start on 19 for DES-OFB (8 discard + 8 LC + 3 reserved)
        else if (state->payload_algid == 0x9F)
          state->octet_counter = 11; //11 with info from LFSR run values (no discard)
        else
          state->octet_counter = 11+16; //start on 27 for AES (16 discard + 8 LC + 3 reserved)
        memset (state->ks_octetL, 0, sizeof(state->ks_octetL));

        //debug
        // fprintf (stderr, "\n KO: %d; ", state->octet_counter);

        if (state->payload_algid == 0x81) //DES-56
          des_multi_keystream_output (state->payload_miP, state->R, state->ks_octetL, 1, 28);
        if (state->payload_algid == 0x83) //3DES, or TDEA
          tdea_multi_keystream_output (state->payload_miP, aes_key, state->ks_octetL, 1, 28);
        if (state->payload_algid == 0x9F) //DES-XL
          des_multi_keystream_output (state->payload_miP, state->R, state->ks_octetL, 2, state->xl_is_hdu); //hard coded bit count value, xl_is_hdu determines lfsr run values
        if (state->payload_algid == 0x84) //AES256
          aes_ofb_keystream_output (state->aes_iv, aes_key, state->ks_octetL, 2, 14); //14 + 1 discard round
        if (state->payload_algid == 0x89) //AES128
          aes_ofb_keystream_output (state->aes_iv, aes_key, state->ks_octetL, 0, 14); //14 + 1 discard round

      }

      int z = 0; int j = 0;
      for (i = 0; i < 11; i++)
      {
        for (j = 0; j < 8; j++)
        {
          cipher[i] = cipher[i] << 1;
          cipher[i] = cipher[i] + imbe_d[z];
          imbe_d[z] = 0;
          z++;
        }
      }

      //debug output blocks
      // fprintf (stderr, "\n OB:");
      for (i = 0; i < 11; i++)
      {
        // fprintf (stderr, " %02X", state->ks_octetL[state->octet_counter]);
        plain[i] = cipher[i] ^ state->ks_octetL[state->octet_counter++];
      }

      z = 0;
      for (i = 0; i < 11; i++)
      {
        for (j = 0; j < 8; j++)
        {
          imbe_d[z] = (plain[i] & 0x80) >> 7;
          plain[i] = plain[i] << 1;
          z++;
        }

      }

    }

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

      rc4_voice_decrypt(state->dropL, 13, 11, rckey, cipher, plain);
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

    // if (opts->mbe_out_f != NULL && state->dmr_encL == 0) //only save if this bit not set
    if (opts->mbe_out_f != NULL) // && state->dmr_encL == 0) //only save if this bit not set //TODO: Fix this checkdown
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
      fprintf (stderr, " 7100");
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

    if ( (state->nxdn_cipher_type == 0x01 && state->R != 0) ||
          (state->forced_alg_id == 1 && state->R > 0) )
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

    //NXDN Generic Cipher 2 and Cipher 3 Keystream Application (to be tested)
    else if ( (state->nxdn_cipher_type == 0x02 && state->R != 0) ||
              (state->nxdn_cipher_type == 0x03 && state->aes_key_loaded[0] == 1) )
    {

      if (state->nxdn_cipher_type == 0x02 && state->nxdn_new_iv == 1 && state->nxdn_part_of_frame == 0)
			{

        //more debug
        // fprintf (stderr, " IV: %016llX; Key: %016llX", state->payload_miN, state->R);

        memset (state->ks_octetL, 0, sizeof(state->ks_octetL));
        memset (state->ks_bitstreamL, 0, sizeof(state->ks_bitstreamL));

				des_multi_keystream_output (state->payload_miN, state->R, state->ks_octetL, 1, 26); //32 4V at 49 bits = 1568/64 = 24.5 + 1 discard block

				//reset bit_counter
				state->bit_counterL = 0;

				//unpack the octets into a bit-wise keystream
				unpack_byte_array_into_bit_array(state->ks_octetL+8, state->ks_bitstreamL, 26*8);

				//reset flag to 0
				state->nxdn_new_iv = 0;

			}

      //untested, but same setup as DES, so it 'SHOULD' work...maybe
      if (state->nxdn_cipher_type == 0x03 && state->nxdn_new_iv == 1 && state->nxdn_part_of_frame == 0)
			{

        //more debug
        // fprintf (stderr, " IV: %016llX; Key: %016llX", state->payload_miN, state->R);

        memset (state->ks_octetL, 0, sizeof(state->ks_octetL));
        memset (state->ks_bitstreamL, 0, sizeof(state->ks_bitstreamL));

				aes_ofb_keystream_output (state->aes_iv, state->aes_key, state->ks_octetL, 2, 15); //14 + 1 discard round

				//reset bit_counter
				state->bit_counterL = 0;

				//unpack the octets into a bit-wise keystream
				unpack_byte_array_into_bit_array(state->ks_octetL+8, state->ks_bitstreamL, 15*8);

				//reset flag to 0
				state->nxdn_new_iv = 0;

			}

      //sanity check, don't exceed bit application counter
      if (state->bit_counterL > (1568-49))
        state->bit_counterL = (1568-49);

      //Keystream creation is currently inside of the NXDN_decode_VCALL_IV function
      for (i = 0; i < 49; i++)
        ambe_d[i] ^= state->ks_bitstreamL[state->bit_counterL++];

      // fprintf (stderr, " bc: %04d;", state->bit_counterL); //debug to see what the counter value is up to currently (seems nominal)

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
      if (state->forced_alg_id == 0 && state->payload_algid == 0)
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
            (state->K > 0 && state->forced_alg_id == 1) )
      {
        k = BPK[state->K];
        k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
        for (short int j = 0; j < 48; j++) //49
        {
          x = ( ((k << j) & 0x800000000000) >> 47 );
          ambe_d[j] ^= x;
        }
      }

      if ( (state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68) ||
            (state->K1 > 0 && state->forced_alg_id == 1) )
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
      if (memcmp(ambe_d, ambe_silence, 49) == 0)
        pos += 49;
      else
      {
        for(i = 0; i < 49; i++)
        {
          ambe_d[i] ^= pN[pos];
          pos++;
        }
      }
      state->DMRvcL++;
      }

      //DMR and P25p2 DES-OFB 56 Handling, Slot 1, VCH 0 -- consider moving into the AES handler
      if ( (state->currentslot == 0 && state->payload_algid == 0x22 && state->R != 0) ||
           (state->currentslot == 0 && state->payload_algid == 0x81 && state->R != 0)   )
      {

        //sanity check in case of error
        if (state->DMRvcL > 17) state->DMRvcL = 17;

        int j; int z = 0; int n = 8; uint8_t b = 0;
        if (state->DMRvcL == 0)
        {
          memset (state->ks_octetL, 0, sizeof(state->ks_octetL));
          memset (state->ks_bitstreamL, 0, sizeof(state->ks_bitstreamL));
          state->bit_counterL = 0;

          //check all these comments I'm making for accuracy
          des_multi_keystream_output (state->payload_miP, state->R, state->ks_octetL, 1, 19); //18 + 1

          //Load Keystream Octet Bytes directly into a bit array, that way we don't have
          //to keep track of the byte positions and masks for the 49th bit
          for (i = 0; i < 18 * 8; i++) //19 blocks minus 1 discard block at 8 bits each
          {
            for (j = 0; j < 8; j++)
            {
              b = (( (state->ks_octetL[n] << j) & 0x80) >> 7);
              state->ks_bitstreamL[z++] = b;
            }
            n++;
          }
        }

        //now we do the bit by bit xor depending on the frame and position of the state bit counter
        //run 6 instead of 7 so we can just do bit 49 outside of loop to keep extra bits overloading the array
        z = 0;
        for (i = 0; i < 6; i++)
        {
          for (j = 0; j < 8; j++)
            ambe_d[z++] ^= state->ks_bitstreamL[state->bit_counterL++];
        }

        //last bit
        ambe_d[48] ^= state->ks_bitstreamL[state->bit_counterL++];

        //skip the next 7 bits of the array
        state->bit_counterL += 7;

        //increment vc counter by one
        state->DMRvcL++;

      }

      //DMR and P25p2 AES 256, Slot 1, VCH 0 -- need a way to make sure we have a key when zero fill on some parts of it //&& state->aes_key_loaded[0] == 1
      if (  (state->currentslot == 0 && state->payload_algid == 0x24 && state->aes_key_loaded[0] == 1 ) || //DMR AES128
            (state->currentslot == 0 && state->payload_algid == 0x25 && state->aes_key_loaded[0] == 1 ) || //DMR AES256
            (state->currentslot == 0 && state->payload_algid == 0x89 && state->aes_key_loaded[0] == 1 ) || //P25 AES128
            (state->currentslot == 0 && state->payload_algid == 0x84 && state->aes_key_loaded[0] == 1 ) || //P25 AES256
            (state->currentslot == 0 && state->payload_algid == 0x36 && state->aes_key_loaded[0] == 1 ) || //KIRI ADV
            (state->currentslot == 0 && state->payload_algid == 0x37 && state->aes_key_loaded[0] == 1 ) || //KIRI UNI
            (state->currentslot == 0 && state->payload_algid == 0x02 && state->R != 0 )                  ) //HYT ENHANCED
      {

        int j; int z = 0; int n = 16; uint8_t b = 0; //n=16 for AES-OFB discard round
        uint8_t aes_key[32];
        memset (aes_key, 0, sizeof(aes_key));

        //Load key from A1 - A4
        for (i = 0; i < 8; i++)
        {
          aes_key[i+0]  = (state->A1[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+8]  = (state->A2[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+16] = (state->A3[0] >> (56-(i*8))) & 0xFF;
          aes_key[i+24] = (state->A4[0] >> (56-(i*8))) & 0xFF;
        }

        //sanity check in case of error
        if (state->DMRvcL > 17) state->DMRvcL = 17;

        if (state->DMRvcL == 0)
        {
          memset (state->ks_octetL, 0, sizeof(state->ks_octetL));
          memset (state->ks_bitstreamL, 0, sizeof(state->ks_bitstreamL));
          state->bit_counterL = 0;

          if(state->payload_algid == 0x24 || state->payload_algid == 0x89) //AES128
            aes_ofb_keystream_output (state->aes_iv, aes_key, state->ks_octetL, 0, 10); //9 + 1 discard round
          if(state->payload_algid == 0x25 || state->payload_algid == 0x84) //AES256
            aes_ofb_keystream_output (state->aes_iv, aes_key, state->ks_octetL, 2, 10); //9 + 1 discard round
          if(state->payload_algid == 0x02)
          {
            n = 0;
            hytera_enhanced_rc4_setup(opts, state, state->R, state->payload_mi);
          }
          if (state->payload_algid == 0x36)
          {
            n = 0;
            kirisun_adv_keystream_creation(state);
          }
          if (state->payload_algid == 0x37)
          {
            n = 0;
            kirisun_uni_keystream_creation(state);
          }

          //Load Keystream Octet Bytes directly into keystream array //TODO: Convert to unpack function
          for (i = 0; i < 9 * 16; i++) //9 rounds at 16 octets
          {
            for (j = 0; j < 8; j++)
            {
              b = (( (state->ks_octetL[n] << j) & 0x80) >> 7);
              state->ks_bitstreamL[z++] = b;
            }
            n++;
          }
        }

        //skip keystream if silence or zeroes (some CCR), else apply keystream directly and increment counter
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->bit_counterL += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->bit_counterL += 49;
        else
        {
          for (i = 0; i < 49; i++)
            ambe_d[i] ^= state->ks_bitstreamL[state->bit_counterL++];
        }

        //skip the next 7 bits of the array (if not Hytera Enhanced)
        if(state->payload_algid != 0x02)
          state->bit_counterL += 7;

        //increment vc counter by one
        state->DMRvcL++;

        opts->dmr_mute_encL = 0; //shim to unmute

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

        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->dropL += 7;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->dropL += 7;
        else
        {
          if (state->errs < 3)
            rc4_voice_decrypt(state->dropL, 9, 7, rckey, cipher, plain);
          else memcpy (plain, cipher, sizeof(plain));

          state->dropL += 7;

          //unpack deciphered plain array back into ambe_d bit array
          memset (ambe_d, 0, 49*sizeof(char));
          unpack_ambe(plain, ambe_d);

        }

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

        rc4_voice_decrypt(state->dropL, 13, 7, rckey, cipher, plain);
        state->dropL += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }
      
      //DMR Retevis AP, Either Slot (static single key'd enforced KS)
      if (state->retevis_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          uint8_t frame1_cipher[49];
    
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
    
          decrypt_rc2((CryptoContext *)state->rc2_context, frame1_cipher);
          
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = frame1_cipher[i];
        }

      }

      //DMR TYT AP, Either Slot (static single key'd enforced KS)
      if (state->tyt_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          short frame1_cipher[49];
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
          decrypt_frame_49(frame1_cipher);
  
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = ctx.bits[i];
        }

      }

      //DMR BAOFENG AP, Either Slot (static single key'd enforced KS)
      if (state->baofeng_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          short frame1_cipher[49];
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
          
          decrypt_frame_49_pc5(frame1_cipher);
          
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = ctxpc5.bits[i];
        }
      }

      //DMR TYT EP, Either Slot (static single key'd enforced KS)
      if (state->tyt_ep == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(ctx.bits[i] & 1);
        }
      }

      //DMR Kenwood Scrambler, Either Slot (static single key'd enforced KS)
      if (state->ken_sc == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%882] & 1); //Yikes!
        }
      }

      //DMR Anytone BP, Either Slot (static single key'd enforced KS)
      if (state->any_bp == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%16] & 1); //Yikes!
        }
      }

      //Generic Straight Static Keystream
      if (state->straight_ks == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          //disable enc identifiers, if present
          state->dmr_so = 0;
          state->payload_algid = 0;
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%state->straight_mod] & 1); //Yikes!
        }
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
      if (state->forced_alg_id == 0 && state->payload_algidR == 0)
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
            (state->K > 0 && state->forced_alg_id == 1) )
      {
        k = BPK[state->K];
        k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
        for (short int j = 0; j < 48; j++)
        {
          x = ( ((k << j) & 0x800000000000) >> 47 );
          ambe_d[j] ^= x;
        }
      }

      if ( (state->K1 > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fidR == 0x68) ||
            (state->K1 > 0 && state->forced_alg_id == 1))
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
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          pos += 49;
        else
        {
          for(i = 0; i < 49; i++)
          {
            ambe_d[i] ^= pN[pos];
            pos++;
          }
        }
        state->DMRvcR++;
      }

      //DMR and P25p2 DES-OFB 56 Handling, Slot 2, VCH 1 -- Consider moving into AES handler
      if ( (state->currentslot == 1 && state->payload_algidR == 0x22 && state->RR != 0) ||
           (state->currentslot == 1 && state->payload_algidR == 0x81 && state->RR != 0)   )
      {

        //sanity check in case of error
        if (state->DMRvcR > 17) state->DMRvcR = 17;

        int j; int z = 0; int n = 8; uint8_t b = 0;
        if (state->DMRvcR == 0)
        {
          memset (state->ks_octetR, 0, sizeof(state->ks_octetR));
          memset (state->ks_bitstreamR, 0, sizeof(state->ks_bitstreamR));
          state->bit_counterR = 0;

          //check all these comments I'm making for accuracy
          des_multi_keystream_output (state->payload_miN, state->RR, state->ks_octetR, 1, 19); //18 + 1

          //Load Keystream Octet Bytes directly into a bit array, that way we don't have
          //to keep track of the byte positions and masks for the 49th bit
          for (i = 0; i < 18 * 8; i++) //19 blocks minus 1 discard block at 8 bits each
          {
            for (j = 0; j < 8; j++)
            {
              b = (( (state->ks_octetR[n] << j) & 0x80) >> 7);
              state->ks_bitstreamR[z++] = b;
            }
            n++;
          }
        }

        //now we do the bit by bit xor depending on the frame and position of the state bit counter
        //run 6 instead of 7 so we can just do bit 49 outside of loop to keep extra bits overloading the array
        z = 0;
        for (i = 0; i < 6; i++)
        {
          for (j = 0; j < 8; j++)
            ambe_d[z++] ^= state->ks_bitstreamR[state->bit_counterR++];
        }

        //last bit
        ambe_d[48] ^= state->ks_bitstreamR[state->bit_counterR++];

        //skip the next 7 bits of the array
        state->bit_counterR += 7;

        //increment vc counter by one
        state->DMRvcR++;

      }

      //DMR and P25p2 AES, Slot 2, VCH 1
      if (  (state->currentslot == 1 && state->payload_algidR == 0x24 && state->aes_key_loaded[1] == 1 ) || //DMR AES128
            (state->currentslot == 1 && state->payload_algidR == 0x25 && state->aes_key_loaded[1] == 1 ) || //DMR AES256
            (state->currentslot == 1 && state->payload_algidR == 0x89 && state->aes_key_loaded[1] == 1 ) || //P25 AES128
            (state->currentslot == 1 && state->payload_algidR == 0x84 && state->aes_key_loaded[1] == 1 ) || //P25 AES256
            (state->currentslot == 1 && state->payload_algidR == 0x36 && state->aes_key_loaded[1] == 1 ) || //KIRI ADV
            (state->currentslot == 1 && state->payload_algidR == 0x37 && state->aes_key_loaded[1] == 1 ) || //KIRI UNI
            (state->currentslot == 1 && state->payload_algidR == 0x02 && state->RR != 0 )                 ) //HYT ENHANCED
      {

        int j; int z = 0; int n = 16; uint8_t b = 0; //n=16 for AES-OFB discard round
        uint8_t aes_key[32];
        memset (aes_key, 0, sizeof(aes_key));

        //Load key from A1 - A4
        for (i = 0; i < 8; i++)
        {
          aes_key[i+0]  = (state->A1[1] >> (56-(i*8))) & 0xFF;
          aes_key[i+8]  = (state->A2[1] >> (56-(i*8))) & 0xFF;
          aes_key[i+16] = (state->A3[1] >> (56-(i*8))) & 0xFF;
          aes_key[i+24] = (state->A4[1] >> (56-(i*8))) & 0xFF;
        }

        //sanity check in case of error
        if (state->DMRvcR > 17) state->DMRvcR = 17;

        if (state->DMRvcR == 0)
        {
          memset (state->ks_octetR, 0, sizeof(state->ks_octetR));
          memset (state->ks_bitstreamR, 0, sizeof(state->ks_bitstreamR));
          state->bit_counterR = 0;

          if (state->payload_algidR == 0x24 || state->payload_algidR == 0x89)
            aes_ofb_keystream_output (state->aes_ivR, aes_key, state->ks_octetR, 0, 10); //9 + 1 discard round
          if (state->payload_algidR == 0x25 || state->payload_algidR == 0x84)
            aes_ofb_keystream_output (state->aes_ivR, aes_key, state->ks_octetR, 2, 10); //9 + 1 discard round
          if(state->payload_algidR == 0x02)
          {
            n = 0;
            hytera_enhanced_rc4_setup(opts, state, state->RR, state->payload_miR);
          }
          if (state->payload_algidR == 0x36)
          {
            n = 0;
            kirisun_adv_keystream_creation(state);
          }
          if (state->payload_algidR == 0x37)
          {
            n = 0;
            kirisun_uni_keystream_creation(state);
          }

          //Load Keystream Octet Bytes directly into keystream array
          for (i = 0; i < 9 * 16; i++) //9 rounds at 16 octets //TODO: Convert to unpack function
          {
            for (j = 0; j < 8; j++)
            {
              b = (( (state->ks_octetR[n] << j) & 0x80) >> 7);
              state->ks_bitstreamR[z++] = b;
            }
            n++;
          }
        }

        //skip keystream if silence or zeroes (some CCR), else apply keystream directly and increment counter
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->bit_counterR += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->bit_counterR += 49;
        else
        {
          for (i = 0; i < 49; i++)
            ambe_d[i] ^= state->ks_bitstreamR[state->bit_counterR++];
        }

        //skip the next 7 bits of the array (if not Hytera Enhanced)
        if(state->payload_algidR != 0x02)
          state->bit_counterR += 7;

        //increment vc counter by one
        state->DMRvcR++;

        opts->dmr_mute_encR = 0; //shim to unmute

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
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->dropR += 7;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->dropR += 7;
        else
        {
          if (state->errsR < 3)
            rc4_voice_decrypt(state->dropR, 9, 7, rckey, cipher, plain);
          else memcpy (plain, cipher, sizeof(plain));
          state->dropR += 7;

          //unpack deciphered plain array back into ambe_d bit array
          memset (ambe_d, 0, 49*sizeof(char));
          unpack_ambe(plain, ambe_d);

        }

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

        rc4_voice_decrypt(state->dropR, 13, 7, rckey, cipher, plain);
        state->dropR += 7;

        //unpack deciphered plain array back into ambe_d bit array
        memset (ambe_d, 0, 49*sizeof(char));
        unpack_ambe(plain, ambe_d);

      }

      //DMR Retevis AP, Either Slot (static single key'd enforced KS)
      if (state->retevis_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          uint8_t frame1_cipher[49];
    
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
    
          decrypt_rc2((CryptoContext *)state->rc2_context, frame1_cipher);
          
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = frame1_cipher[i];
        }

      }

      //DMR TYT AP, Either Slot (static single key'd enforced KS)
      if (state->tyt_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          short frame1_cipher[49];
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
          decrypt_frame_49(frame1_cipher);
  
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = ctx.bits[i];
        }

      }

      //DMR BAOFENG AP, Either Slot (static single key'd enforced KS)
      if (state->baofeng_ap == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          short frame1_cipher[49];
          for (int i = 0; i < 49; i++) frame1_cipher[i] = ambe_d[i];
          
          decrypt_frame_49_pc5(frame1_cipher);
          
          memset (ambe_d, 0, 49*sizeof(char));
          for (int i = 0; i < 49; i++) ambe_d[i] = ctxpc5.bits[i];
        }

      }

      //DMR TYT EP, Either Slot (static single key'd enforced KS)
      if (state->tyt_ep == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0) {}
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0) {}
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(ctx.bits[i] & 1);
        }
      }

      //DMR Kenwood Scrambler, Either Slot (static single key'd enforced KS) //should probably break this up, but this is a test for now
      if (state->ken_sc == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%882] & 1); //Yikes!
        }
      }

      //DMR Anytone BP, Either Slot (static single key'd enforced KS)
      if (state->any_bp == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%16] & 1); //Yikes!
        }
      }

      //Generic Straight Static Keystream
      if (state->straight_ks == 1)
      {
        if (memcmp(ambe_d, ambe_silence, 49) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else if (memcmp(ambe_d+24, zeroes+24, zeroes_threshold) == 0)
          state->static_ks_counter[state->currentslot] += 49;
        else
        {
          //disable enc identifiers, if present
          state->dmr_soR = 0;
          state->payload_algidR = 0;
          for (int i = 0; i < 49; i++)
            ambe_d[i] ^= (uint8_t)(state->static_ks_bits[state->currentslot][(state->static_ks_counter[state->currentslot]++)%state->straight_mod] & 1); //Yikes!
        }
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

    if (state->ken_sc == 1)
      state->dmr_encL = 0;

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
        #ifdef __CYGWIN__
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

    if (state->ken_sc == 1)
      state->dmr_encR = 0;

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
        #ifdef __CYGWIN__
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

  //still need this for any switch that opens a 1 channel output config
  if (opts->static_wav_file == 0)
  {
    //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
    if (opts->wav_out_f != NULL && opts->dmr_stereo == 0 && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0))
    {
      writeSynthesizedVoice (opts, state);
    }
  }

  //per call wav file writing for slot 1
  if (opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 0)
  {
    if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0)
    {
      //write wav to per call on left channel Slot 1
      writeSynthesizedVoice (opts, state);
    }
  }

  //per call wav file writing for slot 2
  if (opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 1)
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
