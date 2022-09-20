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

#define _MAIN

#include "dsd.h"
#include "p25p1_const.h"
#include "x2tdma_const.h"
#include "dstar_const.h"
#include "nxdn_const.h"
#include "dmr_const.h"
#include "provoice_const.h"
#include "git_ver.h"

int pretty_colors()
{
    fprintf (stderr,"%sred\n", KRED);
    fprintf (stderr,"%sgreen\n", KGRN);
    fprintf (stderr,"%syellow\n", KYEL);
    fprintf (stderr,"%sblue\n", KBLU);
    fprintf (stderr,"%smagenta\n", KMAG);
    fprintf (stderr,"%scyan\n", KCYN);
    fprintf (stderr,"%swhite\n", KWHT);
    fprintf (stderr,"%snormal\n", KNRM);

    return 0;
}


#include "p25p1_heuristics.h"
#include "pa_devs.h"

char * FM_banner[9] = {
  "                                                        ",
  " ██████╗  ██████╗██████╗     ███████╗███╗   ███╗███████╗",
  " ██╔══██╗██╔════╝██╔══██╗    ██╔════╝████╗ ████║██╔════╝",
  " ██║  ██║╚█████╗ ██║  ██║    █████╗  ██╔████╔██║█████╗  ",
  " ██║  ██║ ╚═══██╗██║  ██║    ██╔══╝  ██║╚██╔╝██║██╔══╝  ",
  " ██████╔╝██████╔╝██████╔╝    ██║     ██║ ╚═╝ ██║███████╗",
  " ╚═════╝ ╚═════╝ ╚═════╝     ╚═╝     ╚═╝     ╚═╝╚══════╝",
  " https://github.com/lwvmobile/dsd-fme/tree/pulseaudio    "
};

int
comp (const void *a, const void *b)
{
  if (*((const int *) a) == *((const int *) b))
    return 0;
  else if (*((const int *) a) < *((const int *) b))
    return -1;
  else
    return 1;
}

//struct for checking existence of directory to write to
struct stat st = {0};
char wav_file_directory[1024] = {0};

void
noCarrier (dsd_opts * opts, dsd_state * state)
{
  // state->testcounter = 0;
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  //dmr buffer
  state->dmr_payload_p = state->dibit_buf + 200;
  memset (state->dmr_payload_buf, 0, sizeof (int) * 200);
  //dmr buffer end
  if (opts->mbe_out_f != NULL)
    {
      closeMbeOutFile (opts, state);
    }
  state->jitter = -1;
  state->lastsynctype = -1;
  state->carrier = 0;
  state->max = 15000;
  state->min = -15000;
  state->center = 0; //quick disable for dmr testing with buffer
  state->err_str[0] = 0;
  state->err_strR[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->errs = 0;
  state->errs2 = 0;
  //should I disable this or not?
  state->lasttg = 0;
  state->lastsrc = 0;
  state->lasttgR = 0;
  state->lastsrcR = 0;
  state->lastp25type = 0;
  state->repeat = 0;
  state->nac = 0;
  state->numtdulc = 0;
  sprintf (state->slot1light, "%s", "");
  sprintf (state->slot2light, "%s", "");
  state->firstframe = 0;
  if (opts->audio_gain == (float) 0)
  {
    state->aout_gain = 25;
  }

  if (opts->audio_gainR == (float) 0)
  {
    state->aout_gainR = 25;
  }
  memset (state->aout_max_buf, 0, sizeof (float) * 200);
  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;

  memset (state->aout_max_bufR, 0, sizeof (float) * 200);
  state->aout_max_buf_pR = state->aout_max_bufR;
  state->aout_max_buf_idxR = 0;

  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
  mbe_initMbeParms (state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2);

  state->dmr_ms_mode = 0;

  //not sure if desirable here or not just yet, may need to disable a few of these
  state->payload_mi  = 0;
  state->payload_miR = 0;
  state->payload_mfid = 0;
  state->payload_mfidR = 0;
  state->payload_algid = 0;
  state->payload_algidR = 0;
  state->payload_keyid = 0;
  state->payload_keyidR = 0;

  // state->dmr_fid = 0;
  // state->dmr_so = 0;
  // state->dmr_fidR = 0;
  // state->dmr_soR = 0;

  state->HYTL = 0;
  state->HYTR = 0;
  state->DMRvcL = 0;
  state->DMRvcR = 0;

  state->payload_miN = 0;
  state->p25vc = 0;
  state->payload_miP = 0;

  //initialize dmr data header source
  state->dmr_lrrp_source[0] = 0;
  state->dmr_lrrp_source[1] = 0;

  //initialize 12 rate superframe
  for (short i = 0; i < 288; i++)
  {
    state->dmr_12_rate_sf[0][i] = 0;
    state->dmr_12_rate_sf[1][i] = 0;
  }

  //initialize 34 rate superframe
  for (short i = 0; i < 288; i++)
  {
    state->dmr_34_rate_sf[0][i] = 0;
    state->dmr_34_rate_sf[1][i] = 0;
  }

  //initialize data header bits
  state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
  state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
  state->data_header_padding[0] = 0;
  state->data_header_padding[1] = 0;
  state->data_header_format[0] = 0;
  state->data_header_format[1] = 0;
  state->data_header_sap[0] = 0;
  state->data_header_sap[1] = 0;
  state->data_block_counter[0] = 0;
  state->data_block_counter[1] = 0;
  state->data_p_head[0] = 0;
  state->data_p_head[1] = 0;

  // state->dmr_so   = 0; //let TLC or Voice LC/Burst zero or set this instead?
  // state->dmr_soR  = 0;

  state->dmr_encL = 0;
  state->dmr_encR = 0;

  state->dmrburstL = 17;
  state->dmrburstR = 17;

  //reset P2 ESS_B fragments and 4V counter
  for (short i = 0; i < 4; i++)
  {
    state->ess_b[0][i] = 0;
    state->ess_b[1][i] = 0;
  }
  state->fourv_counter[0] = 0;
  state->fourv_counter[1] = 0;

}

void
initOpts (dsd_opts * opts)
{

  opts->onesymbol = 10;
  opts->mbe_in_file[0] = 0;
  opts->mbe_in_f = NULL;
  opts->errorbars = 1;
  opts->datascope = 0;
  opts->symboltiming = 0;
  opts->verbose = 2;
  opts->p25enc = 0;
  opts->p25lc = 0;
  opts->p25status = 0;
  opts->p25tg = 0;
  opts->scoperate = 15;
  sprintf (opts->audio_in_dev, "pulse");
  opts->audio_in_fd = -1;

  sprintf (opts->audio_out_dev, "pulse");
  opts->audio_out_fd = -1;

  opts->split = 0;
  opts->playoffset = 0;
  opts->playoffsetR = 0;
  opts->mbe_out_dir[0] = 0;
  opts->mbe_out_file[0] = 0;
  opts->mbe_out_path[0] = 0;
  opts->mbe_out_f = NULL;
  opts->audio_gain = 0;
  opts->audio_gainR = 0;
  opts->audio_out = 1;
  opts->wav_out_file[0] = 0;
  opts->wav_out_fileR[0] = 0;
  opts->wav_out_file_raw[0] = 0;
  opts->symbol_out_file[0] = 0;
  opts->lrrp_out_file[0] = 0;
  opts->symbol_out_f = NULL;
  opts->symbol_out = 0;
  opts->mbe_out = 0;
  opts->wav_out_f = NULL;
  opts->wav_out_fR = NULL;
  opts->wav_out_raw = NULL;

  opts->dmr_stereo_wav = 0; //flag for per call dmr stereo wav recordings
  //opts->wav_out_fd = -1;
  opts->serial_baud = 115200;
  sprintf (opts->serial_dev, "/dev/ttyUSB0");
  opts->resume = 0;
  opts->frame_dstar = 0;
  opts->frame_x2tdma = 1;
  opts->frame_p25p1 = 1;
  opts->frame_p25p2 = 1;
  opts->frame_nxdn48 = 0;
  opts->frame_nxdn96 = 0;
  opts->frame_dmr = 1;
  opts->frame_dpmr = 0;
  opts->frame_provoice = 0;
  opts->mod_c4fm = 1;
  opts->mod_qpsk = 1;
  opts->mod_gfsk = 0;
  opts->uvquality = 3;
  opts->inverted_x2tdma = 1;    // most transmitter + scanner + sound card combinations show inverted signals for this
  opts->inverted_dmr = 0;       // most transmitter + scanner + sound card combinations show non-inverted signals for this
  opts->mod_threshold = 26;
  opts->ssize = 36;
  opts->msize = 15;
  opts->playfiles = 0;
  opts->delay = 0;
  opts->use_cosine_filter = 1;
  opts->unmute_encrypted_p25 = 0;
  opts->rtl_dev_index = 0;        //choose which device we want by index number
  opts->rtl_gain_value = 0;    //set actual gain and not automatic gain
  opts->rtl_squelch_level = 0; //fully open by default, want to specify level for things like NXDN with false positives
  opts->rtl_volume_multiplier = 1; //set multipler from rtl sample to 'boost volume'
  opts->rtl_udp_port = 6020; //set UDP port for RTL remote
  opts->rtl_bandwidth = 12; //changed recommended default to 12, 24 for ProVoice
  opts->rtlsdr_ppm_error = 0; //initialize ppm with 0 value; bug reported by N.
  opts->rtl_started = 0;
  opts->pulse_raw_rate_in   = 48000;
  opts->pulse_raw_rate_out  = 48000; //
  opts->pulse_digi_rate_in  = 48000;
  opts->pulse_digi_rate_out = 24000; //new default for XDMA
  opts->pulse_raw_in_channels   = 1;
  opts->pulse_raw_out_channels  = 1;
  opts->pulse_digi_in_channels  = 1; //2
  opts->pulse_digi_out_channels = 2; //new default for XDMA

  sprintf (opts->output_name, "XDMA");
  opts->pulse_flush = 1; //set 0 to flush, 1 for flushed
  opts->use_ncurses_terminal = 0;
  opts->ncurses_compact = 0;
  opts->ncurses_history = 1;
  opts->payload = 0;
  opts->inverted_dpmr = 0;
  opts->dmr_stereo = 1;
  opts->aggressive_framesync = 1; //more aggressive to combat wonk wonk voice decoding
  //see if initializing these values causes issues elsewhere, if so, then disable.
  opts->audio_in_type = 0;  //this was never initialized, causes issues on rPI 64 (bullseye) if not initialized
  opts->audio_out_type = 0; //this was never initialized, causes issues on rPI 64 (bullseye) if not initialized

  opts->lrrp_file_output = 0;

  opts->dmr_mute_encL = 1;
  opts->dmr_mute_encR = 1;

  opts->monitor_input_audio = 0;

  opts->inverted_p2 = 0;
  opts->p2counter = 0;

}

void
initState (dsd_state * state)
{

  int i, j;
  // state->testcounter = 0;
  state->last_dibit = 0;
  state->dibit_buf = malloc (sizeof (int) * 1000000);
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  //dmr buffer -- double check this set up
  state->dmr_payload_buf = malloc (sizeof (int) * 1000000);
  state->dmr_payload_p = state->dmr_payload_buf + 200;
  memset (state->dmr_payload_buf, 0, sizeof (int) * 200);
  //dmr buffer end
  state->repeat = 0;
  state->audio_out_buf = malloc (sizeof (short) * 1000000);
  state->audio_out_bufR = malloc (sizeof (short) * 1000000);
  memset (state->audio_out_buf, 0, 100 * sizeof (short));
  memset (state->audio_out_bufR, 0, 100 * sizeof (short));
  state->audio_out_buf_p = state->audio_out_buf + 100;
  state->audio_out_buf_pR = state->audio_out_bufR + 100;
  state->audio_out_float_buf = malloc (sizeof (float) * 1000000);
  state->audio_out_float_bufR = malloc (sizeof (float) * 1000000);
  memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
  memset (state->audio_out_float_bufR, 0, 100 * sizeof (float));
  state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
  state->audio_out_float_buf_pR = state->audio_out_float_bufR + 100;
  state->audio_out_idx = 0;
  state->audio_out_idx2 = 0;
  state->audio_out_idxR = 0;
  state->audio_out_idx2R = 0;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;
  state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
  //state->wav_out_bytes = 0;
  state->center = 0;
  state->jitter = -1;
  state->synctype = -1;
  state->min = -15000;
  state->max = 15000;
  state->lmid = 0;
  state->umid = 0;
  state->minref = -12000;
  state->maxref = 12000;
  state->lastsample = 0;
  for (i = 0; i < 128; i++)
    {
      state->sbuf[i] = 0;
    }
  state->sidx = 0;
  for (i = 0; i < 1024; i++)
    {
      state->maxbuf[i] = 15000;
    }
  for (i = 0; i < 1024; i++)
    {
      state->minbuf[i] = -15000;
    }
  state->midx = 0;
  state->err_str[0] = 0;
  state->err_strR[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->symbolcnt = 0;
  state->symbolc = 0; //
  state->rf_mod = 0;
  state->numflips = 0;
  state->lastsynctype = -1;
  state->lastp25type = 0;
  state->offset = 0;
  state->carrier = 0;
  for (i = 0; i < 25; i++)
    {
      for (j = 0; j < 16; j++)
        {
          state->tg[i][j] = 48;
        }
    }
  state->tgcount = 0;
  state->lasttg = 0;
  state->lastsrc = 0;
  state->nac = 0;
  state->errs = 0;
  state->errs2 = 0;
  state->mbe_file_type = -1;
  state->optind = 0;
  state->numtdulc = 0;
  state->firstframe = 0;
  sprintf (state->slot1light, "%s", "");
  sprintf (state->slot2light, "%s", "");
  state->aout_gain = 25;
  memset (state->aout_max_buf, 0, sizeof (float) * 200);
  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;

  memset (state->aout_max_bufR, 0, sizeof (float) * 200);
  state->aout_max_buf_pR = state->aout_max_bufR;
  state->aout_max_buf_idxR = 0;

  state->samplesPerSymbol = 10;
  state->symbolCenter = 4;
  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
  state->currentslot = 0;
  state->cur_mp = malloc (sizeof (mbe_parms));
  state->prev_mp = malloc (sizeof (mbe_parms));
  state->prev_mp_enhanced = malloc (sizeof (mbe_parms));

  state->cur_mp2 = malloc (sizeof (mbe_parms));
  state->prev_mp2 = malloc (sizeof (mbe_parms));
  state->prev_mp_enhanced2 = malloc (sizeof (mbe_parms));

  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
  mbe_initMbeParms (state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2);
  state->p25kid = 0;

  state->debug_audio_errors = 0;
  state->debug_audio_errorsR = 0;
  state->debug_header_errors = 0;
  state->debug_header_critical_errors = 0;

  state->nxdn_last_ran = -1;
  state->nxdn_last_rid = 0;
  state->nxdn_last_tg = 0;
  state->nxdn_cipher_type = 0;
  state->nxdn_key = 0;
  sprintf (state->nxdn_call_type, "%s", "");
  state->payload_miN = 0;

  state->dpmr_color_code = -1;
  state->dpmr_caller_id = 0;
  state->dpmr_target_id = 0;

  state->payload_mi  = 0;
  state->payload_miR = 0;
  state->payload_mfid = 0;
  state->payload_mfidR = 0;
  state->payload_algid = 0;
  state->payload_algidR = 0;
  state->payload_keyid = 0;
  state->payload_keyidR = 0;

  //init P2 ESS_B fragments and 4V counter
  for (short i = 0; i < 4; i++)
  {
    state->ess_b[0][i] = 0;
    state->ess_b[1][i] = 0;
  }
  state->fourv_counter[0] = 0;
  state->fourv_counter[1] = 0;

  sprintf (state->dmr_branding, "%s", "");
  sprintf (state->dmr_callsign[0][0], "%s", "");
  sprintf (state->dmr_callsign[0][1], "%s", "");
  sprintf (state->dmr_callsign[0][2], "%s", "");
  sprintf (state->dmr_callsign[0][3], "%s", "");
  sprintf (state->dmr_callsign[0][4], "%s", "");
  sprintf (state->dmr_callsign[0][5], "%s", "");
  sprintf (state->dmr_callsign[1][0], "%s", "");
  sprintf (state->dmr_callsign[1][1], "%s", "");
  sprintf (state->dmr_callsign[1][2], "%s", "");
  sprintf (state->dmr_callsign[1][3], "%s", "");
  sprintf (state->dmr_callsign[1][4], "%s", "");
  sprintf (state->dmr_callsign[1][5], "%s", "");
  sprintf (state->dmr_lrrp[0][0], "%s", "");
  sprintf (state->dmr_lrrp[0][1], "%s", "");
  sprintf (state->dmr_lrrp[0][2], "%s", "");
  sprintf (state->dmr_lrrp[0][3], "%s", "");
  sprintf (state->dmr_lrrp[0][4], "%s", "");
  sprintf (state->dmr_lrrp[0][5], "%s", "");
  sprintf (state->dmr_lrrp[1][0], "%s", "");
  sprintf (state->dmr_lrrp[1][1], "%s", "");
  sprintf (state->dmr_lrrp[1][2], "%s", "");
  sprintf (state->dmr_lrrp[1][3], "%s", "");
  sprintf (state->dmr_lrrp[1][4], "%s", "");
  sprintf (state->dmr_lrrp[1][5], "%s", "");

  state->K = 0;
  state->R = 0;
  state->RR = 0;
  state->H = 0;
  state->K1 = 0;
  state->K2 = 0;
  state->K3 = 0;
  state->K4 = 0;
  state->M = 0; //force key priority over settings from fid/so

  state->dmr_stereo = 0; //1, or 0?
  state->dmrburstL = 17; //initialize at higher value than possible
  state->dmrburstR = 17; //17 in char array is set for ERR
  state->dmr_so   = 0;
  state->dmr_soR  = 0;
  state->dmr_fid  = 0;
  state->dmr_fidR = 0;
  state->dmr_ms_mode = 0;

  state->HYTL = 0;
  state->HYTR = 0;
  state->DMRvcL = 0;
  state->DMRvcR = 0;

  state->p25vc = 0;

  state->payload_miP = 0;

  memset(state->dstarradioheader, 0, 41);

  //initialize dmr data header source
  state->dmr_lrrp_source[0] = 0;
  state->dmr_lrrp_source[1] = 0;

  //initialize 12 rate superframe
  for (short i = 0; i < 288; i++)
  {
    state->dmr_12_rate_sf[0][i] = 0;
    state->dmr_12_rate_sf[1][i] = 0;
  }

  //initialize 34 rate superframe
  for (short i = 0; i < 288; i++)
  {
    state->dmr_34_rate_sf[0][i] = 0;
    state->dmr_34_rate_sf[1][i] = 0;
  }

  //initialize data header bits
  state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
  state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
  state->data_header_padding[0] = 0;
  state->data_header_padding[1] = 0;
  state->data_header_format[0] = 0;
  state->data_header_format[1] = 0;
  state->data_header_sap[0] = 0;
  state->data_header_sap[1] = 0;
  state->data_block_counter[0] = 0;
  state->data_block_counter[1] = 0;
  state->data_p_head[0] = 0;
  state->data_p_head[1] = 0;

  state->menuopen = 0; //is the ncurses menu open, if so, don't process frame sync

  state->dmr_encL = 0;
  state->dmr_encR = 0;

  //P2 variables
  state->p2_wacn = 0;
  state->p2_sysid = 0;
  state->p2_cc = 0;
  state->p2_hardset = 0;
  state->p2_is_lcch = 0;

  //experimental symbol file capture read throttle
  state->symbol_throttle = 0; //throttle speed
  state->use_throttle = 0; //only use throttle if set to 1

  state->p2_scramble_offset = 0;
  state->p2_vch_chan_num = 0;

#ifdef TRACE_DSD
  state->debug_sample_index = 0;
  state->debug_label_file = NULL;
  state->debug_label_dibit_file = NULL;
  state->debug_label_imbe_file = NULL;
#endif

  initialize_p25_heuristics(&state->p25_heuristics);
}

void
usage ()
{

  printf ("\n");
  printf ("Usage: dsd-fme [options]            Live scanner mode\n");
  printf ("  or:  dsd-fme [options] -r <files> Read/Play saved mbe data from file(s)\n");
  printf ("  or:  dsd-fme -h                   Show help\n");
  printf ("\n");
  printf ("Display Options:\n");
  printf ("  -N            Use NCurses Terminal\n");
  printf ("                 dsd-fme -N 2> log.txt \n");
  printf ("  -e            Show Frame Info and errorbars (default)\n");
  printf ("  -pe           Show P25 encryption sync bits\n");
  printf ("  -pl           Show P25 link control bits\n");
  printf ("  -ps           Show P25 status bits and low speed data\n");
  printf ("  -pt           Show P25 talkgroup info\n");
  printf ("  -q            Don't show Frame Info/errorbars\n");
  printf ("  -s            Datascope (disables other display options)\n");
  printf ("  -t            Show symbol timing during sync\n");
  printf ("  -v <num>      Frame information Verbosity\n");
  printf ("  -z <num>      Frame rate for datascope\n");
  printf ("\n");
  printf ("Input/Output options:\n");
  printf ("  -i <device>   Audio input device (default is pulse audio, \n                  - for piped stdin, rtl for rtl device)\n");
  printf ("  -o <device>   Audio output device (default is pulse audio)\n");
  printf ("  -d <dir>      Create mbe data files, use this directory\n");
  printf ("  -r <files>    Read/Play saved mbe data from file(s)\n");
  printf ("  -g <num>      Audio output gain (default = 0 = auto, disable = -1)\n");
  printf ("  -w <file>     Output synthesized speech to a .wav file\n");
  printf ("  -T            Enable Per Call WAV file saving in XDMA  and NXDN decoding classes\n");
  printf ("                 (Per Call can only be used in Ncurses Terminal!)\n");
  printf ("                 (Running in console will use static wav files)\n");
  printf ("  -n            Throttle Symbol Capture Bin Input");
  printf ("                 (useful when reading files still being written to by OP25)");
  printf ("\n");
  printf ("RTL-SDR options:\n");
  printf ("  -c <hertz>    RTL-SDR Frequency\n");
  printf ("  -P <num>      RTL-SDR PPM Error (default = 0)\n");
  printf ("  -D <num>      RTL-SDR Device Index Number\n");
  printf ("  -G <num>      RTL-SDR Device Gain (0-49) (default = 0 Auto Gain)\n");
  printf ("  -L <num>      RTL-SDR Squelch Level (0 - Open, 25 - Little, 50 - Higher)\n                 (Just have to guess really...)\n");
  printf ("  -V <num>      RTL-SDR Sample Gain Multiplier (default = 1)\n");
  printf ("  -Y <num>      RTL-SDR VFO Bandwidth kHz (default = 48)(6, 8, 12, 16, 24, 48) \n");
  printf ("  -U <num>      RTL-SDR UDP Remote Port (default = 6020)\n");
  printf ("\n");
  printf ("Scanner control options:\n");
  printf ("  -B <num>      Serial port baud rate (default=115200)\n");
  printf ("  -C <device>   Serial port for scanner control (default=/dev/ttyUSB0)\n");
  printf ("  -R <num>      Resume scan after <num> TDULC frames or any PDU or TSDU\n");
  printf ("\n");
  printf ("Decoder options:\n");
  printf ("  -fa           Legacy Auto Detection (old methods default)\n");
  printf ("  -ft           XDMA P25 and DMR BS/MS frame types (new default)\n");
  printf ("  -f1           Decode only P25 Phase 1\n");
  printf ("  -fd           Decode only D-STAR\n");
  printf ("  -fr           Decode only DMR\n");
  printf ("  -fx           Decode only X2-TDMA\n");
  printf ("  -fi             Decode only NXDN48* (6.25 kHz) / IDAS*\n");
  printf ("  -fn             Decode only NXDN96* (12.5 kHz)\n");
  printf ("  -fp             Decode only ProVoice*\n");
  printf ("  -fm             Decode only dPMR*\n");
  printf ("  -l            Disable DMR and NXDN input filtering\n");
  printf ("  -pu           Unmute Encrypted P25\n");
  printf ("  -u <num>      Unvoiced speech quality (default=3)\n");
  printf ("  -xx           Expect non-inverted X2-TDMA signal\n");
  printf ("  -xr           Expect inverted DMR signal\n");
  printf ("  -xd           Expect inverted ICOM dPMR signal\n");
  printf ("\n");
  printf ("  * denotes frame types that cannot be auto-detected.\n");
  printf ("\n");
  printf ("Advanced Decoder options:\n");
  // printf ("  -A <num>      QPSK modulation auto detection threshold (default=26)\n");
  printf ("  -S <num>      Symbol buffer size for QPSK decision point tracking\n");
  printf ("                 (default=36)\n");
  printf ("  -M <num>      Min/Max buffer size for QPSK decision point tracking\n");
  printf ("                 (default=15)\n");
  // printf ("  -F            Enable DMR TDMA Stereo Passive Frame Sync\n");
  // printf ("                 This feature will attempt to resync less often due to excessive voice errors\n");
  // printf ("                 Use if skipping occurs, but may cause wonky audio due to loss of good sync\n");
  printf ("  -Z            Log MBE/Frame Payloads to console\n");
  printf ("\n");
  printf ("  -K <dec>      Manually Enter DMRA Privacy Key (Decimal Value of Key Number)\n");
  printf ("\n");
  printf ("  -H <hex>      Manually Enter **tera 10 Privacy Key (40-Bit/10-Char Hex Value)\n");
  printf ("                (32/64-Char values can only be entered in the NCurses Terminal)\n");
  printf ("\n");
  printf ("  -R <dec>      Manually Enter NXDN 4800 EHR Scrambler Key Value \n");
  printf ("                 \n");
  printf ("\n");
  exit (0);
}

void
liveScanner (dsd_opts * opts, dsd_state * state)
{

if (opts->audio_in_type == 1)
{
  opts->pulse_digi_rate_out = 8000; //revert to 8K/1 for STDIN input, sometimes crackles when upsampling
  opts->pulse_digi_out_channels = 1;
  if (opts->dmr_stereo == 1)
  {
    opts->pulse_digi_rate_out = 24000; //stdin needs 24000 by 2 channel for DMR TDMA Stereo output
    opts->pulse_digi_out_channels = 2;
    fprintf (stderr, "STDIN Audio Rate Out set to 24000 Khz/2 Channel \n");
  }
  else fprintf (stderr, "STDIN Audio Rate Out set to 48000 Khz/1 Channel \n");
  opts->pulse_raw_rate_out = 48000;
  opts->pulse_raw_out_channels = 1;

}

#ifdef USE_RTLSDR
  if(opts->audio_in_type == 3)
  {
    //still need this section mostly due the the crackling on the rtl dongle when upsampled
    //probably need to dig a little deeper, maybe inlvl releated?
    opts->pulse_digi_rate_out = 8000; //revert to 8K/1 for RTL input, random crackling otherwise
    opts->pulse_digi_out_channels = 1;
    if (opts->dmr_stereo == 1)
    {
      opts->pulse_digi_rate_out = 24000; //rtl needs 24000 by 2 channel for DMR TDMA Stereo output
      opts->pulse_digi_out_channels = 2; //minimal crackling 'may' be observed, not sure, can't test to see on DMR with RTL
      fprintf (stderr, "RTL Audio Rate Out set to 24000 Khz/2 Channel \n");
    }
    else fprintf (stderr, "RTL Audio Rate Out set to 48000 Khz/1 Channel \n");
    opts->pulse_raw_rate_out = 48000;
    opts->pulse_raw_out_channels = 1;

    open_rtlsdr_stream(opts);
    opts->rtl_started = 1; //set here so ncurses terminal doesn't attempt to open it again
  }
#endif

if (opts->use_ncurses_terminal == 1)
{
  ncursesOpen(opts, state);
}

if (opts->audio_in_type == 0)
{
  openPulseInput(opts);
}

if (opts->audio_out_type == 0)
{
  openPulseOutput(opts);
}

	while (1)
    {

      noCarrier (opts, state);
      if (state->menuopen == 0)
      {
        state->synctype = getFrameSync (opts, state);
        // recalibrate center/umid/lmid
        state->center = ((state->max) + (state->min)) / 2;
        state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
        state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
      }



      while (state->synctype != -1)
        {
          processFrame (opts, state);

#ifdef TRACE_DSD
          state->debug_prefix = 'S';
#endif

          // state->synctype = getFrameSync (opts, state);

#ifdef TRACE_DSD
          state->debug_prefix = '\0';
#endif

          // // recalibrate center/umid/lmid
          // state->center = ((state->max) + (state->min)) / 2;
          // state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
          // state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
          if (state->menuopen == 0)
          {
            state->synctype = getFrameSync (opts, state);
            // recalibrate center/umid/lmid
            state->center = ((state->max) + (state->min)) / 2;
            state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
            state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
          }
        }
    }
}

void
cleanupAndExit (dsd_opts * opts, dsd_state * state)
{
  noCarrier (opts, state);
  if (opts->wav_out_f != NULL)
  {
    closeWavOutFile (opts, state);
  }
  if (opts->wav_out_raw != NULL)
  {
    closeWavOutFileRaw (opts, state);
  }
  if (opts->dmr_stereo_wav == 1) //cause of crash on exit, need to check if NULL first, may need to set NULL when turning off in nterm
  {
    closeWavOutFileL (opts, state);
    closeWavOutFileR (opts, state);
  }
  if (opts->symbol_out == 1)
  {
    closeSymbolOutFile (opts, state);
  }
  if (opts->mbe_out_f != NULL)
  {
    closeMbeOutFile (opts, state);
  }

#ifdef USE_RTLSDR
  if(opts->audio_in_type == 3)
  {
    // TODO: cleanup demod threads
    //temp disable to see if this corrects issues with closing
    //cleanup_rtlsdr_stream();
  }
#endif

  fprintf (stderr,"\n");
  fprintf (stderr,"Total audio errors: %i\n", state->debug_audio_errors);
  fprintf (stderr,"Total header errors: %i\n", state->debug_header_errors);
  fprintf (stderr,"Total irrecoverable header errors: %i\n", state->debug_header_critical_errors);

  //debug_print_heuristics(&(state->p25_heuristics));

  fprintf (stderr,"\n");
  fprintf (stderr,"+P25 BER estimate: %.2f%%\n", get_P25_BER_estimate(&state->p25_heuristics));
  fprintf (stderr,"-P25 BER estimate: %.2f%%\n", get_P25_BER_estimate(&state->inv_p25_heuristics));
  fprintf (stderr,"\n");

#ifdef TRACE_DSD
  if (state->debug_label_file != NULL) {
      fclose(state->debug_label_file);
      state->debug_label_file = NULL;
  }
  if(state->debug_label_dibit_file != NULL) {
      fclose(state->debug_label_dibit_file);
      state->debug_label_dibit_file = NULL;
  }
  if(state->debug_label_imbe_file != NULL) {
      fclose(state->debug_label_imbe_file);
      state->debug_label_imbe_file = NULL;
  }
#endif

  fprintf (stderr,"Exiting.\n");
  exit (0);
}

void
sigfun (int sig)
{
    exitflag = 1;
    signal (SIGINT, SIG_DFL);
    ncursesClose ();

#ifdef USE_RTLSDR
    rtlsdr_sighandler();
#endif
}

double atofs(char *s)
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen(s);
	last = s[len-1];
	s[len-1] = '\0';
	switch (last) {
		case 'g':
		case 'G':
			suff *= 1e3;
		case 'm':
		case 'M':
			suff *= 1e3;
		case 'k':
		case 'K':
			suff *= 1e3;
			suff *= atof(s);
			s[len-1] = last;
			return suff;
	}
	s[len-1] = last;
	return atof(s);
}

int
main (int argc, char **argv)
{
  int c;
  extern char *optarg;
  extern int optind, opterr, optopt;
  dsd_opts opts;
  dsd_state state;
  char versionstr[25];
  mbe_printVersion (versionstr);

  fprintf (stderr,"            Digital Speech Decoder: Florida Man Edition\n");
  for (short int i = 1; i < 7; i++) {
    //fprintf (stderr,"%s%s \n", FM_banner[i], KGRN); //cyan/blue text
    fprintf (stderr,"%s\n", FM_banner[i]); //cyan/blue tex
  }
  //fprintf (stderr,"%s", KNRM); //change back to normal

  fprintf (stderr, "Github Build Version: %s \n", GIT_TAG);
  fprintf (stderr,"MBElib version %s\n", versionstr);

  initOpts (&opts);
  initState (&state);

  InitAllFecFunction();

  exitflag = 0;
  signal (SIGINT, sigfun);

  while ((c = getopt (argc, argv, "haep:P:qstv:z:i:o:d:c:g:nw:B:C:R:f:m:u:x:A:S:M:G:D:L:V:U:Y:K:H:NQWrlZTF")) != -1)
    {
      opterr = 0;
      switch (c)
        {
        case 'h':
          usage ();
          exit (0);
        case 'a':
          //printPortAudioDevices();
          exit(0);
        case 'e':
          opts.errorbars = 1;
          opts.datascope = 0;
          break;
        case 'p':
          if (optarg[0] == 'e')
            {
              opts.p25enc = 1;
            }
          else if (optarg[0] == 'l')
            {
              opts.p25lc = 1;
            }
          else if (optarg[0] == 's')
            {
              opts.p25status = 1;
            }
          else if (optarg[0] == 't')
            {
              opts.p25tg = 1;
            }
          else if (optarg[0] == 'u')
            {
             opts.unmute_encrypted_p25 = 1;
             fprintf(stderr, "P25 Encrypted Audio Unmuted\n");
            }
          break;
        case 'P':
          //can't handle negative numbers?
          sscanf (optarg, "%d", &opts.rtlsdr_ppm_error);
          break;
        case 'q':
          opts.errorbars = 0;
          opts.verbose = 0;
          break;
        case 's':
          opts.errorbars = 0;
          opts.p25enc = 0;
          opts.p25lc = 0;
          opts.p25status = 0;
          opts.p25tg = 0;
          opts.datascope = 1;
          opts.symboltiming = 0;
          break;
        case 't':
          opts.symboltiming = 1;
          opts.errorbars = 1;
          opts.datascope = 0;
          break;
        case 'v':
          sscanf (optarg, "%d", &opts.verbose);
          break;
        case 'n':
          state.use_throttle = 1;
          break;
        case 'K':
          sscanf (optarg, "%lld", &state.K);
          if (state.K > 256)
          {
           state.K = 256;
          }
          opts.dmr_mute_encL = 0;
          opts.dmr_mute_encR = 0;
          if (state.K == 0)
          {
            opts.dmr_mute_encL = 1;
            opts.dmr_mute_encR = 1;
          }
          break;

        case 'R':
          sscanf (optarg, "%lld", &state.R);
          if (state.R > 0x7FFF)
          {
           state.R = 0x7FFF;
          }
          // opts.dmr_mute_encL = 0;
          // opts.dmr_mute_encR = 0;
          if (state.R == 0)
          {
            // opts.dmr_mute_encL = 1;
            // opts.dmr_mute_encR = 1;
          }
          break;

        case 'H':
          sscanf (optarg, "%llX", &state.H);
          if (state.H > 0xFFFFFFFFFF)
          {
           state.H = 0xFFFFFFFFFF;
          }
          opts.dmr_mute_encL = 0;
          opts.dmr_mute_encR = 0;
          if (state.H == 0)
          {
            opts.dmr_mute_encL = 1;
            opts.dmr_mute_encR = 1;
          }
          state.K1 = state.H; //shim
          break;

        case 'G': //Set rtl device gain
          sscanf (optarg, "%d", &opts.rtl_gain_value); //multiple value by ten to make it consitent with the way rtl_fm really works
          break;

        case 'L': //Set rtl squelch level
          sscanf (optarg, "%d", &opts.rtl_squelch_level); //set squelch by user to prevent false positives on NXDN and others
          break;

        case 'V': //Set rtl voltage level
          sscanf (optarg, "%d", &opts.rtl_volume_multiplier); //set 'volume' multiplier for rtl-sdr samples
          break;

        case 'U': //Set rtl udp remote port
          sscanf (optarg, "%d", &opts.rtl_udp_port); //set udp port number for RTL remote
          break;

        case 'D': //Set rtl device index number
          sscanf (optarg, "%d", &opts.rtl_dev_index);
          break;

        case 'Y': //Set rtl VFO bandwidth --recommend 6, 12, 24, 36, 48, default 48? or 12?
          sscanf (optarg, "%d", &opts.rtl_bandwidth);
          break;

        case 'N':
          opts.use_ncurses_terminal = 1;
          fprintf (stderr,"Enabling NCurses Terminal.\n");
          //fprintf (stderr,"  - may need to issue 'reset' command in terminal after use\n");
          break;

        case 'Z':
          opts.payload = 1;
          fprintf (stderr,"Logging Frame Payload to console\n");
          break;

        case 'Q':
          opts.lrrp_file_output = 1;
          fprintf (stderr,"Logging LRRP data to ~/lrrp.txt \n");
          break;

        case 'T': //repurposed to TDMA/NXDN Per Call
        sprintf (wav_file_directory, "./WAV"); // /wav, or ./wav
        wav_file_directory[1023] = '\0';
        if (stat(wav_file_directory, &st) == -1)
        {
          fprintf (stderr, "-T %s WAV file directory does not exist\n", wav_file_directory);
          fprintf (stderr, "Creating directory %s to save decoded wav files\n", wav_file_directory);
          mkdir(wav_file_directory, 0700); //user read write execute, needs execute for some reason or segfault
        }
        fprintf (stderr,"XDMA and NXDN Per Call Wav File Saving Enabled. (NCurses Terminal Only)\n");
        sprintf (opts.wav_out_file, "./WAV/DSD-FME-X1.wav"); // foward slash here, on wav_file_directory?
        sprintf (opts.wav_out_fileR, "./WAV/DSD-FME-X2.wav");
        opts.dmr_stereo_wav = 1;
        openWavOutFileL (&opts, &state); //testing for now, will want to move to per call later
        openWavOutFileR (&opts, &state); //testing for now, will want to move to per call later
        break;

        case 'F':
          opts.aggressive_framesync = 0;
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr,"DMR Stereo Aggressive Resync Disabled!\n");
          fprintf (stderr, "%s", KNRM);
          break;

        case 'z':
          sscanf (optarg, "%d", &opts.scoperate);
          opts.errorbars = 0;
          opts.p25enc = 0;
          opts.p25lc = 0;
          opts.p25status = 0;
          opts.p25tg = 0;
          opts.datascope = 1;
          opts.symboltiming = 0;
          fprintf (stderr,"Setting datascope frame rate to %i frame per second.\n", opts.scoperate);
          break;
        case 'i':
          strncpy(opts.audio_in_dev, optarg, 1023);
          opts.audio_in_dev[1023] = '\0';
          //fprintf (stderr,"audio_in_dev = %s\n", opts.audio_in_dev);
          break;
        case 'o':
          strncpy(opts.audio_out_dev, optarg, 1023);
          opts.audio_out_dev[1023] = '\0';
          break;
        case 'd':
          strncpy(opts.mbe_out_dir, optarg, 1023);
          opts.mbe_out_dir[1023] = '\0';
          if (stat(opts.mbe_out_dir, &st) == -1)
          {
            fprintf (stderr, "-d %s directory does not exist\n", opts.mbe_out_dir);
            fprintf (stderr, "Creating directory %s to save MBE files\n", opts.mbe_out_dir);
            mkdir(opts.mbe_out_dir, 0700); //user read write execute, needs execute for some reason or segfault
          }
          else fprintf (stderr,"Writing mbe data files to directory %s\n", opts.mbe_out_dir);
          // fprintf (stderr,"Writing mbe data temporarily disabled!\n");
          break;
        case 'c':
          opts.rtlsdr_center_freq = (uint32_t)atofs(optarg);
          fprintf (stderr,"Tuning to frequency: %i Hz\n", opts.rtlsdr_center_freq);
          break;
        case 'g':
          sscanf (optarg, "%f", &opts.audio_gain);
          if (opts.audio_gain < (float) 0 )
            {
              fprintf (stderr,"Disabling audio out gain setting\n");
            }
          else if (opts.audio_gain == (float) 0)
            {
              opts.audio_gain = (float) 0;
              fprintf (stderr,"Enabling audio out auto-gain\n");
            }
          else
            {
              fprintf (stderr,"Setting audio out gain to %f\n", opts.audio_gain);
              state.aout_gain = opts.audio_gain;
              state.aout_gainR = opts.audio_gain;
            }
          break;

        case 'w':
          strncpy(opts.wav_out_file, optarg, 1023);
          opts.wav_out_file[1023] = '\0';
          fprintf (stderr,"Writing + Appending decoded audio to file %s\n", opts.wav_out_file);
          openWavOutFile (&opts, &state);
          break;

        case 'B':
          sscanf (optarg, "%d", &opts.serial_baud);
          break;

        case 'C':
          strncpy(opts.serial_dev, optarg, 1023);
          opts.serial_dev[1023] = '\0';
          break;

        // case 'R':
        //  sscanf (optarg, "%d", &opts.resume);
        //  fprintf (stderr,"Enabling scan resume after %i TDULC frames\n", opts.resume);
        //  break;

        case 'f':
          if (optarg[0] == 'a')
            {
              opts.frame_dstar = 1;
              opts.frame_x2tdma = 1;
              opts.frame_p25p1 = 1;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 1;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "Legacy Auto");
            }
          else if (optarg[0] == 'd')
            {
              opts.frame_dstar = 1;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              state.rf_mod = 0;
              sprintf (opts.output_name, "D-STAR");
              fprintf (stderr,"Decoding only D-STAR frames.\n");
            }
          else if (optarg[0] == 'x')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 1;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "X2-TDMA");
              fprintf (stderr,"Decoding only X2-TDMA frames.\n");
            }
          else if (optarg[0] == 'p')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 1;
              state.samplesPerSymbol = 5;
              state.symbolCenter = 2;
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "ProVoice");
              fprintf (stderr,"Setting symbol rate to 9600 / second\n");
              fprintf (stderr,"Decoding only ProVoice frames.\n");
            }
          else if (optarg[0] == '1')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 1;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0; //
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              sprintf (opts.output_name, "P25P1");
              fprintf (stderr,"Decoding only P25 Phase 1 frames.\n");
            }
          else if (optarg[0] == 'i')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 1;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.frame_ysf = 0;
              state.samplesPerSymbol = 20;
              state.symbolCenter = 10;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "NXDN48");
              fprintf (stderr,"Setting symbol rate to 2400 / second\n");
              fprintf (stderr,"Decoding only NXDN 4800 baud frames.\n");
            }
          else if (optarg[0] == 'y')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.frame_ysf = 1;
              state.samplesPerSymbol = 20; //10
              state.symbolCenter = 10;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "YSF");
              fprintf (stderr,"Setting symbol rate to 2400 / second\n");
              fprintf (stderr,"Decoding only YSF frames.\nNot working yet!\n");
              }
              else if (optarg[0] == '2')
                {
                  opts.frame_dstar = 0;
                  opts.frame_x2tdma = 0;
                  opts.frame_p25p1 = 0;
                  opts.frame_p25p2 = 1;
                  opts.frame_nxdn48 = 0;
                  opts.frame_nxdn96 = 0;
                  opts.frame_dmr = 0;
                  opts.frame_dpmr = 0;
                  opts.frame_provoice = 0;
                  opts.frame_ysf = 0;
                  state.samplesPerSymbol = 20; //10
                  state.symbolCenter = 10;
                  opts.mod_c4fm = 0;
                  opts.mod_qpsk = 1;
                  opts.mod_gfsk = 0;
                  state.rf_mod = 0;
                  opts.dmr_stereo = 1;
                  state.dmr_stereo = 1;
                  sprintf (opts.output_name, "P25-P2");
                  fprintf (stderr,"Decoding P25-P2 frames C4FM or OP25 Symbol Captures!\n");
                  }
                  else if (optarg[0] == 't')
                    {
                      opts.frame_dstar = 0;
                      opts.frame_x2tdma = 1;
                      opts.frame_p25p1 = 1;
                      opts.frame_p25p2 = 1;
                      opts.inverted_p2 = 0;
                      opts.frame_nxdn48 = 0;
                      opts.frame_nxdn96 = 0;
                      opts.frame_dmr = 1;
                      opts.frame_dpmr = 0;
                      opts.frame_provoice = 0;
                      opts.frame_ysf = 0;
                      opts.mod_c4fm = 1;
                      opts.mod_qpsk = 0;
                      opts.mod_gfsk = 0;
                      //Need a new demodulator is needed to handle p2 cqpsk
                      //or consider using an external modulator (GNURadio?)
                      state.rf_mod = 0;
                      opts.dmr_stereo = 1;
                      //state.dmr_stereo = 1;
                      opts.pulse_digi_rate_out = 24000;
                      opts.pulse_digi_out_channels = 2;
                      sprintf (opts.output_name, "XDMA");
                      fprintf (stderr,"Decoding XDMA P25 and DMR\n");
                      }
          else if (optarg[0] == 'n')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 1;
              opts.frame_dmr = 0;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "NXDN96");
              fprintf (stderr,"Decoding only NXDN 9600 baud frames.\n");
            }
          else if (optarg[0] == 'r')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_p25p2 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 1;
              opts.frame_dpmr = 0;
              opts.frame_provoice = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0; //
              state.rf_mod = 0;  //
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              sprintf (opts.output_name, "DMR LEH");
              fprintf(stderr, "Notice: DMR cannot autodetect polarity. \n Use -xr option if Inverted Signal expected.\n");
              fprintf (stderr,"Decoding only DMR frames.\n");
            }
          else if (optarg[0] == 'm')
          {
            opts.frame_dstar = 0;
            opts.frame_x2tdma = 0;
            opts.frame_p25p1 = 0;
            opts.frame_p25p2 = 0;
            opts.frame_nxdn48 = 0;
            opts.frame_nxdn96 = 0;
            opts.frame_dmr = 0;
            opts.frame_provoice = 0;
            opts.frame_dpmr = 1;
            opts.frame_ysf = 0; //testing with NXDN48 parameters
            state.samplesPerSymbol = 20; //same as NXDN48 - 20
            state.symbolCenter = 10; //same as NXDN48 - 10
            opts.mod_c4fm = 1;
            opts.mod_qpsk = 0;
            opts.mod_gfsk = 0;
            state.rf_mod = 0;
            opts.pulse_digi_rate_out = 8000;
            opts.pulse_digi_out_channels = 1;
            opts.dmr_stereo = 0;
            state.dmr_stereo = 0;
            sprintf (opts.output_name, "dPMR");
            fprintf(stderr, "Notice: dPMR cannot autodetect polarity. \n Use -xd option if Inverted Signal expected.\n");
            fprintf(stderr, "Decoding only dPMR frames.\n");
          }
          break;
        //don't mess with the modulations, doesn't really do anything meaningful anyways
        case 'm':
          if (optarg[0] == 'a')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 1;
              state.rf_mod = 0;
            }
          else if (optarg[0] == 'c')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              fprintf (stderr,"Enabling only C4FM modulation optimizations.\n");
            }
          else if (optarg[0] == 'g')
            {
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              fprintf (stderr,"Enabling only GFSK modulation optimizations.\n");
            }
          else if (optarg[0] == 'q')
            {
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 0;
              state.rf_mod = 1;
              fprintf (stderr,"Enabling only QPSK modulation optimizations.\n");
            }
          break;
        case 'u':
          sscanf (optarg, "%i", &opts.uvquality);
          if (opts.uvquality < 1)
            {
              opts.uvquality = 1;
            }
          else if (opts.uvquality > 64)
            {
              opts.uvquality = 64;
            }
          fprintf (stderr,"Setting unvoice speech quality to %i waves per band.\n", opts.uvquality);
          break;
        case 'x':
          if (optarg[0] == 'x')
            {
              opts.inverted_x2tdma = 0;
              fprintf (stderr,"Expecting non-inverted X2-TDMA signals.\n");
            }
          else if (optarg[0] == 'r')
            {
              opts.inverted_dmr = 1;
              fprintf (stderr,"Expecting inverted DMR signals.\n");
            }
          else if (optarg[0] == 'd')
            {
              opts.inverted_dpmr = 1;
              fprintf (stderr, "Expecting inverted ICOM dPMR signals.\n");
            }
          break;
        case 'A':
          sscanf (optarg, "%i", &opts.mod_threshold);
          fprintf (stderr,"Setting C4FM/QPSK auto detection threshold to %i\n", opts.mod_threshold);
        case 'S':
          sscanf (optarg, "%i", &opts.ssize);
          if (opts.ssize > 128)
            {
              opts.ssize = 128;
            }
          else if (opts.ssize < 1)
            {
              opts.ssize = 1;
            }
          fprintf (stderr,"Setting QPSK symbol buffer to %i\n", opts.ssize);
          break;
        case 'M':
          sscanf (optarg, "%i", &opts.msize);
          if (opts.msize > 1024)
            {
              opts.msize = 1024;
            }
          else if (opts.msize < 1)
            {
              opts.msize = 1;
            }
          fprintf (stderr,"Setting QPSK Min/Max buffer to %i\n", opts.msize);
          break;
        case 'r':
          opts.playfiles = 1;
          opts.errorbars = 0;
          opts.datascope = 0;
          opts.pulse_digi_rate_out = 8000;
          opts.pulse_digi_out_channels = 1;
          opts.dmr_stereo = 0;
          state.dmr_stereo = 0;
          sprintf (opts.output_name, "MBE Playback");
          state.optind = optind;
          break;
        case 'l':
          opts.use_cosine_filter = 0;
          break;
        default:
          usage ();
          exit (0);
        }
    }

    if (opts.resume > 0)
    {
      openSerial (&opts, &state);
    }

    if((strncmp(opts.audio_in_dev, "pulse", 5) == 0))
    {
      opts.audio_in_type == 0;
    }

    if((strncmp(opts.audio_out_dev, "pulse", 5) == 0))
    {
      opts.audio_out_type == 0;
    }

    if (opts.playfiles == 1)
    {
      opts.split = 1;
      opts.playoffset = 0;
      opts.playoffsetR = 0;
      opts.delay = 0;
      if (strlen(opts.wav_out_file) > 0)
        {
          openWavOutFile (&opts, &state);
        }
      else
        {
          openAudioOutDevice (&opts, SAMPLE_RATE_OUT);
        }
    }
    else if (strcmp (opts.audio_in_dev, opts.audio_out_dev) != 0)
    {
      opts.split = 1;
      opts.playoffset = 0;
      opts.playoffsetR = 0;
      opts.delay = 0;
      if (strlen(opts.wav_out_file) > 0)
        {
          openWavOutFile (&opts, &state);
        }
      else
        {
          openAudioOutDevice (&opts, SAMPLE_RATE_OUT);
        }
      openAudioInDevice (&opts);


      fprintf (stderr,"Press CTRL + C twice to close.\n");
    }

  else
    {
      opts.split = 0;
      opts.playoffset = 25;
      opts.playoffsetR = 25;
      opts.delay = 0;
      openAudioInDevice (&opts);
      opts.audio_out_fd = opts.audio_in_fd;
    }

  if (opts.playfiles == 1)
    {
      opts.pulse_digi_rate_out = 8000;
      opts.pulse_digi_out_channels = 1;
      openPulseOutput(&opts); //need to open it up for output
      playMbeFiles (&opts, &state, argc, argv);
    }

  else
    {
        liveScanner (&opts, &state);
    }

  cleanupAndExit (&opts, &state);

  return (0);
}
