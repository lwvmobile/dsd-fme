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

void
playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv)
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
              processAudio (opts, state);
              if (opts->wav_out_f != NULL)
                {
                  writeSynthesizedVoice (opts, state);
                }

              if (opts->audio_out == 1)
                {

                  playSynthesizedVoice (opts, state);

                }
            }
          else if (state->mbe_file_type == 1)
            {
              readAmbe2450Data (opts, state, ambe_d);
              mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
              processAudio (opts, state);
              if (opts->wav_out_f != NULL)
                {
                  writeSynthesizedVoice (opts, state);
                }
              if (opts->audio_out == 1)
                {
                  playSynthesizedVoice (opts, state);
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

  //
  int BP[256] = {
    0x0000, 0x1F00, 0xE300, 0xFC00, 0x2503, 0x3A03, 0xC603, 0xD903,
    0x4A05, 0x5505, 0xA905, 0xB605, 0x6F06, 0x7006, 0x8C06, 0x9306,
    0x2618, 0x3918, 0xC518, 0xDA18, 0x031B, 0x1C1B, 0xE01B, 0xFF1B,
    0x6C1D, 0x731D, 0x8F1D, 0x901D, 0x491E, 0x561E, 0xAA1E, 0xB51E, //31
    0x4B28, 0x5428, 0xA828, 0xB728, 0x6E2B, 0x712B, 0x8D2B, 0x922B,
    0x012D, 0x1E2D, 0xE22D, 0xFD2D, 0x242E, 0x3B2E, 0xC72E, 0xD82E,
    0x6D30, 0x7230, 0x8E30, 0x9130, 0x4833, 0x5733, 0xAB33, 0xB433,
    0x2735, 0x3835, 0xC435, 0xDB35, 0x0236, 0x1D36, 0xE136, 0xFE36, //63
    0x2B49, 0x3449, 0xC849, 0xD749, 0x0E4A, 0x114A, 0xED4A, 0xF24A,
    0x614C, 0xAE4C, 0x824C, 0x9D4C, 0x444F, 0x5B4F, 0xA74F, 0xB84F,
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
  //

	int i;
  char imbe_d[88];
  char ambe_d[49];
  char ambe_d_str[50];
  unsigned long long int k;
  int x;


  for (i = 0; i < 88; i++)
    {
      imbe_d[i] = 0;
    }

    for (i = 0; i < 49; i++)
      {
        ambe_d[i] = 0;
      }


  if ((state->synctype == 0) || (state->synctype == 1))
    {
      //  0 +P25p1
      //  1 -P25p1
			state->errs = mbe_eccImbe7200x4400C0 (imbe_fr);
			state->errs2 = state->errs;
			mbe_demodulateImbe7200x4400Data (imbe_fr);
			state->errs2 += mbe_eccImbe7200x4400Data (imbe_fr, imbe_d);



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
			mbe_convertImbe7100to7200(imbe_d); //needs extra conversion step apparently

			mbe_processImbe4400Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
																imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

			//below line the old way of doing this
      // mbe_processImbe7100x4400Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, imbe7100_fr, imbe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
      if (opts->payload == 1)
      {
        PrintIMBEData (opts, state, imbe_d);
      }
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
	else if ((state->synctype == 8) || (state->synctype == 9))
	{
		state->errs = mbe_eccAmbe3600x2450C0 (ambe_fr);
		state->errs2 = state->errs;
		mbe_demodulateAmbe3600x2450Data (ambe_fr);
		state->errs2 += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);


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
      if (state->currentslot == 0 && opts->dmr_stereo == 1)
      {

        state->errs = mbe_eccAmbe3600x2450C0 (ambe_fr);
        state->errs2 = state->errs;
        mbe_demodulateAmbe3600x2450Data (ambe_fr);
        state->errs2 += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);

		    if (state->K > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x10)
        {
          k = BP[state->K];
          k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
          for (short int j = 0; j < 48; j++) //49
          {
            x = ( ((k << j) & 0x800000000000) >> 47 );
            ambe_d[j] ^= x;
          }
        }

        if (state->currentslot == 0 && state->H > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68)
				{
					unsigned long long int msb = 0;
					if (state->DMRvcL == 0)
					{
						state->HYTL = state->H;
						k = state->H;

						k = k << 8;
						k = k | ((k >> 40) & 0xFF);
						state->HYTL = k;
					}

					k = state->HYTL;
  	     	for(short int j = 0; j < 48; j++)
		     	{
			      x = ( ((k << j) & 0x800000000000) >> 47 );
			      ambe_d[j] ^= x;
			    }

					k = k << 1;
					msb = (k >> 32) & 0xFFFF; //new MSB
					k = (k << 8) & 0xFFFFFFFF0000;
					k = k | msb;

					state->HYTL = k;
					state->DMRvcL++;

				}

      	mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
                                	ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);


        //old method for this step below
        //mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
        if (opts->payload == 1) // && state->R == 0 this is why slot 1 didn't primt abme, probably had it set during testing
        {
          PrintAMBEData (opts, state, ambe_d);
        }
      }
      //stereo slots and slot 1 (right slot)
      if (state->currentslot == 1 && opts->dmr_stereo == 1)
      {

        state->errsR = mbe_eccAmbe3600x2450C0 (ambe_fr);
        state->errs2R = state->errsR;
        mbe_demodulateAmbe3600x2450Data (ambe_fr);
        state->errs2R += mbe_eccAmbe3600x2450Data (ambe_fr, ambe_d);


        if (state->K > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fid == 0x10)
        {
          k = BP[state->K];
          k = ( ((k & 0xFF0F) << 32 ) + (k << 16) + k );
          for (short int j = 0; j < 48; j++)
          {
            x = ( ((k << j) & 0x800000000000) >> 47 );
            ambe_d[j] ^= x;
          }
        }

        if (state->H > 0 && state->dmr_soR & 0x40 && state->payload_keyidR == 0 && state->dmr_fidR == 0x68)
				{
					unsigned long long int msb = 0;
					if (state->DMRvcR == 0)
					{
						state->HYTR = state->H;
						k = state->H;

						k = k << 8;
						k = k | ((k >> 40) & 0xFF);
						state->HYTR = k;
					}

					k = state->HYTR;
  	     	for(short int j = 0; j < 48; j++)
		     	{
			      x = ( ((k << j) & 0x800000000000) >> 47 );
			      ambe_d[j] ^= x;
			    }

					k = k << 1;
					msb = (k >> 32) & 0xFFFF;
					k = (k << 8) & 0xFFFFFFFF0000;
					k = k | msb;

					state->HYTR = k;
					state->DMRvcR++;

				}

				mbe_processAmbe2450Dataf (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR,
	                                ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);

        //old method for this step below
        //mbe_processAmbe3600x2450Framef (state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR, ambe_fr, ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2, opts->uvquality);
        if (opts->payload == 1)
        {
          PrintAMBEData (opts, state, ambe_d);
        }
      }

      //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
      if (opts->dmr_stereo == 0)
      {
        mbe_processAmbe3600x2450Framef (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_fr, ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);
        if (opts->payload == 1)
        {
          PrintAMBEData (opts, state, ambe_d);
        }
        //only save MBE files if not enc or unmuted, THIS does not seem to work for some reason
        if (opts->mbe_out_f != NULL && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0) )
        {
          saveAmbe2450Data (opts, state, ambe_d);
        }
      }


    }

  //quick enc check to determine whether or not to play enc traffic
  int enc_bit = 0;
  //end enc check

  if (opts->dmr_stereo == 1 && state->currentslot == 0)
  {
    enc_bit = (state->dmr_so >> 6) & 0x1;
    if (enc_bit == 1)
    {
      state->dmr_encL = 1;
    }
    else state->dmr_encL = 0;
    if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0)
    {
      state->debug_audio_errors += state->errs2;
      processAudio (opts, state);
      if (opts->audio_out == 1)
      {
        playSynthesizedVoice (opts, state);
      }
    }
  }

  if (opts->dmr_stereo == 1 && state->currentslot == 1)
  {
    enc_bit = (state->dmr_soR >> 6) & 0x1;
    if (enc_bit == 0x1)
    {
      state->dmr_encR = 1;
    }
    else state->dmr_encR = 0;
    if (state->dmr_encR == 0 || opts->dmr_mute_encR == 0)
    {
      state->debug_audio_errorsR += state->errs2R;
      processAudioR (opts, state);
      if (opts->audio_out == 1)
      {
        playSynthesizedVoiceR (opts, state);
      }
    }
  }

  //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
  if (opts->dmr_stereo == 0 && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0) )
  {
    state->debug_audio_errors += state->errs2;
    processAudio (opts, state);
    if (opts->audio_out == 1)
    {
      playSynthesizedVoice (opts, state);
    }
  }

  //if using anything but DMR Stereo, borrowing state->dmr_encL to signal enc or clear for other types
  if (opts->wav_out_f != NULL && opts->dmr_stereo == 0 && (opts->unmute_encrypted_p25 == 1 || state->dmr_encL == 0))
  {
    writeSynthesizedVoice (opts, state);
  }

	//per call can only be used when ncurses terminal is active since we use its call history matrix for when to record
	if (opts->use_ncurses_terminal == 1 && opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 0)
	{
    if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0)
    {
	    //write wav to per call on left channel Slot 1
	    writeSynthesizedVoice (opts, state);
    }
  }

	//per call can only be used when ncurses terminal is active since we use its call history matrix for when to record
	if (opts->use_ncurses_terminal == 1 && opts->dmr_stereo_wav == 1 && opts->dmr_stereo == 1 && state->currentslot == 1)
	{
    if (state->dmr_encR == 0 || opts->dmr_mute_encR == 0)
    {
	    //write wav to per call on right channel Slot 2
	    writeSynthesizedVoiceR (opts, state);
    }
	}


}
