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
#include <locale.h>

void
printFrameSync (dsd_opts * opts, dsd_state * state, char *frametype, int offset, char *modulation)
{
  UNUSED3(state, offset, modulation);

  char * timestr = getTimeC();
  if (opts->verbose > 0)
  {
    fprintf (stderr,"%s ", timestr);
    fprintf (stderr,"Sync: %s ", frametype);
  }

  //oops, that made a nested if-if-if-if statement,
  //causing a memory leak
  
  // if (opts->verbose > 2)
    //fprintf (stderr,"o: %4i ", offset);
  // if (opts->verbose > 1)
    //fprintf (stderr,"mod: %s ", modulation); 
  // if (opts->verbose > 2)
    //fprintf (stderr,"g: %f ", state->aout_gain);

  if (timestr != NULL)
  {
    free (timestr);
    timestr = NULL;
  }

}

int
getFrameSync (dsd_opts * opts, dsd_state * state)
{
  /* detects frame sync and returns frame type
   *  0 = +P25p1
   *  1 = -P25p1
   *  2 = +X2-TDMA (non inverted signal data frame)
   *  3 = -X2-TDMA (inverted signal voice frame)
   *  4 = +X2-TDMA (non inverted signal voice frame)
   *  5 = -X2-TDMA (inverted signal data frame)
   *  6 = +D-STAR
   *  7 = -D-STAR
   *  8 = +M17 STR (non inverted stream frame)
   *  9 = -M17 STR (inverted stream frame)
   * 10 = +DMR (non inverted signal data frame)
   * 11 = -DMR (inverted signal voice frame)
   * 12 = +DMR (non inverted signal voice frame)
   * 13 = -DMR (inverted signal data frame)
   * 14 = +ProVoice
   * 15 = -ProVoice
   * 16 = +M17 LSF (non inverted link frame)
   * 17 = -M17 LSF (inverted link frame)
   * 18 = +D-STAR_HD
   * 19 = -D-STAR_HD
   * 20 = +dPMR Frame Sync 1
   * 21 = +dPMR Frame Sync 2
   * 22 = +dPMR Frame Sync 3
   * 23 = +dPMR Frame Sync 4
   * 24 = -dPMR Frame Sync 1
   * 25 = -dPMR Frame Sync 2
   * 26 = -dPMR Frame Sync 3
   * 27 = -dPMR Frame Sync 4
   * 28 = +NXDN (sync only)
   * 29 = -NXDN (sync only)
   * 30 = +YSF
   * 31 = -YSF
   * 32 = DMR MS Voice
   * 33 = DMR MS Data
   * 34 = DMR RC Data
   * 35 = +P25 P2
   * 36 = -P25 P2
   * 37 = +EDACS
   * 38 = -EDACS
   */


  //start control channel hunting if using trunking, time needs updating on each successful sync
  //will need to assign frequencies to a CC array for P25 since that isn't imported from CSV
  if (state->dmr_rest_channel == -1 && opts->p25_is_tuned == 0 && opts->p25_trunk == 1 && ( (time(NULL) - state->last_cc_sync_time) > (opts->trunk_hangtime + 0) ) ) //was 3, go to hangtime value
  {

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      //re-enable both slots
      opts->slot1_on = 1;
      opts->slot2_on = 1;
    }

    //start going through the lcn/frequencies CC/signal hunting
    fprintf (stderr, "Control Channel Signal Lost. Searching for Control Channel.\n");
    //make sure our current roll value doesn't exceed value of frequencies imported
    if (state->lcn_freq_roll >= state->lcn_freq_count) //fixed this to skip the extra wait out at the end of the list
    {
      state->lcn_freq_roll = 0; //reset to zero
    }
    //roll an extra value up if the current is the same as what's already loaded -- faster hunting on Cap+, etc
    if (state->lcn_freq_roll != 0)
    {
      if (state->trunk_lcn_freq[state->lcn_freq_roll-1] == state->trunk_lcn_freq[state->lcn_freq_roll])
      {
        state->lcn_freq_roll++;
        //check roll again if greater than expected, then go back to zero
        if (state->lcn_freq_roll >= state->lcn_freq_count) 
        {
          state->lcn_freq_roll = 0; //reset to zero
        } 
      }
    }
    //check that we have a non zero value first, then tune next frequency
    if (state->trunk_lcn_freq[state->lcn_freq_roll] != 0) 
    {
      //rigctl
      if (opts->use_rigctl == 1)
      {
        if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
        SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[state->lcn_freq_roll]);
      }
      //rtl
      if (opts->audio_in_type == 3)
      {
        #ifdef USE_RTLSDR
        rtl_dev_tune (opts, state->trunk_lcn_freq[state->lcn_freq_roll]);
        #endif
      }
      
      fprintf (stderr, "Tuning to Frequency: %.06lf MHz\n", 
                (double)state->trunk_lcn_freq[state->lcn_freq_roll]/1000000);

    }
    state->lcn_freq_roll++;
    state->last_cc_sync_time = time(NULL); //set again to give another x seconds
  }

  int i, t, dibit, sync, symbol, synctest_pos, lastt;
  char synctest[25];
  char synctest12[13]; //dPMR
  char synctest10[11]; //NXDN FSW only
  char synctest32[33];
  char synctest20[21]; //YSF
  char synctest48[49]; //EDACS
  char synctest8[9];   //M17
  char synctest16[17]; //M17 Preamble
  char modulation[8];
  char *synctest_p;
  char synctest_buf[10240]; //what actually is assigned to this, can't find its use anywhere?
  int lmin, lmax, lidx;

  //assign t_max value based on decoding type expected (all non-auto decodes first)
  int t_max = 24; //initialize as an actual value to prevent any overflow related issues
  if (opts->frame_nxdn48 == 1 || opts->frame_nxdn96 == 1)
  {
    t_max = 10;
  }
  //dPMR
  else if (opts->frame_dpmr == 1)
  {
    t_max = 12; //based on Frame_Sync_2 pattern
  }
  else if (opts->frame_m17 == 1)
  {
    t_max = 8;
  }
  else if (state->lastsynctype == 30 || state->lastsynctype == 31 )
  {
    t_max = 20; //20 on YSF
  }
  //if Phase 2, then only 19
  else if (state->lastsynctype == 35 || state->lastsynctype == 36 )
  {
    t_max = 19; //Phase 2 S-ISCH is only 19
  }
  else if (opts->analog_only == 1) //shim to make sure this is set to a reasonable value
  {
    t_max = 24;
  }
  else t_max = 24; //24 for everything else

  int lbuf[48], lbuf2[48]; //if we use t_max in these arrays, and t >=  t_max in condition below, then it can overflow those checks in there if t exceeds t_max
  int lsum;
  //init the lbuf
  memset (lbuf, 0, sizeof(lbuf));
  memset (lbuf2, 0, sizeof(lbuf2));

  // detect frame sync
  t = 0;
  synctest10[10] = 0; 
  synctest[24] = 0;
  synctest8[8] = 0;   //M17, wasn't initialized or terminated (source of much pain and frustration in Cygwin)
  synctest12[12] = 0;
  synctest16[16] = 0; //M17, wasn't initialized or terminated (source of much pain and frustration in Cygwin)
  synctest48[48] = 0;
  synctest32[32] = 0;
  synctest20[20] = 0;
  modulation[7] = 0;  //not initialized or terminated (unsure if this would be an issue or not)
  synctest_pos = 0;
  synctest_p = synctest_buf + 10;
  sync = 0;
  lmin = 0;
  lmax = 0;
  lidx = 0;
  lastt = 0;
  state->numflips = 0;

  //move ncursesPrinter outside of the sync loop, causes weird lag inside the loop
  if (opts->use_ncurses_terminal == 1)
  {
    ncursesPrinter(opts, state);
  }

  if ((opts->symboltiming == 1) && (state->carrier == 1))
    {
      //fprintf (stderr,"\nSymbol Timing:\n");
      //printw("\nSymbol Timing:\n");
    }
  while (sync == 0)
    {

      t++;

      symbol = getSymbol (opts, state, 0);

      lbuf[lidx] = symbol;
      state->sbuf[state->sidx] = symbol;
      if ( lidx == (t_max - 1) ) //23 //9 for NXDN
        {
          lidx = 0;
        }
      else
        {
          lidx++;
        }
      if (state->sidx == (opts->ssize - 1))
        {
          state->sidx = 0;
        }
      else
        {
          state->sidx++;
        }

      if (lastt == t_max) //issue for QPSK on shorter sync pattern? //23
        {
          lastt = 0;
          if (state->numflips > opts->mod_threshold)
            {
              if (opts->mod_qpsk == 1)
                {
                  state->rf_mod = 1;
                }
            }
          else if (state->numflips > 18)
            {
              if (opts->mod_gfsk == 1)
                {
                  state->rf_mod = 2;
                }
            }
          else
            {
              if (opts->mod_c4fm == 1)
                {
                  state->rf_mod = 0;
                }
            }
          state->numflips = 0;
        }
      else
        {
          lastt++;
        }

      if (state->dibit_buf_p > state->dibit_buf + 900000)
      {
    	 state->dibit_buf_p = state->dibit_buf + 200;
      }

      //determine dibit state
      if (symbol > 0)
        {
          *state->dibit_buf_p = 1;
          state->dibit_buf_p++;
          dibit = 49;               // '1'
        }
      else
        {
          *state->dibit_buf_p = 3;
          state->dibit_buf_p++;
          dibit = 51;               // '3'
        }

      if (opts->symbol_out_f && dibit != 0)
      {
        int csymbol = 0;
        if (dibit == 49)
        {
          csymbol = 1; //1
        }
        if (dibit == 51)
        {
          csymbol = 3; //3
        }
        //fprintf (stderr, "%d", dibit);
        fputc (csymbol, opts->symbol_out_f);
      }

      //digitize test for storing dibits in buffer correctly for dmr recovery

      if (state->dmr_payload_p > state->dmr_payload_buf + 900000)
      {
       state->dmr_payload_p = state->dmr_payload_buf + 200;
      }

      if (1 == 1)
      {
        if (symbol > state->center)
        {
          if (symbol > state->umid)
          {
            *state->dmr_payload_p = 1;               // +3
          }
          else
          {
            *state->dmr_payload_p = 0;               // +1
          }
        }
        else
        {
          if (symbol < state->lmid)
          {
            *state->dmr_payload_p = 3;               // -3
          }
          else
          {
            *state->dmr_payload_p = 2;               // -1
          }
        }
      }
      
      state->dmr_payload_p++;
      // end digitize and dmr buffer testing

      *synctest_p = dibit; 
      if (t >= t_max) //works excelent now with short sync patterns, and no issues with large ones!
        {
          for (i = 0; i < t_max; i++) //24
            {
              lbuf2[i] = lbuf[i];
            }
          qsort (lbuf2, t_max, sizeof (int), comp); 
          lmin = (lbuf2[1] + lbuf2[2] + lbuf2[3]) / 3;
          lmax = (lbuf2[t_max - 3] + lbuf2[t_max - 2] + lbuf2[t_max - 1]) / 3; 

          if (state->rf_mod == 1)
            {
              state->minbuf[state->midx] = lmin;
              state->maxbuf[state->midx] = lmax;
              if (state->midx == (opts->msize - 1)) //-1
                {
                  state->midx = 0;
                }
              else
                {
                  state->midx++;
                }
              lsum = 0;
              for (i = 0; i < opts->msize; i++)
                {
                  lsum += state->minbuf[i];
                }
              state->min = lsum / opts->msize;
              lsum = 0;
              for (i = 0; i < opts->msize; i++)
                {
                  lsum += state->maxbuf[i];
                }
              state->max = lsum / opts->msize;
              state->center = ((state->max) + (state->min)) / 2;
              state->maxref = (int)((state->max) * 0.80F);
              state->minref = (int)((state->min) * 0.80F);
            }
          else
            {
              state->maxref = state->max;
              state->minref = state->min;
            }


          //if using an rtl input method, do not look for sync patterns if the rms value is lower than our 'soft squelch' level
          if (opts->audio_in_type == 3 && opts->rtl_rms < opts->rtl_squelch_level)
          {
            if (opts->frame_nxdn48 == 1 || opts->frame_nxdn96 == 1 || opts->frame_dpmr == 1 || opts->frame_m17 == 1)
            {
              goto SYNC_TEST_END;
            }
          }

          strncpy (synctest, (synctest_p - 23), 24);
          if (opts->frame_p25p1 == 1)
            {
              if (strcmp (synctest, P25P1_SYNC) == 0)
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  state->dmrburstR = 17;
                  state->payload_algidR = 0;
                  state->dmr_stereo = 1; //check to see if this causes dmr data issues later on during mixed sync types
                  sprintf (state->ftype, "P25 Phase 1");
                  if (opts->errorbars == 1)
                    {
                      printFrameSync (opts, state, "+P25p1 ", synctest_pos + 1, modulation);
                    }
                  state->lastsynctype = 0;
                  state->last_cc_sync_time = time(NULL);
                  return (0);
                }
              if (strcmp (synctest, INV_P25P1_SYNC) == 0)
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  state->dmrburstR = 17;
                  state->payload_algidR = 0;
                  state->dmr_stereo = 1; //check to see if this causes dmr data issues later on during mixed sync types
                  sprintf (state->ftype, "P25 Phase 1");
                  if (opts->errorbars == 1)
                    {
                      printFrameSync (opts, state, "-P25p1 ", synctest_pos + 1, modulation);
                    }
                  state->lastsynctype = 1;
                  state->last_cc_sync_time = time(NULL);
                  return (1);
                }
            }
          if (opts->frame_x2tdma == 1)
            {
              if ((strcmp (synctest, X2TDMA_BS_DATA_SYNC) == 0) || (strcmp (synctest, X2TDMA_MS_DATA_SYNC) == 0))
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + (lmax)) / 2;
                  state->min = ((state->min) + (lmin)) / 2;
                  if (opts->inverted_x2tdma == 0)
                    {
                      // data frame
                      sprintf (state->ftype, "X2-TDMA");
                      if (opts->errorbars == 1)
                        {
                          printFrameSync (opts, state, "+X2-TDMA ", synctest_pos + 1, modulation);
                        }
                      state->lastsynctype = 2;
                      return (2);
                    }
                  else
                    {
                      // inverted voice frame
                      sprintf (state->ftype, "X2-TDMA");
                      if (opts->errorbars == 1)
                        {
                          printFrameSync (opts, state, "-X2-TDMA ", synctest_pos + 1, modulation);
                        }
                      if (state->lastsynctype != 3)
                        {
                          state->firstframe = 1;
                        }
                      state->lastsynctype = 3;
                      return (3);
                    }
                }
              if ((strcmp (synctest, X2TDMA_BS_VOICE_SYNC) == 0) || (strcmp (synctest, X2TDMA_MS_VOICE_SYNC) == 0))
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  if (opts->inverted_x2tdma == 0)
                    {
                      // voice frame
                      sprintf (state->ftype, "X2-TDMA");
                      if (opts->errorbars == 1)
                        {
                          printFrameSync (opts, state, "+X2-TDMA ", synctest_pos + 1, modulation);
                        }
                      if (state->lastsynctype != 4)
                        {
                          state->firstframe = 1;
                        }
                      state->lastsynctype = 4;
                      return (4);
                    }
                  else
                    {
                      // inverted data frame
                      sprintf (state->ftype, "X2-TDMA");
                      if (opts->errorbars == 1)
                        {
                          printFrameSync (opts, state, "-X2-TDMA ", synctest_pos + 1, modulation);
                        }
                      state->lastsynctype = 5;
                      return (5);
                    }
                }
            }
          //YSF sync
          strncpy(synctest20, (synctest_p - 19), 20);
          if(opts->frame_ysf == 1) 
          {
            if (strcmp(synctest20, FUSION_SYNC) == 0)
            {
              printFrameSync (opts, state, "+YSF ", synctest_pos + 1, modulation);
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              opts->inverted_ysf = 0;
              state->lastsynctype = 30;
              return (30);
            }
            else if (strcmp(synctest20, INV_FUSION_SYNC) == 0)
            {
              printFrameSync (opts, state, "-YSF ", synctest_pos + 1, modulation);
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              opts->inverted_ysf = 1;
              state->lastsynctype = 31;
              return (31);
            }
          }
          //end YSF sync

          //M17 Sync -- Just STR and LSF for now
          strncpy(synctest16, (synctest_p - 15), 16);
          strncpy(synctest8, (synctest_p - 7), 8);
          if(opts->frame_m17 == 1) 
          {
            //preambles will skip dibits in an attempt to prime the 
            //demodulator but not attempt any decoding
            if (strcmp(synctest8, M17_PRE) == 0)
            {
              if (opts->inverted_m17 == 0)
              {
                printFrameSync (opts, state, "+M17 PREAMBLE", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                state->lastsynctype = 98;
                fprintf (stderr, "\n");
                return (98);
              }
            }
            else if (strcmp(synctest8, M17_PIV) == 0)
            {
              if (opts->inverted_m17 == 1)
              {
                printFrameSync (opts, state, "-M17 PREAMBLE", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                state->lastsynctype = 99;
                fprintf (stderr, "\n");
                return (99);
              }
            }
            else if (strcmp(synctest8, M17_PKT) == 0)
            {
              if (opts->inverted_m17 == 0)
              {
                printFrameSync (opts, state, "+M17 PKT", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                if (state->lastsynctype == 86 || state->lastsynctype == 8)
                {
                  state->lastsynctype = 86;
                  return (86);
                }
                state->lastsynctype = 86;
                fprintf (stderr, "\n");
              }
              // else //unknown, -BRT?
              // {
              //   printFrameSync (opts, state, "-M17 BRT", synctest_pos + 1, modulation);
              //   state->carrier = 1;
              //   state->offset = synctest_pos;
              //   state->max = ((state->max) + lmax) / 2;
              //   state->min = ((state->min) + lmin) / 2;
              //   if (state->lastsynctype == 77)
              //   {
              //     state->lastsynctype = 77;
              //     return (77);
              //   }
              //   state->lastsynctype = 77;
              //   fprintf (stderr, "\n");
              // }
            }
            else if (strcmp(synctest8, M17_STR) == 0)
            {
              if (opts->inverted_m17 == 0)
              {
                printFrameSync (opts, state, "+M17 STR", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                if (state->lastsynctype == 16 || state->lastsynctype == 8)
                {
                  state->lastsynctype = 16;
                  return (16);
                }
                state->lastsynctype = 16;
                fprintf (stderr, "\n");
              }
              else
              {
                printFrameSync (opts, state, "-M17 LSF", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                if (state->lastsynctype == 99)
                {
                  state->lastsynctype = 9;
                  return (9);
                }
                state->lastsynctype = 9;
                fprintf (stderr, "\n");
              }
            }
            else if (strcmp(synctest8, M17_LSF) == 0)
            {
              if (opts->inverted_m17 == 1)
              {
                printFrameSync (opts, state, "-M17 STR", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                if (state->lastsynctype == 17 || state->lastsynctype == 9)
                {
                  state->lastsynctype = 17;
                  return (17);
                }
                state->lastsynctype = 17;
                fprintf (stderr, "\n");
              }
              else
              {
                printFrameSync (opts, state, "+M17 LSF", synctest_pos + 1, modulation);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                if (state->lastsynctype == 98)
                {
                  state->lastsynctype = 8;
                  return (8);
                }
                state->lastsynctype = 8;
                fprintf (stderr, "\n");
              }
            }
          }
          //end M17 

          //P25 P2 sync S-ISCH VCH
          strncpy(synctest20, (synctest_p - 19), 20);
          if(opts->frame_p25p2 == 1)
          {
            if (0 == 0)
            {
              if (strcmp(synctest20, P25P2_SYNC) == 0)
              {
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                opts->inverted_p2 = 0;
                state->lastsynctype = 35; //35
                if (opts->errorbars == 1)
                {
                  printFrameSync (opts, state, "+P25p2 SISCH", synctest_pos + 1, modulation);
                }
                if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0)
            		{
            			//fprintf (stderr, "%s", KCYN);
            			fprintf (stderr, " WACN [%05llX] SYS [%03llX] NAC [%03llX] ", state->p2_wacn, state->p2_sysid, state->p2_cc);
            			//fprintf (stderr, "%s", KNRM);
            		}
            		else
            		{
            			fprintf (stderr, "%s", KRED);
            			fprintf (stderr, " P2 Missing Parameters            ");
            			fprintf (stderr, "%s", KNRM);
            		}
                state->last_cc_sync_time = time(NULL);
                return (35); //35
              }
            }
          }
          if(opts->frame_p25p2 == 1)
          {
            if (0 == 0)
            {
              //S-ISCH VCH
              if (strcmp(synctest20, INV_P25P2_SYNC) == 0)
              {
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                opts->inverted_p2 = 1;
                if (opts->errorbars == 1)
                {
                  printFrameSync (opts, state, "-P25p2 SISCH", synctest_pos + 1, modulation);
                }
                if (state->p2_wacn != 0 && state->p2_cc != 0 && state->p2_sysid != 0)
            		{
            			//fprintf (stderr, "%s", KCYN);
            			fprintf (stderr, " WACN [%05llX] SYS [%03llX] NAC [%03llX] ", state->p2_wacn, state->p2_sysid, state->p2_cc);
            			//fprintf (stderr, "%s", KNRM);
            		}
            		else
            		{
            			fprintf (stderr, "%s", KRED);
            			fprintf (stderr, " P2 Missing Parameters            ");
            			fprintf (stderr, "%s", KNRM);
            		}

                state->lastsynctype = 36; //36
                state->last_cc_sync_time = time(NULL);
                return (36); //36
              }
            }
          }

          //dPMR sync
          strncpy(synctest,   (synctest_p - 23), 24);
          strncpy(synctest12, (synctest_p - 11), 12);
          if(opts->frame_dpmr == 1)
          {
            if (opts->inverted_dpmr == 0)
            {
              if(strcmp(synctest, DPMR_FRAME_SYNC_1) == 0)
              {
                //fprintf (stderr, "+dPMR FS1\n");
              }
              if(strcmp(synctest12, DPMR_FRAME_SYNC_2) == 0)
              {
                //fprintf (stderr, "DPMR_FRAME_SYNC_2\n");
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;

                sprintf(state->ftype, "dPMR ");
                if (opts->errorbars == 1)
                {
                  printFrameSync (opts, state, "+dPMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 21;
                return (21);
              }
              if(strcmp(synctest12, DPMR_FRAME_SYNC_3) == 0)
              {
                //fprintf (stderr, "+dPMR FS3 \n");
              }
              if(strcmp(synctest, DPMR_FRAME_SYNC_4) == 0)
              {
                //fprintf (stderr, "+dPMR FS4 \n");
              }
            }
            if (opts->inverted_dpmr == 1)
            {
              if(strcmp(synctest, INV_DPMR_FRAME_SYNC_1) == 0)
              {
                //fprintf (stderr, "-dPMR FS1 \n");
              }
              if(strcmp(synctest12, INV_DPMR_FRAME_SYNC_2) == 0)
              {
                //fprintf (stderr, "INV_DPMR_FRAME_SYNC_2\n");
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;

                sprintf(state->ftype, "dPMR ");
                if (opts->errorbars == 1)
                {
                  printFrameSync (opts, state, "-dPMR ", synctest_pos + 1, modulation);
                }

                state->lastsynctype = 25;
                return (25);
              }
              if(strcmp(synctest12, INV_DPMR_FRAME_SYNC_3) == 0)
              {
                //fprintf (stderr, "-dPMR FS3 \n");
              }
              if(strcmp(synctest, INV_DPMR_FRAME_SYNC_4) == 0)
              {
                //fprintf (stderr, "-dPMR FS4 \n");
              }
            }
          }

          //New DMR Sync
          if (opts->frame_dmr == 1)
          {

            if(strcmp (synctest, DMR_MS_DATA_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              //state->directmode = 0;
              //fprintf (stderr, "DMR MS Data");
              if (opts->inverted_dmr == 0) //opts->inverted_dmr
              {
                // data frame
                sprintf(state->ftype, "DMR MS");
                if (opts->errorbars == 1)
                {
                  //printFrameSync (opts, state, "+DMR MS Data", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 33) //33
                {
                  //state->firstframe = 1;
                }
                state->lastsynctype = 33; //33
                return (33); //33
              }
              else //inverted MS voice frame
              {
                sprintf(state->ftype, "DMR MS");
                state->lastsynctype = 32;
                return (32);
              }
            }

            if(strcmp (synctest, DMR_MS_VOICE_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              //state->directmode = 0;
              //fprintf (stderr, "DMR MS VOICE\n");
              if (opts->inverted_dmr == 0) //opts->inverted_dmr
              {
                // voice frame
                sprintf(state->ftype, "DMR MS");
                if (opts->errorbars == 1)
                {
                  //printFrameSync (opts, state, "+DMR MS Voice", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 32)
                {
                  //state->firstframe = 1;
                }
                state->lastsynctype = 32;
                return (32);
              }
              else //inverted MS data frame
              {
                sprintf(state->ftype, "DMR MS");
                state->lastsynctype = 33;
                return (33);
              }

            }

            //if ((strcmp (synctest, DMR_MS_DATA_SYNC) == 0) || (strcmp (synctest, DMR_BS_DATA_SYNC) == 0))
            if (strcmp (synctest, DMR_BS_DATA_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + (lmax)) / 2;
              state->min = ((state->min) + (lmin)) / 2;
              state->directmode = 0;
              if (opts->inverted_dmr == 0)
              {
                // data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1)
                {
                  printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 10;
                state->last_cc_sync_time = time(NULL); 
                return (10);
              }
              else
              {
                // inverted voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 11)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 11;
                state->last_cc_sync_time = time(NULL); 
                return (11); //11
              }
            }
            if(strcmp (synctest, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + (lmax)) / 2;
              state->min = ((state->min) + (lmin)) / 2;
              //state->currentslot = 0;
              state->directmode = 1;  //Direct mode
              if (opts->inverted_dmr == 0)
              {
                // data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1)
                {
                  //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 33;
                state->last_cc_sync_time = time(NULL);
                return (33);
              }
              else
              {
                // inverted voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1)
                {
                  //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 11)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 32;
                state->last_cc_sync_time = time(NULL);
                return (32);
              }
            } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0) */
            if(strcmp (synctest, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + (lmax)) / 2;
              state->min = ((state->min) + (lmin)) / 2;
              //state->currentslot = 1;
              state->directmode = 1;  //Direct mode
              if (opts->inverted_dmr == 0)
              {
                // data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1)
                {
                  //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 33;
                state->last_cc_sync_time = time(NULL);
                return (33);
              }
              else
              {
                // inverted voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 11)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 32;
                state->last_cc_sync_time = time(NULL);
                return (32);
              }
            } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0) */
            //if((strcmp (synctest, DMR_MS_VOICE_SYNC) == 0) || (strcmp (synctest, DMR_BS_VOICE_SYNC) == 0))
            if(strcmp (synctest, DMR_BS_VOICE_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              state->directmode = 0;
              if (opts->inverted_dmr == 0)
              {
                // voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 12)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 12;
                state->last_cc_sync_time = time(NULL); 
                return (12);
              }

              else
              {
                // inverted data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1) //&& opts->dmr_stereo == 0
                {
                  printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 13;
                state->last_cc_sync_time = time(NULL); 
                return (13);
              }
            }
            if(strcmp (synctest, DMR_DIRECT_MODE_TS1_VOICE_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              //state->currentslot = 0;
              state->directmode = 1;  //Direct mode
              if (opts->inverted_dmr == 0) //&& opts->dmr_stereo == 1
              {
                // voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 12)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 32;
                state->last_cc_sync_time = time(NULL);
                return (32); //treat Direct Mode same as MS mode for now
              }
              else
              {
                // inverted data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 33;
                return (33);
              }
            } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS1_VOICE_SYNC) == 0) */
            if(strcmp (synctest, DMR_DIRECT_MODE_TS2_VOICE_SYNC) == 0)
            {
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              // state->currentslot = 1;
              state->directmode = 1;  //Direct mode
              if (opts->inverted_dmr == 0) //&& opts->dmr_stereo == 1
              {
                // voice frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                }
                if (state->lastsynctype != 12)
                {
                  state->firstframe = 1;
                }
                state->lastsynctype = 32;
                state->last_cc_sync_time = time(NULL);
                return (32);
              }
              else
              {
                // inverted data frame
                sprintf(state->ftype, "DMR ");
                if (opts->errorbars == 1 && opts->dmr_stereo == 0)
                {
                  //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                }
                state->lastsynctype = 33;
                state->last_cc_sync_time = time(NULL);
                return (33);
              }
            } //End if(strcmp (synctest, DMR_DIRECT_MODE_TS2_VOICE_SYNC) == 0)
          } //End if (opts->frame_dmr == 1)

          //end DMR Sync

          //ProVoice and EDACS sync
          if (opts->frame_provoice == 1)
          {
            strncpy (synctest32, (synctest_p - 31), 32);
            strncpy (synctest48, (synctest_p - 47), 48);
            if ((strcmp (synctest32, PROVOICE_SYNC) == 0) || (strcmp (synctest32, PROVOICE_EA_SYNC) == 0))
            {
                state->last_cc_sync_time = time(NULL);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                sprintf (state->ftype, "ProVoice ");
                if (opts->errorbars == 1)
                printFrameSync (opts, state, "+PV   ", synctest_pos + 1, modulation);
                state->lastsynctype = 14;
                return (14);
            }
            else if ((strcmp (synctest32, INV_PROVOICE_SYNC) == 0) || (strcmp (synctest32, INV_PROVOICE_EA_SYNC) == 0))
            {
                state->last_cc_sync_time = time(NULL);
                state->carrier = 1;
                state->offset = synctest_pos;
                state->max = ((state->max) + lmax) / 2;
                state->min = ((state->min) + lmin) / 2;
                sprintf (state->ftype, "ProVoice ");
                printFrameSync (opts, state, "-PV   ", synctest_pos + 1, modulation);
                state->lastsynctype = 15;
                return (15);
            }
            else if ( strcmp (synctest48, EDACS_SYNC) == 0)
            {
              state->last_cc_sync_time = time(NULL);
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              printFrameSync (opts, state, "-EDACS", synctest_pos + 1, modulation);
              state->lastsynctype = 38; 
              return (38);
            }
            else if ( strcmp (synctest48, INV_EDACS_SYNC) == 0)
            {
              state->last_cc_sync_time = time(NULL);
              state->carrier = 1;
              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              printFrameSync (opts, state, "+EDACS", synctest_pos + 1, modulation);
              state->lastsynctype = 37; 
              return (37);
            }
            else if ((strcmp (synctest48, DOTTING_SEQUENCE_A) == 0) || (strcmp (synctest48, DOTTING_SEQUENCE_B) == 0))
            {
              //only print and execute Dotting Sequence if Trunking and Tuned so we don't get multiple prints on this
              if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1)
              {
                printFrameSync (opts, state, " EDACS  DOTTING SEQUENCE: ", synctest_pos + 1, modulation);
                eot_cc (opts, state);
              }
            }

          }
         
          else if (opts->frame_dstar == 1)
            {
              if (strcmp (synctest, DSTAR_SYNC) == 0)
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  sprintf (state->ftype, "DSTAR ");
                  if (opts->errorbars == 1)
                    {
                      printFrameSync (opts, state, "+DSTAR VOICE ", synctest_pos + 1, modulation);
                    }
                  state->lastsynctype = 6;
                  return (6);
                }
              if (strcmp (synctest, INV_DSTAR_SYNC) == 0)
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  sprintf (state->ftype, "DSTAR ");
                  if (opts->errorbars == 1)
                    {
                      printFrameSync (opts, state, "-DSTAR VOICE ", synctest_pos + 1, modulation);
                    }
                  state->lastsynctype = 7;
                  return (7);
                }
              if (strcmp (synctest, DSTAR_HD) == 0)
                 {
                   state->carrier = 1;
                   state->offset = synctest_pos;
                   state->max = ((state->max) + lmax) / 2;
                   state->min = ((state->min) + lmin) / 2;
                   sprintf (state->ftype, "DSTAR_HD ");
                   if (opts->errorbars == 1)
                     {
                       printFrameSync (opts, state, "+DSTAR HEADER", synctest_pos + 1, modulation);
                     }
                   state->lastsynctype = 18;
                   return (18);
                 }
              if (strcmp (synctest, INV_DSTAR_HD) == 0)
                {
                   state->carrier = 1;
                   state->offset = synctest_pos;
                   state->max = ((state->max) + lmax) / 2;
                   state->min = ((state->min) + lmin) / 2;
                   sprintf (state->ftype, " DSTAR_HD");
                   if (opts->errorbars == 1)
                     {
                       printFrameSync (opts, state, "-DSTAR HEADER", synctest_pos + 1, modulation);
                     }
                   state->lastsynctype = 19;
                   return (19);
                 }

            }

          //NXDN
          else if ((opts->frame_nxdn96 == 1) || (opts->frame_nxdn48 == 1))
          {
            strncpy (synctest10, (synctest_p - 9), 10); //FSW only
            if (
                   (strcmp (synctest10, "3131331131") == 0 ) //this seems to be the most common 'correct' pattern on Type-C
                || (strcmp (synctest10, "3331331131") == 0 ) //this one hits on new sync but gives a bad lich code
                || (strcmp (synctest10, "3131331111") == 0 )
                || (strcmp (synctest10, "3331331111") == 0 )
                || (strcmp (synctest10, "3131311131") == 0 ) //First few FSW on NXDN48 Type-C seems to hit this for some reason
                      
                )
            {

              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              if (state->lastsynctype == 28) 
              {
                state->last_cc_sync_time = time(NULL);
                return (28);
              }
              state->lastsynctype = 28;
            }

            else if ( 
                      
                       (strcmp (synctest10, "1313113313") == 0 )
                    || (strcmp (synctest10, "1113113313") == 0 )
                    || (strcmp (synctest10, "1313113333") == 0 )
                    || (strcmp (synctest10, "1113113333") == 0 )
                    || (strcmp (synctest10, "1313133313") == 0 )
                      
                    )
            {

              state->offset = synctest_pos;
              state->max = ((state->max) + lmax) / 2;
              state->min = ((state->min) + lmin) / 2;
              if (state->lastsynctype == 29) 
              {
                state->last_cc_sync_time = time(NULL);
                return (29);
              }
              state->lastsynctype = 29;
            }
          }

          //Provoice Conventional -- Some False Positives due to shortened frame sync pattern, so use squelch if possible
          #ifdef PVCONVENTIONAL
          if (opts->frame_provoice == 1)
          {
            memset (synctest32, 0, sizeof(synctest32)); 
            strncpy (synctest32, (synctest_p - 31), 16); //short sync grab here on 32
            char pvc_txs[9]; //string (symbol) value of TX Address
            char pvc_rxs[9]; //string (symbol) value of RX Address
            uint8_t pvc_txa = 0; //actual value of TX Address
            uint8_t pvc_rxa = 0; //actual value of RX Address
            strncpy (pvc_txs, (synctest_p - 15), 8); //copy string value of TX Address
            strncpy (pvc_rxs, (synctest_p - 7), 8); //copy string value of RX Address
            if ((strcmp (synctest32, INV_PROVOICE_CONV_SHORT) == 0))
            {
                if (state->lastsynctype == 15) //use this condition, like NXDN, to migitage false positives due to short sync pattern
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  sprintf (state->ftype, "ProVoice ");
                  // fprintf (stderr, "Sync Pattern = %s ", synctest32);
                  // fprintf (stderr, "TX = %s ", pvc_txs);
                  // fprintf (stderr, "RX = %s ", pvc_rxs);
                  for (int i = 0; i < 8; i++)
                  {
                    pvc_txa = pvc_txa << 1;
                    pvc_rxa = pvc_rxa << 1;
                    //symbol 1 is binary 1 on inverted
                    //I hate working with strings, has to be a better way to evaluate this
                    memset (pvc_txs, 0, sizeof (pvc_txs));
                    memset (pvc_rxs, 0, sizeof (pvc_rxs));
                    strncpy (pvc_txs, (synctest_p - 15+i), 1);
                    strncpy (pvc_rxs, (synctest_p - 7+i), 1);
                    if ((strcmp (pvc_txs, "1") == 0))
                      pvc_txa = pvc_txa + 1;
                    if ((strcmp (pvc_rxs, "1") == 0))
                      pvc_rxa = pvc_rxa + 1;
                  }
                  printFrameSync (opts, state, "-PV_C ", synctest_pos + 1, modulation);
                  fprintf (stderr, "TX: %d ", pvc_txa);
                  fprintf (stderr, "RX: %d ", pvc_rxa);
                  if (pvc_txa == 172) fprintf (stderr, "ALL CALL ");
                  state->lastsynctype = 15;
                  return (15);
                }
                state->lastsynctype = 15;
            }
            else if ((strcmp (synctest32, PROVOICE_CONV_SHORT) == 0))
            {
                if (state->lastsynctype == 14) //use this condition, like NXDN, to migitage false positives due to short sync pattern
                {
                  state->carrier = 1;
                  state->offset = synctest_pos;
                  state->max = ((state->max) + lmax) / 2;
                  state->min = ((state->min) + lmin) / 2;
                  sprintf (state->ftype, "ProVoice ");
                  // fprintf (stderr, "Sync Pattern = %s ", synctest32);
                  // fprintf (stderr, "TX = %s ", pvc_txs);
                  // fprintf (stderr, "RX = %s ", pvc_rxs);
                  for (int i = 0; i < 8; i++)
                  {
                    pvc_txa = pvc_txa << 1;
                    pvc_rxa = pvc_rxa << 1;
                    //symbol 3 is binary 1 on positive
                    //I hate working with strings, has to be a better way to evaluate this
                    memset (pvc_txs, 0, sizeof (pvc_txs));
                    memset (pvc_rxs, 0, sizeof (pvc_rxs));
                    strncpy (pvc_txs, (synctest_p - 15+i), 1);
                    strncpy (pvc_rxs, (synctest_p - 7+i), 1);
                    if ((strcmp (pvc_txs, "3") == 0))
                      pvc_txa = pvc_txa + 1;
                    if ((strcmp (pvc_rxs, "3") == 0))
                      pvc_rxa = pvc_rxa + 1;
                  }
                  printFrameSync (opts, state, "+PV_C ", synctest_pos + 1, modulation);
                  fprintf (stderr, "TX: %d ", pvc_txa);
                  fprintf (stderr, "RX: %d ", pvc_rxa);
                  if (pvc_txa == 172) fprintf (stderr, "ALL CALL ");
                  state->lastsynctype = 14;
                  return (14);
                }
                state->lastsynctype = 14;
            }
          }
          #endif //End Provoice Conventional

          SYNC_TEST_END: ; //do nothing

        } // t >= 10

      if (exitflag == 1)
        {
          cleanupAndExit (opts, state);
        }

      if (synctest_pos < 10200)
        {
          synctest_pos++;
          synctest_p++;

        }
      else
        {
          // buffer reset
          synctest_pos = 0;
          synctest_p = synctest_buf;
          noCarrier (opts, state);

        }

      if (state->lastsynctype != 1)
        {

          if (synctest_pos >= 1800)
            {
              if ((opts->errorbars == 1) && (opts->verbose > 1) && (state->carrier == 1))
                {
                  fprintf (stderr,"Sync: no sync\n");
                  // fprintf (stderr,"Press CTRL + C to close.\n"); 

                }
              noCarrier (opts, state);

              return (-1);
            }
        }        

    }

  return (-1);
  
}

