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
#if !defined(NULL)
#define NULL 0
#endif

#include "p25p1_check_nid.h"

void
printFrameInfo (dsd_opts * opts, dsd_state * state)
{

  int level;

  level = (int) state->max / 164;
  if (opts->verbose > 0)
    {
      // fprintf (stderr,"inlvl: %2i%% ", level);
      UNUSED(level);
    }
  if (state->nac != 0)
    {
      fprintf (stderr, "%s", KCYN);
      fprintf (stderr,"nac: [%4X] ", state->nac);
      fprintf (stderr, "%s", KNRM);
    }

  if (opts->verbose > 1)
    {
      fprintf (stderr, "%s", KGRN);
      fprintf (stderr,"src: [%8i] ", state->lastsrc);
      fprintf (stderr, "%s", KNRM);
    }
  fprintf (stderr, "%s", KGRN);
  fprintf (stderr,"tg: [%5i] ", state->lasttg);
  fprintf (stderr, "%s", KNRM);
}

void
processFrame (dsd_opts * opts, dsd_state * state)
{

  int i, j, dibit;
  char duid[3];
  char nac[13];
  int level;
  UNUSED2(nac, level);

  char status_0;
  UNUSED(status_0);
  char bch_code[63];
  int index_bch_code;
  unsigned char parity;
  char v;
  int new_nac;
  char new_duid[3];
  int check_result;

  nac[12] = 0;
  duid[2] = 0;
  j = 0;

  if (state->rf_mod == 1)
    {
      state->maxref = (int)(state->max * 0.80F);
      state->minref = (int)(state->min * 0.80F);
    }
  else
    {
      state->maxref = state->max;
      state->minref = state->min;
    }

  //NXDN FSW
  if ((state->synctype == 28) || (state->synctype == 29))
  {
    //MBEout restored, is not handled internally by nxdn_frame.c
    nxdn_frame (opts, state); 
    return;
  }

  //DSTAR
  else if ((state->synctype == 6) || (state->synctype == 7))
  {
    if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
      openMbeOutFile (opts, state);
    sprintf (state->fsubtype, " VOICE        ");
    processDSTAR (opts, state);
    return;
  }
  else if ((state->synctype == 18) || (state->synctype == 19))
  {
    if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
      openMbeOutFile (opts, state);
    sprintf (state->fsubtype, " DATA         ");
    processDSTAR_HD (opts, state);
    return;
  }
    //Start DMR Types
    else if ((state->synctype >= 10 && state->synctype <= 13) || (state->synctype == 32) || (state->synctype == 33) || (state->synctype == 34) ) //32-34 DMR MS and RC
    {

      //print manufacturer strings to branding, disabled 0x10 moto other systems can use that fid set
      //0x06 is trident, but when searching, apparently, they developed con+, but was bought by moto?
      if (state->dmr_mfid == 0x10) ; //sprintf (state->dmr_branding, "%s",  "Motorola");
      else if (state->dmr_mfid == 0x68) sprintf (state->dmr_branding, "%s", "  Hytera");
      else if (state->dmr_mfid == 0x58) sprintf (state->dmr_branding, "%s", "    Tait");
      
      //disabling these due to random data decodes setting an odd mfid, could be legit, but only for that one packet?
      //or, its just a decode error somewhere
      // else if (state->dmr_mfid == 0x20) sprintf (state->dmr_branding, "%s", "JVC Kenwood");
      // else if (state->dmr_mfid == 0x04) sprintf (state->dmr_branding, "%s", "Flyde Micro");
      // else if (state->dmr_mfid == 0x05) sprintf (state->dmr_branding, "%s", "PROD-EL SPA");
      // else if (state->dmr_mfid == 0x06) sprintf (state->dmr_branding, "%s", "Motorola"); //trident/moto con+
      // else if (state->dmr_mfid == 0x07) sprintf (state->dmr_branding, "%s", "RADIODATA");
      // else if (state->dmr_mfid == 0x08) sprintf (state->dmr_branding, "%s", "Hytera");
      // else if (state->dmr_mfid == 0x09) sprintf (state->dmr_branding, "%s", "ASELSAN");
      // else if (state->dmr_mfid == 0x0A) sprintf (state->dmr_branding, "%s", "Kirisun");
      // else if (state->dmr_mfid == 0x0B) sprintf (state->dmr_branding, "%s", "DMR Association");
      // else if (state->dmr_mfid == 0x13) sprintf (state->dmr_branding, "%s", "EMC S.P.A.");
      // else if (state->dmr_mfid == 0x1C) sprintf (state->dmr_branding, "%s", "EMC S.P.A.");
      // else if (state->dmr_mfid == 0x33) sprintf (state->dmr_branding, "%s", "Radio Activity");
      // else if (state->dmr_mfid == 0x3C) sprintf (state->dmr_branding, "%s", "Radio Activity");
      // else if (state->dmr_mfid == 0x77) sprintf (state->dmr_branding, "%s", "Vertex Standard");

      //disable so radio id doesn't blink in and out during ncurses and aggressive_framesync
      state->nac = 0;

      if (opts->errorbars == 1)
      {
        if (opts->verbose > 0)
        {
          level = (int) state->max / 164;
          //fprintf (stderr,"inlvl: %2i%% ", level);
        }
      }
      if ( (state->synctype == 11) || (state->synctype == 12) || (state->synctype == 32) ) //DMR Voice Modes
      {

        sprintf (state->fsubtype, " VOICE        ");
        if (opts->dmr_stereo == 0 && state->synctype < 32)
        {
          sprintf (state->slot1light, " slot1 ");
          sprintf (state->slot2light, " slot2 ");
          //we can safely open MBE on any MS or mono handling
          if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state); 
          if (opts->p25_trunk == 0) dmrMSBootstrap (opts, state); 
        }
        if (opts->dmr_mono == 1 && state->synctype == 32)
        {
          //we can safely open MBE on any MS or mono handling
          if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
          if (opts->p25_trunk == 0) dmrMSBootstrap (opts, state); 
        }
        if (opts->dmr_stereo == 1) //opts->dmr_stereo == 1
        {
          state->dmr_stereo = 1; //set the state to 1 when handling pure voice frames
          if (state->synctype > 31 )
          {
            //we can safely open MBE on any MS or mono handling
            if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL)) openMbeOutFile (opts, state);
            if (opts->p25_trunk == 0) dmrMSBootstrap (opts, state); //bootstrap into MS Bootstrap (voice only)
          }
          else dmrBSBootstrap (opts, state); //bootstrap into BS Bootstrap
        }
      }
      else if ( (state->synctype == 33) || (state->synctype == 34) ) //MS Data and RC data
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
        if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);
        if (opts->p25_trunk == 0) dmrMSData (opts, state);
      }
      else
      {
        if (opts->dmr_stereo == 0)
        {
          if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
          if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

          state->err_str[0] = 0;
          sprintf (state->slot1light, " slot1 ");
          sprintf (state->slot2light, " slot2 ");
          dmr_data_sync (opts, state);
        }
        //switch dmr_stereo to 0 when handling BS data frame syncs with processDMRdata
        if (opts->dmr_stereo == 1)
        {
          if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
          if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

          state->dmr_stereo = 0; //set the state to zero for handling pure data frames
          sprintf (state->slot1light, " slot1 ");
          sprintf (state->slot2light, " slot2 ");
          dmr_data_sync (opts, state);
        }
      }
      return;
    }
    //X2-TDMA
    else if ((state->synctype >= 2) && (state->synctype <= 5))
    {
      state->nac = 0;
      if (opts->errorbars == 1)
        {
          printFrameInfo (opts, state);
        }
      if ((state->synctype == 3) || (state->synctype == 4))
        {
          if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
          {
            openMbeOutFile (opts, state);
          }
          sprintf (state->fsubtype, " VOICE        ");
          processX2TDMAvoice (opts, state);
        }
      else
        {
          if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
          state->err_str[0] = 0;
          processX2TDMAdata (opts, state);
        }
      return;
    }
  else if ((state->synctype == 14) || (state->synctype == 15))
    {
      if (opts->errorbars == 1)
      {
        if (opts->verbose > 0)
        {
          level = (int) state->max / 164;
          //fprintf (stderr,"inlvl: %2i%% ", level);
        }
      }
      if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
      {
        openMbeOutFile (opts, state);
      }
      sprintf (state->fsubtype, " VOICE        ");
      processProVoice (opts, state);
      return;
    }
    //edacs
    else if ((state->synctype == 37) || (state->synctype == 38))
    {
      if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state); 
      edacs (opts, state);
      return;
    }
    //ysf
    else if ((state->synctype == 30) || (state->synctype == 31))
    {
      //relocate MBEout to inside frame handling -- Not Working Currently for YSF
      processYSF(opts, state);
      return;
    }
    //M17
    else if ((state->synctype == 16) || (state->synctype == 9)  || (state->synctype == 17) || (state->synctype == 8)  || 
             (state->synctype == 76) || (state->synctype == 77) || (state->synctype == 86) || (state->synctype == 87) ||
             (state->synctype == 99) || (state->synctype == 98) )
    {
      if (state->synctype == 98 || state->synctype == 99) //preamble only
        skipDibit(opts, state, 8); //skip dibits to prime the demodulator
      else if (state->synctype == 16 || state->synctype == 17)
        processM17STR(opts, state);
      else if (state->synctype == 76 || state->synctype == 77) {} //Not available yet
      //   processM17BRT(opts, state); //Not available yet
      else if (state->synctype == 86 || state->synctype == 87)
        processM17PKT(opts, state);
      else 
        processM17LSF(opts, state);
      return;
    }
    //P25 P2
    else if ((state->synctype == 35) || (state->synctype == 36))
    {
      //relocate MBEout to inside frame handling
      processP2(opts, state);
      return;
    }
    //dPMR
    else if ((state->synctype == 20) || (state->synctype == 24))
    {
      /* dPMR Frame Sync 1 */
      fprintf(stderr, "dPMR Frame Sync 1 ");
      if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
    }
    else if ((state->synctype == 21) || (state->synctype == 25))
    {
      /* dPMR Frame Sync 2 */
        fprintf(stderr, "dPMR Frame Sync 2 ");

        state->nac = 0;
        state->lastsrc = 0;
        state->lasttg = 0;
        if (opts->errorbars == 1)
        {
          if (opts->verbose > 0)
          {
            level = (int) state->max / 164;
          }
        }
        state->nac = 0;

        if ((opts->mbe_out_dir[0] != 0) && (opts->mbe_out_f == NULL))
        {
          openMbeOutFile (opts, state);
        }
        sprintf(state->fsubtype, " VOICE        ");
        processdPMRvoice (opts, state);

        return;

    }
    else if ((state->synctype == 22) || (state->synctype == 26))
    {
      /* dPMR Frame Sync 3 */
      fprintf(stderr, "dPMR Frame Sync 3 ");
      if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
    }
    else if ((state->synctype == 23) || (state->synctype == 27))
    {
      /* dPMR Frame Sync 4 */
      fprintf(stderr, "dPMR Frame Sync 4 ");
      if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
    }
    //dPMR
  else //P25
    {
      // Read the NAC, 12 bits
      j = 0;
      index_bch_code = 0;
      for (i = 0; i < 6; i++)
        {
          dibit = getDibit (opts, state);

          v = 1 & (dibit >> 1); // bit 1
          nac[j] = v + '0';
          j++;
          bch_code[index_bch_code] = v;
          index_bch_code++;

          v = 1 & dibit;        // bit 0
          nac[j] = v + '0';
          j++;
          bch_code[index_bch_code] = v;
          index_bch_code++;
        }
        //this one setting bogus nac data
      // state->nac = strtol (nac, NULL, 2);

      // Read the DUID, 4 bits
      for (i = 0; i < 2; i++)
        {
          dibit = getDibit (opts, state);
          duid[i] = dibit + '0';

          bch_code[index_bch_code] = 1 & (dibit >> 1);  // bit 1
          index_bch_code++;
          bch_code[index_bch_code] = 1 & dibit;         // bit 0
          index_bch_code++;
        }

      // Read the BCH data for error correction of NAC and DUID
      for (i = 0; i < 3; i++)
        {
          dibit = getDibit (opts, state);

          bch_code[index_bch_code] = 1 & (dibit >> 1);  // bit 1
          index_bch_code++;
          bch_code[index_bch_code] = 1 & dibit;         // bit 0
          index_bch_code++;
        }
      // Intermission: read the status dibit
      status_0 = getDibit (opts, state) + '0';
      // ... continue reading the BCH error correction data
      for (i = 0; i < 20; i++)
        {
          dibit = getDibit (opts, state);

          bch_code[index_bch_code] = 1 & (dibit >> 1);  // bit 1
          index_bch_code++;
          bch_code[index_bch_code] = 1 & dibit;         // bit 0
          index_bch_code++;
        }

      // Read the parity bit
      dibit = getDibit (opts, state);
      bch_code[index_bch_code] = 1 & (dibit >> 1);      // bit 1
      parity = (1 & dibit);     // bit 0

      // Check if the NID is correct
      check_result = check_NID (bch_code, &new_nac, new_duid, parity);
      if (check_result) {
          if (new_nac != state->nac) {
              // NAC fixed by error correction
              state->nac = new_nac;
              //apparently, both 0 and 0xFFF can the BCH code on signal drop
              if (state->p2_hardset == 0 && new_nac != 0 && new_nac != 0xFFF)
              {
                state->p2_cc = new_nac;
              }
              state->debug_header_errors++;
          }
          if (strcmp(new_duid, duid) != 0) {
              // DUID fixed by error correction
              //fprintf (stderr,"Fixing DUID %s -> %s\n", duid, new_duid);
              duid[0] = new_duid[0];
              duid[1] = new_duid[1];
              state->debug_header_errors++;
          }
      } else {
          // Check of NID failed and unable to recover its value
          //fprintf (stderr,"NID error\n");
          duid[0] = 'E';
          duid[1] = 'E';
          state->debug_header_critical_errors++;
      }
    }

  if (strcmp (duid, "00") == 0)
    {
      // Header Data Unit
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," HDU\n");
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state); 
        if (opts->mbe_out_f == NULL) openMbeOutFile (opts, state);
      }
      mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
      state->lastp25type = 2;
      state->dmrburstL = 25;
      state->currentslot = 0;
      sprintf (state->fsubtype, " HDU          ");
      processHDU (opts, state);
    }
  else if (strcmp (duid, "11") == 0)
    {
      // Logical Link Data Unit 1
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," LDU1  ");
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f == NULL)
        {
          openMbeOutFile (opts, state);
        }
      }
      state->lastp25type = 1;
      state->dmrburstL = 26;
      state->currentslot = 0;
      sprintf (state->fsubtype, " LDU1         ");
      state->numtdulc = 0;

      processLDU1 (opts, state);
    }
  else if (strcmp (duid, "22") == 0)
    {
      // Logical Link Data Unit 2
      state->dmrburstL = 27;
      state->currentslot = 0;
      if (state->lastp25type != 1)
        {
          if (opts->errorbars == 1)
          {
            printFrameInfo (opts, state);
            fprintf (stderr,"\n Ignoring LDU2 not preceeded by LDU1\n");
          }
          state->lastp25type = 0;
          sprintf (state->fsubtype, "              ");
        }
      else
        {
          if (opts->errorbars == 1)
          {
            printFrameInfo (opts, state);
            fprintf (stderr," LDU2  ");
          }
          if (opts->mbe_out_dir[0] != 0)
          {
            if (opts->mbe_out_f == NULL)
            {
              openMbeOutFile (opts, state);
            }
          }
          state->lastp25type = 2;
          sprintf (state->fsubtype, " LDU2         ");
          state->numtdulc = 0;
          processLDU2 (opts, state);
        }
    }
  else if (strcmp (duid, "33") == 0)
    {
      // Terminator with subsequent Link Control
      state->dmrburstL = 28;
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," TDULC\n");
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
      }
      mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
      // state->lasttg = 0;
      // state->lastsrc = 0;
      state->lastp25type = 0;
      state->err_str[0] = 0;
      sprintf (state->fsubtype, " TDULC        ");
      state->numtdulc++;
      if ((opts->resume > 0) && (state->numtdulc > opts->resume))
      {
        resumeScan (opts, state);
      }
      processTDULC (opts, state);
      state->err_str[0] = 0;
    }
  else if (strcmp (duid, "03") == 0)
    {
      // Terminator without subsequent Link Control
      state->dmrburstL = 28;
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," TDU\n");
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
      }
      mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
      state->lasttg = 0;
      state->lastsrc = 0;
      state->lastp25type = 0;
      state->err_str[0] = 0;
      sprintf (state->fsubtype, " TDU          ");

      processTDU (opts, state);
    }
  else if (strcmp (duid, "13") == 0)
    {
      state->dmrburstL = 29;
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," TSBK");
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
        if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);
      }
      if (opts->resume > 0)
      {
        resumeScan (opts, state);
      }
      state->lasttg = 0;
      state->lastsrc = 0;
      state->lastp25type = 3;
      sprintf (state->fsubtype, " TSBK         ");

      processTSBK(opts, state);

    }
  else if (strcmp (duid, "30") == 0)
    {
      state->dmrburstL = 29;
      if (opts->errorbars == 1)
      {
        printFrameInfo (opts, state);
        fprintf (stderr," MPDU\n"); //multi block format PDU
      }
      if (opts->mbe_out_dir[0] != 0)
      {
        if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
        if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);
      }
      if (opts->resume > 0)
      {
        resumeScan (opts, state);
      }
      state->lastp25type = 4;
      sprintf (state->fsubtype, " MPDU         ");

      processMPDU(opts, state);
    }

  else
  {
    state->lastp25type = 0;
    sprintf (state->fsubtype, "              ");
    if (opts->errorbars == 1)
    {
      printFrameInfo (opts, state);
      // fprintf (stderr," duid:%s *Unknown DUID*\n", duid);
      fprintf (stderr," duid:%s \n", duid); //DUID ERR
    }
  }
}
