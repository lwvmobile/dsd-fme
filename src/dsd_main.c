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
  "                                                        "
};

int comp (const void *a, const void *b)
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
char dsp_filename[1024] = {0};
unsigned long long int p2vars = 0;

char * pEnd; //bugfix

void
noCarrier (dsd_opts * opts, dsd_state * state)
{

  //tune back to last known CC when using trunking after x second hangtime 
  if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && ( (time(NULL) - state->last_cc_sync_time) > opts->trunk_hangtime) ) 
  {
    if (state->p25_cc_freq != 0) 
    {

      //cap+ rest channel - redundant?
      if (state->dmr_rest_channel != -1)
      {
        if (state->trunk_chan_map[state->dmr_rest_channel] != 0)
        {
          state->p25_cc_freq = state->trunk_chan_map[state->dmr_rest_channel];
        }
      } 

      if (opts->use_rigctl == 1) //rigctl tuning
      {
        if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
        SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
        state->dmr_rest_channel = -1; //maybe?
      }

      else if (opts->audio_in_type == 3) //rtl_fm tuning
      {
        //UDP command to tune the RTL dongle
        rtl_udp_tune(opts, state, state->p25_cc_freq);
        state->dmr_rest_channel = -1; //maybe?
      }

      opts->p25_is_tuned = 0; 
      state->edacs_tuned_lcn = -1;

      //only for EDACS/PV
      if (opts->frame_provoice == 1 && opts->wav_out_file != NULL)
      {
        closeWavOutFile(opts, state);
      }
      state->last_cc_sync_time = time(NULL);
      //test to switch back to 10/4 P1 QPSK for P25 FDMA CC
      if (opts->mod_qpsk == 1 && state->p25_cc_is_tdma == 0)
      {
        state->samplesPerSymbol = 10;
        state->symbolCenter = 4;
      }
    }
    //zero out vc frequencies?
    state->p25_vc_freq[0] = 0;
    state->p25_vc_freq[1] = 0;

    state->is_con_plus = 0; //flag off
  }

  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  //dmr buffer
  state->dmr_payload_p = state->dibit_buf + 200;
  memset (state->dmr_payload_buf, 0, sizeof (int) * 200);
  //dmr buffer end

  //close MBE out files
  if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
  if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

  state->jitter = -1;
  state->lastsynctype = -1;
  state->carrier = 0;
  state->max = 15000;
  state->min = -15000;
  state->center = 0; 
  state->err_str[0] = 0;
  state->err_strR[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->errs = 0;
  state->errs2 = 0;

  //zero out right away if not trunking
  if (opts->p25_trunk == 0)
  {
    state->lasttg = 0;
    state->lastsrc = 0;
    state->lasttgR = 0;
    state->lastsrcR = 0;

    //zero out vc frequencies?
    state->p25_vc_freq[0] = 0;
    state->p25_vc_freq[1] = 0;

    //only reset cap+ rest channel if not trunking
    state->dmr_rest_channel = -1;

    //zero out nxdn site/srv/cch info if not trunking
    state->nxdn_location_site_code = 0;
    state->nxdn_location_sys_code = 0;
    sprintf (state->nxdn_location_category, "%s", " ");

    //dmr mfid branding and site parms
    sprintf(state->dmr_branding_sub, "%s", "");
    sprintf(state->dmr_branding, "%s", "");
    sprintf (state->dmr_site_parms, "%s", ""); 
  }

  //zero out after x second hangtime when trunking to prevent premature zeroing on these variables
  //mainly bugfix for ncurses and per call wavs (edacs) and also signal fade, etc
  if (opts->p25_trunk == 1 && opts->p25_is_tuned == 1 && time(NULL) - state->last_cc_sync_time > opts->trunk_hangtime) 
  {
    state->lasttg = 0;
    state->lastsrc = 0;
    state->lasttgR = 0;
    state->lastsrcR = 0;

  }
    
  state->lastp25type = 0;
  state->repeat = 0;
  state->nac = 0;
  state->numtdulc = 0;
  sprintf (state->slot1light, "%s", "");
  sprintf (state->slot2light, "%s", "");
  state->firstframe = 0;
  if (opts->audio_gain == (float) 0)
  {
    if (state->audio_smoothing == 0) state->aout_gain = 15; //15
    else state->aout_gain = 25; //25
  }

  if (opts->audio_gainR == (float) 0)
  {
    if (state->audio_smoothing == 0) state->aout_gainR = 15; //15
    else state->aout_gainR = 25; //25
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

  state->HYTL = 0;
  state->HYTR = 0;
  state->DMRvcL = 0;
  state->DMRvcR = 0;
  state->dropL = 256; 
  state->dropR = 256; 

  state->payload_miN = 0;
  state->p25vc = 0;
  state->payload_miP = 0;

  //initialize dmr data header source
  state->dmr_lrrp_source[0] = 0;
  state->dmr_lrrp_source[1] = 0;


  //initialize data header bits
  state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
  state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
  state->data_header_padding[0] = 0;
  state->data_header_padding[1] = 0;
  state->data_header_format[0] = 7;
  state->data_header_format[1] = 7;
  state->data_header_sap[0] = 0;
  state->data_header_sap[1] = 0;
  state->data_block_counter[0] = 1;
  state->data_block_counter[1] = 1;
  state->data_p_head[0] = 0;
  state->data_p_head[1] = 0;

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

  //values displayed in ncurses terminal
  state->p25_vc_freq[0] = 0;
  state->p25_vc_freq[1] = 0;

  //new nxdn stuff
  state->nxdn_part_of_frame = 0;
  state->nxdn_ran = 0;
  state->nxdn_sf = 0;
  memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc)); //init on 1, bad CRC all
  state->nxdn_sacch_non_superframe = TRUE; 
  memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
  state->nxdn_alias_block_number = 0;
  memset (state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));
  sprintf (state->nxdn_call_type, "%s", "");

  //dmr overaching manufacturer in use for a particular system or radio  
  // state->dmr_mfid = -1;

  //dmr slco stuff
  memset(state->dmr_cach_fragment, 1, sizeof(state->dmr_cach_fragment));
  state->dmr_cach_counter = 0;

  //initialize unified dmr pdu 'superframe'
  memset (state->dmr_pdu_sf, 0, sizeof (state->dmr_pdu_sf));
  memset (state->data_header_valid, 0, sizeof(state->data_header_valid));

  //initialize cap+ bits and block num storage
  memset (state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));  
  state->cap_plus_block_num = 0;

  //init confirmed data individual block crc as invalid
  memset (state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));

  //embedded signalling
  memset(state->dmr_embedded_signalling, 0, sizeof(state->dmr_embedded_signalling));

  //late entry mi fragments
  memset (state->late_entry_mi_fragment, 0, sizeof (state->late_entry_mi_fragment));

  //dmr talker alias new/fixed stuff
  memset(state->dmr_alias_format, 0, sizeof(state->dmr_alias_format));
  memset(state->dmr_alias_len, 0, sizeof(state->dmr_alias_len));
  memset(state->dmr_alias_block_segment, 0, sizeof(state->dmr_alias_block_segment));
  memset(state->dmr_embedded_gps, 0, sizeof(state->dmr_embedded_gps));
  memset(state->dmr_lrrp_gps, 0, sizeof(state->dmr_lrrp_gps));

  //REMUS! multi-purpose call_string
  sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  sprintf (state->call_string[1], "%s", "                     "); //21 spaces

  if (time(NULL) - state->last_cc_sync_time > 30) //thirty seconds of no carrier
  {
    state->dmr_rest_channel = -1;
    state->p25_vc_freq[0] = 0;
    state->p25_vc_freq[1] = 0;
    state->dmr_mfid = -1; 
    sprintf(state->dmr_branding_sub, "%s", "");
    sprintf(state->dmr_branding, "%s", "");
    sprintf (state->dmr_site_parms, "%s", ""); 
    opts->p25_is_tuned = 0;
  }

  opts->dPMR_next_part_of_superframe = 0;

  state->dPMRVoiceFS2Frame.CalledIDOk  = 0;
  state->dPMRVoiceFS2Frame.CallingIDOk = 0;
  memset(state->dPMRVoiceFS2Frame.CalledID, 0, 8);
  memset(state->dPMRVoiceFS2Frame.CallingID, 0, 8);
  memset(state->dPMRVoiceFS2Frame.Version, 0, 8);

  sprintf (state->dpmr_caller_id, "%s", "      ");
  sprintf (state->dpmr_target_id, "%s", "      ");

} //nocarrier

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
  opts->mbe_out_fileR[0] = 0; //second slot on a TDMA system
  opts->mbe_out_path[0] = 0;
  opts->mbe_out_f = NULL;
  opts->mbe_out_fR = NULL; //second slot on a TDMA system
  opts->audio_gain = 0; //0
  opts->audio_gainR = 0; //0
  opts->audio_out = 1;
  opts->wav_out_file[0] = 0;
  opts->wav_out_fileR[0] = 0;
  opts->wav_out_file_raw[0] = 0;
  opts->symbol_out_file[0] = 0;
  opts->lrrp_out_file[0] = 0;
  //csv import filenames
  opts->group_in_file[0] = 0;
  opts->lcn_in_file[0] = 0;
  opts->chan_in_file[0] = 0;
  opts->key_in_file[0] = 0;
  //end import filenames
  opts->szNumbers[0] = 0;
  opts->symbol_out_f = NULL;
  opts->symbol_out = 0;
  opts->mbe_out = 0;
  opts->mbe_outR = 0; //second slot on a TDMA system
  opts->wav_out_f = NULL;
  opts->wav_out_fR = NULL;
  opts->wav_out_raw = NULL;

  opts->dmr_stereo_wav = 0; //flag for per call dmr stereo wav recordings
  //opts->wav_out_fd = -1;
  opts->serial_baud = 115200;
  sprintf (opts->serial_dev, "/dev/ttyUSB0");
  opts->resume = 0;
  opts->frame_dstar = 0;
  opts->frame_x2tdma = 0;
  opts->frame_p25p1 = 1;
  opts->frame_p25p2 = 1;
  opts->frame_nxdn48 = 0;
  opts->frame_nxdn96 = 0;
  opts->frame_dmr = 1;
  opts->frame_dpmr = 0;
  opts->frame_provoice = 0;
  opts->frame_ysf = 0; //forgot to init this, and Cygwin treated as it was turned on.
  opts->mod_c4fm = 1;
  opts->mod_qpsk = 0;
  opts->mod_gfsk = 0;
  opts->uvquality = 3;
  opts->inverted_x2tdma = 1;    // most transmitter + scanner + sound card combinations show inverted signals for this
  opts->inverted_dmr = 0;       // most transmitter + scanner + sound card combinations show non-inverted signals for this
  opts->mod_threshold = 26;
  opts->ssize = 128; //36 default, max is 128, much cleaner data decodes on Phase 2 cqpsk at max
  opts->msize = 1024; //15 default, max is 1024, much cleaner data decodes on Phase 2 cqpsk at max
  opts->playfiles = 0;
  opts->delay = 0;
  opts->use_cosine_filter = 1;
  opts->unmute_encrypted_p25 = 0;
  opts->rtl_dev_index = 0;        //choose which device we want by index number
  opts->rtl_gain_value = 26;    //mid value, 0 - AGC (not recommended) 49 highest value
  opts->rtl_squelch_level = 0; //fully open by default, want to specify level for things like NXDN with false positives
  opts->rtl_volume_multiplier = 1; //sample multiplier; 1 seems like a good value
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

  opts->wav_sample_rate = 48000; //default value (DSDPlus uses 96000 on raw signal wav files)
  opts->wav_interpolator = 1; //default factor of 1 on 48000; 2 on 96000; sample rate / decimator
  opts->wav_decimator = 48000; //maybe for future use?

  sprintf (opts->output_name, "XDMA");
  opts->pulse_flush = 1; //set 0 to flush, 1 for flushed
  opts->use_ncurses_terminal = 0;
  opts->ncurses_compact = 0;
  opts->ncurses_history = 1;
  opts->payload = 0;
  opts->inverted_dpmr = 0;
  opts->dmr_mono = 0;
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

  opts->call_alert = 0; //call alert beeper for ncurses

  //rigctl options
  opts->use_rigctl = 0;
  opts->rigctl_sockfd = 0;
  opts->rigctlportno = 7356; //TCP Port Number; GQRX - 7356; SDR++ - 4532
  sprintf (opts->rigctlhostname, "%s", "localhost");

  //udp input options
  opts->udp_sockfd = 0;
  opts->udp_portno = 7355; //default favored by GQRX and SDR++
  opts->udp_hostname = "localhost";

  //tcp input options
  opts->tcp_sockfd = 0;
  opts->tcp_portno = 7355; //default favored by SDR++
  sprintf (opts->tcp_hostname, "%s", "localhost");

  opts->p25_trunk = 0; //0 disabled, 1 is enabled
  opts->p25_is_tuned = 0; //set to 1 if currently on VC, set back to 0 on carrier drop
  opts->trunk_hangtime = 1; //1 second hangtime by default before tuning back to CC

  //reverse mute 
  opts->reverse_mute = 0;

  //setmod bandwidth
  opts->setmod_bw = 0; //default to 0 - off

  //DMR Location Area - DMRLA B***S***
  opts->dmr_dmrla_is_set = 0;
  opts->dmr_dmrla_n = 0;

  //Trunking - Use Group List as Allow List
  opts->trunk_use_allow_list = 0; //disabled by default

  //Trunking - Tune Group Calls
  opts->trunk_tune_group_calls = 1; //enabled by default

  //Trunking - Tune Private Calls
  opts->trunk_tune_private_calls = 1; //enabled by default

  //Trunking - Tune Data Calls
  opts->trunk_tune_data_calls = 0; //disabled by default

  opts->dPMR_next_part_of_superframe = 0;

  //dsp structured file
  opts->dsp_out_file[0] = 0;
  opts->use_dsp_output = 0;

} //initopts

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

  //Upsampled Audio Smoothing
  state->audio_smoothing = 0;

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
  state->lasttgR = 0;
  state->lastsrcR = 0;
  state->nac = 0;
  state->errs = 0;
  state->errs2 = 0;
  state->mbe_file_type = -1;
  state->optind = 0;
  state->numtdulc = 0;
  state->firstframe = 0;
  sprintf (state->slot1light, "%s", "");
  sprintf (state->slot2light, "%s", "");
  state->aout_gain = 0;
  state->aout_gainR = 0;
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
  state->dmr_so    = 0;
  state->dmr_soR   = 0;
  state->dmr_fid   = 0;
  state->dmr_fidR  = 0;
  state->dmr_flco  = 0;
  state->dmr_flcoR = 0;
  state->dmr_ms_mode = 0;

  state->HYTL = 0;
  state->HYTR = 0;
  state->DMRvcL = 0;
  state->DMRvcR = 0;
  state->dropL = 256; 
  state->dropR = 256;

  state->p25vc = 0;

  state->payload_miP = 0;

  memset(state->dstarradioheader, 0, 41);

  //initialize dmr data header source
  state->dmr_lrrp_source[0] = 0;
  state->dmr_lrrp_source[1] = 0;


  //initialize data header bits
  state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
  state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
  state->data_header_padding[0] = 0;
  state->data_header_padding[1] = 0;
  state->data_header_format[0] = 7;
  state->data_header_format[1] = 7;
  state->data_header_sap[0] = 0;
  state->data_header_sap[1] = 0;
  state->data_block_counter[0] = 1;
  state->data_block_counter[1] = 1;
  state->data_p_head[0] = 0;
  state->data_p_head[1] = 0;

  state->menuopen = 0; //is the ncurses menu open, if so, don't process frame sync

  state->dmr_encL = 0;
  state->dmr_encR = 0;

  //P2 variables
  state->p2_wacn = 0;
  state->p2_sysid = 0;
  state->p2_cc = 0;
  state->p2_siteid = 0;
  state->p2_rfssid = 0;
  state->p2_hardset = 0;
  state->p2_is_lcch = 0;
  state->p25_cc_is_tdma = 2; //init on 2, TSBK NET_STS will set 0, TDMA NET_STS will set 1. //used to determine if we need to change symbol rate when cc hunting

  //experimental symbol file capture read throttle
  state->symbol_throttle = 0; //throttle speed
  state->use_throttle = 0; //only use throttle if set to 1

  state->p2_scramble_offset = 0;
  state->p2_vch_chan_num = 0;

  //p25 iden_up values
  state->p25_chan_iden = 0;
  for (int i = 0; i < 16; i++)
  {
    state->p25_chan_type[i] = 0;
    state->p25_trans_off[i] = 0;
    state->p25_chan_spac[i] = 0;
    state->p25_base_freq[i] = 0;
  }

  //values displayed in ncurses terminal
  state->p25_cc_freq = 0;
  state->p25_vc_freq[0] = 0;
  state->p25_vc_freq[1] = 0;

  //edacs - may need to make these user configurable instead for stability on non-ea systems
  state->ea_mode = -1; //init on -1, 0 is standard, 1 is ea
  state->esk_mode = -1; //same as above, but with esk or not
  state->esk_mask = 0x0; //toggles from 0x0 to 0xA0 if esk mode enabled
  state->edacs_site_id = 0;
  state->edacs_lcn_count = 0;
  state->edacs_cc_lcn = 0;
  state->edacs_vc_lcn = 0;
  state->edacs_tuned_lcn = -1;

  //trunking
  memset (state->trunk_lcn_freq, 0, sizeof(state->trunk_lcn_freq));
  memset (state->trunk_chan_map, 0, sizeof(state->trunk_chan_map));
  state->group_tally = 0;
  state->lcn_freq_count = 0; //number of frequncies imported as an enumerated lcn list
  state->lcn_freq_roll = 0; //needs reset if sync is found?
  state->last_cc_sync_time = time(NULL);
  state->last_vc_sync_time = time(NULL);
  state->is_con_plus = 0;

  //dmr trunking/ncurses stuff 
  state->dmr_rest_channel = -1; //init on -1
  state->dmr_mfid = -1; //

  //new nxdn stuff
  state->nxdn_part_of_frame = 0;
  state->nxdn_ran = 0;
  state->nxdn_sf = 0;
  memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc)); //init on 1, bad CRC all
  state->nxdn_sacch_non_superframe = TRUE; 
  memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
  state->nxdn_alias_block_number = 0;
  memset (state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));
  //site/srv/cch info
  state->nxdn_location_site_code = 0;
  state->nxdn_location_sys_code = 0;
  sprintf (state->nxdn_location_category, "%s", " ");

  //multi-key array
  memset (state->rkey_array, 0, sizeof(state->rkey_array)); 

  //Remus DMR End Call Alert Beep
  state->dmr_end_alert[0] = 0;
  state->dmr_end_alert[1] = 0;

  sprintf (state->dmr_branding, "%s", "");
  sprintf (state->dmr_branding_sub, "%s", "");
  sprintf (state->dmr_site_parms, "%s", "");

  //initialize unified dmr pdu 'superframe'
  memset (state->dmr_pdu_sf, 0, sizeof (state->dmr_pdu_sf));
  memset (state->data_header_valid, 0, sizeof(state->data_header_valid));

  //initialize cap+ bits and block num storage
  memset (state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));
  state->cap_plus_block_num = 0;

  //init confirmed data individual block crc as invalid
  memset (state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));

  //dmr slco stuff
  memset(state->dmr_cach_fragment, 1, sizeof(state->dmr_cach_fragment));
  state->dmr_cach_counter = 0;

  //embedded signalling
  memset(state->dmr_embedded_signalling, 0, sizeof(state->dmr_embedded_signalling));

  //dmr talker alias new/fixed stuff
  memset(state->dmr_alias_format, 0, sizeof(state->dmr_alias_format));
  memset(state->dmr_alias_len, 0, sizeof(state->dmr_alias_len));
  memset(state->dmr_alias_block_segment, 0, sizeof(state->dmr_alias_block_segment));
  memset(state->dmr_embedded_gps, 0, sizeof(state->dmr_embedded_gps));
  memset(state->dmr_lrrp_gps, 0, sizeof(state->dmr_lrrp_gps));

  //REMUS! multi-purpose call_string
  sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  sprintf (state->call_string[1], "%s", "                     "); //21 spaces

  //late entry mi fragments
  memset (state->late_entry_mi_fragment, 0, sizeof (state->late_entry_mi_fragment));
 
  initialize_p25_heuristics(&state->p25_heuristics);

  state->dPMRVoiceFS2Frame.CalledIDOk  = 0;
  state->dPMRVoiceFS2Frame.CallingIDOk = 0;
  memset(state->dPMRVoiceFS2Frame.CalledID, 0, 8);
  memset(state->dPMRVoiceFS2Frame.CallingID, 0, 8);
  memset(state->dPMRVoiceFS2Frame.Version, 0, 8);

  sprintf (state->dpmr_caller_id, "%s", "      ");
  sprintf (state->dpmr_target_id, "%s", "      ");

} //init_state

void
usage ()
{

  printf ("\n");
  printf ("Usage: dsd-fme [options]            Decoder/Trunking Mode\n");
  printf ("  or:  dsd-fme [options] -r <files> Read/Play saved mbe data from file(s)\n");
  printf ("  or:  dsd-fme -h                   Show help\n");
  printf ("\n");
  printf ("Display Options:\n");
  printf ("  -N            Use NCurses Terminal\n");
  printf ("                 dsd-fme -N 2> log.ans \n");
  printf ("  -Z            Log MBE/PDU Payloads to console\n");
  printf ("\n");
  printf ("Input/Output options:\n");
  printf ("  -i <device>   Audio input device (default is pulse audio)\n");
  printf ("                - for piped stdin\n");
  printf ("                rtl:dev:freq:gain:ppm:bw:sq:udp for rtl dongle (see below)\n");
  printf ("                tcp for tcp client SDR++/GNURadio Companion/Other (Port 7355)\n");
  printf ("                tcp:192.168.7.5:7355 for custom address and port \n");
  printf ("                filename.bin for OP25/FME capture bin files\n");
  printf ("                filename.wav for 48K/1 wav files (SDR++, GQRX)\n");
  printf ("                filename.wav -s 96000 for 96K/1 wav files (DSDPlus)\n");
  printf ("                (Use single quotes '/directory/audio file.wav' when directories/spaces are present)\n");
  printf ("  -s <rate>     Sample Rate of wav input files (48000 or 96000) Mono only!\n");
  printf ("  -o <device>   Audio output device (default is pulse audio)(null for no audio output)\n");
  printf ("  -d <dir>      Create mbe data files, use this directory (TDMA version is experimental)\n");
  printf ("  -r <files>    Read/Play saved mbe data from file(s)\n");
  printf ("  -g <num>      Audio output gain (default = 0 = auto, disable = -1)\n");
  printf ("  -w <file>     Output synthesized speech to a .wav file, legacy auto modes only.\n");
  printf ("  -P            Enable Per Call WAV file saving in XDMA and NXDN decoding classes\n");
  printf ("                 (Per Call can only be used in Ncurses Terminal!)\n");
  printf ("                 (Running in console will use static wav files)\n");
  printf ("  -a            Enable Call Alert Beep (NCurses Terminal Only)\n");
  printf ("                 (Warning! Might be annoying.)\n");
  printf ("  -L <file>     Specify Filename for LRRP Data Output.\n");
  printf ("  -Q <file>     Specify Filename for DSP Structured File Output. (placed in DSP folder)\n");
  printf ("  -c <file>     Output symbol capture to .bin file\n");
  printf ("  -q            Reverse Mute - Mute Unencrypted Voice and Unmute Encrypted Voice\n");
  printf ("  -V            Enable Audio Smoothing on Upsampled 48k/1 or 24k/2 Audio (Capital V)\n");
  printf ("                 (Audio Smoothing is now disabled on all upsampled output by default -- fix crackle/buzz bug)\n");
  printf ("\n");
  printf ("RTL-SDR options:\n");
  printf (" WARNING! Old CLI Switch Handling has been depreciated in favor of rtl:<parms>\n");
  printf (" Usage: rtl:dev:freq:gain:ppm:bw:sq:udp\n");
  printf ("  dev  <num>    RTL-SDR Device Index Number\n");
  printf ("  freq <num>    RTL-SDR Frequency (851800000 or 851.8M) \n");
  printf ("  gain <num>    RTL-SDR Device Gain (0-49)(default = 26)(0 = Hardware AGC, not recommended)\n");
  printf ("  ppm  <num>    RTL-SDR PPM Error (default = 0)\n");
  printf ("  bw   <num>    RTL-SDR VFO Bandwidth kHz (default = 12)(6, 8, 12, 24)  \n");
  printf ("  sq   <num>    RTL-SDR Squelch Level (0 - Open, 25 - Little, 50 - Higher)\n");
  printf ("  udp  <num>    RTL-SDR UDP Remote Port (default = 6020)\n");
  printf (" Example: dsd-fme -fp -i rtl:0:851.375M:22:-2:12:0:6021\n");
  printf ("\n");
  printf ("Decoder options:\n");
  printf ("  -fa           Legacy Auto Detection 8k/1 (old methods default)\n");
  printf ("  -ft           XDMA P25 and DMR BS/MS frame types (new default)\n");
  printf ("  -fs           DMR Stereo BS and MS Simplex\n");
  printf ("  -f1           Decode only P25 Phase 1\n");
  printf ("  -fd           Decode only D-STAR\n");
  printf ("  -fr           Decode only DMR Mono - Single Slot Voice\n");
  printf ("  -fx           Decode only X2-TDMA\n");
  printf ("  -fi             Decode only NXDN48* (6.25 kHz) / IDAS*\n");
  printf ("  -fn             Decode only NXDN96* (12.5 kHz)\n");
  printf ("  -fp             Decode only EDACS/ProVoice*\n");
  printf ("  -fm             Decode only dPMR*\n");
  printf ("  -l            Disable DMR, dPMR, and NXDN input filtering\n");
  printf ("  -u <num>      Unvoiced speech quality (default=3)\n");
  printf ("  -xx           Expect non-inverted X2-TDMA signal\n");
  printf ("  -xr           Expect inverted DMR signal\n");
  printf ("  -xd           Expect inverted ICOM dPMR signal\n");
  printf ("\n");
  printf ("  * denotes frame types that cannot be auto-detected.\n");
  printf ("\n");
  printf ("Advanced Decoder options:\n");
  printf ("  -X <hex>      Manually Set P2 Parameters (WACN, SYSID, CC/NAC)\n");
  printf ("                 (-X BEE00ABC123)\n");
  printf ("  -D <dec>      Manually Set TIII DMR Location Area n bit len (0-10)(10 max)\n");
  printf ("                 (Value defaults to max n bit value for site model size)\n");
  printf ("                 (Setting 0 will show full Site ID, no area/subarea)\n");
  printf ("\n");
  printf ("  -A <num>      QPSK modulation auto detection threshold (default=26)\n");
  printf ("  -S <num>      Symbol buffer size for QPSK decision point tracking\n");
  printf ("                 (default=36)\n");
  printf ("  -M <num>      Min/Max buffer size for QPSK decision point tracking\n");
  printf ("                 (default=15)\n");
  printf ("  -ma           Auto-select modulation optimizations (default)\n");
  printf ("  -mc           Use only C4FM modulation optimizations\n");
  printf ("  -mg           Use only GFSK modulation optimizations\n");
  printf ("  -mq           Use only QPSK modulation optimizations\n");
  printf ("  -m2           Use P25p2 6000 sps CQPSK modulation optimizations\n");
  //printf ("                 (4 Level, not 8 Level LSM) (this is honestly unknown since I can't verify what local systems are using)\n");
  printf ("  -F            Relax P25 Phase 2 MAC_SIGNAL CRC Checksum Pass/Fail\n");
  printf ("                 Use this feature to allow MAC_SIGNAL even if CRC errors.\n");
  printf ("  -F            Relax DMR RAS/CRC CSBK/DATA Pass/Fail\n");
  printf ("                 Enabling on some systems could lead to bad voice/data decoding if bad or marginal signal\n");
  printf ("\n");
  printf ("  -K <dec>      Manually Enter Basic Privacy Key (Decimal Value of Key Number)\n");
  printf ("\n");
  printf ("  -H <hex>      Manually Enter **tera 10/32/64 Char Privacy Hex Key (see example below)\n");
  printf ("                 Encapulate in Single Quotation Marks; Space every 16 chars.\n");
  printf ("                 -H 0B57935150 \n");
  printf ("                 -H '736B9A9C5645288B 243AD5CB8701EF8A' \n");
  printf ("                 -H '20029736A5D91042 C923EB0697484433 005EFC58A1905195 E28E9C7836AA2DB8' \n");
  printf ("\n");
  printf ("  -R <dec>      Manually Enter dPMR or NXDN EHR Scrambler Key Value (Decimal Value)\n");
  printf ("  -k <file>     Import NXDN Scrambler Key List from csv file.\n");
  printf ("                 \n");
  printf ("  -4            Force DMR Privacy Key over FID and SVC bits \n");
  printf ("\n");
  printf (" Trunking Options:\n");
  printf ("  -C <file>     Import Channel to Frequency Map (channum, freq) from csv file. (Capital C)                   \n");
  printf ("                 (See channel_map.csv for example)\n");
  printf ("  -G <file>     Import Group List Allow/Block and Label from csv file.\n");
  printf ("                 (See group.csv for example)\n");
  printf ("  -T            Enable Trunking Features (NXDN/P25/EDACS/DMR) with RIGCTL/TCP or RTL Input\n");
  printf ("  -W            Use Imported Group List as a Trunking Allow/White List -- Only Tune with Mode A\n");
  printf ("  -p            Disable Tune to Private Calls (DMR TIII and P25)\n");
  printf ("  -E            Disable Tune to Group Calls (DMR TIII, Con+, Cap+ and P25)\n");
  printf ("                 (NOTE: NXDN and EDACS, simply disable trunking if desired) \n");
  printf ("  -e            Enable Tune to Data Calls (DMR TIII)\n");
  printf ("                 (NOTE: P25 Data Channels Not Enabled (no handling) \n");
  printf ("  -U <port>     Enable RIGCTL/TCP; Set TCP Port for RIGCTL. (4532 on SDR++)\n");
  printf ("  -B <Hertz>    Set RIGCTL Setmod Bandwidth in Hertz (0 - default - OFF)\n");
  printf ("                 P25 - 7000-12000; P25 (QPSK) - 12000; NXDN48 - 4000; DMR - 7000; EDACS/PV - 12500;\n");
  printf ("                 May vary based on system stregnth, etc.\n");
  printf ("  -t <secs>     Set Trunking VC/sync loss hangtime in seconds. (default = 1 second)\n");
  printf ("\n");
  printf (" Trunking Example TCP: dsd-fme -fs -i tcp -U 4532 -T -C dmr_t3_chan.csv -G group.csv -N 2> log.ans\n");
  printf (" Trunking Example RTL: dsd-fme -fs -i rtl:0:450M:26:-2:8:0:6020 -T -C connect_plus_chan.csv -G group.csv -N 2> log.ans\n");
  printf ("\n");
  exit (0);
}

void
liveScanner (dsd_opts * opts, dsd_state * state)
{

if (opts->audio_in_type == 1)
{
  opts->pulse_digi_rate_out = 48000; 
  opts->pulse_digi_out_channels = 1;
  if (opts->dmr_stereo == 1)
  {
    opts->pulse_digi_rate_out = 24000; 
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
    opts->pulse_digi_rate_out = 48000; //revert to 8K/1 for RTL input, random crackling otherwise
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
  
  //close MBE out files
  if (opts->mbe_out_f != NULL) closeMbeOutFile (opts, state);
  if (opts->mbe_out_fR != NULL) closeMbeOutFileR (opts, state);

  fprintf (stderr,"\n");
  fprintf (stderr,"Total audio errors: %i\n", state->debug_audio_errors);
  fprintf (stderr,"Total header errors: %i\n", state->debug_header_errors);
  fprintf (stderr,"Total irrecoverable header errors: %i\n", state->debug_header_critical_errors);
  fprintf (stderr,"Exiting.\n");
  exit (0);
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
  for (short int i = 1; i < 8; i++) {
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

  while ((c = getopt (argc, argv, "haepPqs:t:v:z:i:o:d:c:g:nw:B:C:R:f:m:u:x:A:S:M:G:D:L:VU:Y:K:H:X:NQ:WrlZTF01:2:345:6:7:89:Ek:")) != -1)
    {
      opterr = 0;
      switch (c)
        {
        case 'h':
          usage ();
          exit (0);
        case 'a':
          opts.call_alert = 1;
          break;

        //Free'd up switches include Y,z
        //make sure to put a colon : after each if they need an argument
        //or remove colon if no argument required

        case 'k': //NXDN multi-key loader
          strncpy(opts.key_in_file, optarg, 1023);
          opts.key_in_file[1023] = '\0';
          csvKeyImport(&opts, &state);
          break;

        case 'Q': //'DSP' Structured Output file for OKDMRlib
        sprintf (wav_file_directory, "./DSP"); 
        wav_file_directory[1023] = '\0';
        if (stat(wav_file_directory, &st) == -1)
        {
          fprintf (stderr, "-Q %s DSP file directory does not exist\n", wav_file_directory);
          fprintf (stderr, "Creating directory %s to save DSP Structured files\n", wav_file_directory);
          mkdir(wav_file_directory, 0700); //user read write execute, needs execute for some reason or segfault
        }
        //read in filename
        //sprintf (opts.wav_out_file, "./WAV/DSD-FME-X1.wav");
        strncpy(dsp_filename, optarg, 1023);
        sprintf(opts.dsp_out_file, "%s/%s", wav_file_directory, dsp_filename);
        fprintf (stderr, "Saving DSP Structured files to %s\n", opts.dsp_out_file);
        opts.use_dsp_output = 1;
        break;

        //Enable Audio Smoothing for Upsampled Audio
        case '0': 
        case 'V':
          state.audio_smoothing = 1;
          break; 

        //Trunking - Use Group List as Allow List
        case 'W':
          opts.trunk_use_allow_list = 1;
          fprintf (stderr, "Using Group List as Allow/White List. \n");
          break;

        //Trunking - Tune Group Calls
        case 'E':
          opts.trunk_tune_group_calls = 0; //disable
          fprintf (stderr, "Disable Tuning to Group Calls. \n");
          break;

        //Trunking - Tune Private Calls
        case 'p':
          opts.trunk_tune_private_calls = 0; //disable
          fprintf (stderr, "Disable Tuning to Private Calls. \n");
          break;

        //Trunking - Tune Data Calls
        case 'e':
          opts.trunk_tune_data_calls = 1; //enable
          fprintf (stderr, "Enable Tuning to Data Calls. \n");
          break;

        case 'D': //user set DMRLA n value
          sscanf (optarg, "%c", &opts.dmr_dmrla_n);
          if (opts.dmr_dmrla_n > 10) opts.dmr_dmrla_n = 10; //max out at 10;
          // if (opts.dmr_dmrla_n != 0) opts.dmr_dmrla_is_set = 1; //zero will fix a capmax site id value...I think
          opts.dmr_dmrla_is_set = 1;
          fprintf (stderr, "DMRLA n value set to %d. \n", opts.dmr_dmrla_n);
          break;

        case 'C': //new letter assignment for Channel import, flow down to allow temp numbers
        case '1': //LCN to Frequency CSV Import <--> flow down to chanimport now, tell users to use a channel map for edacs
        case '7': //Channel Map Frequency CSV Import
          strncpy(opts.chan_in_file, optarg, 1023);
          opts.chan_in_file[1023] = '\0';
          csvChanImport (&opts, &state);
          break;

        case 'G': //new letter assignment for group import, flow down to allow temp numbers
        case '2': //Group CSV Import
          strncpy(opts.group_in_file, optarg, 1023);
          opts.group_in_file[1023] = '\0';
          csvGroupImport(&opts, &state);
          break;

        case 'T': //new letter assignment for trunking, flow down to allow temp numbers
        case '3':
          opts.p25_trunk = 1;
          break;

        case 'U': //New letter assignment for RIGCTL TCP port, flow down to allow temp numbers
        case '5': //RIGCTL TCP port
          sscanf (optarg, "%d", &opts.rigctlportno);
          if (opts.rigctlportno != 0) opts.use_rigctl = 1; 
          break;

        case 't': //New letter assignment for Trunk Hangtime, flow down to allow temp numbers
        case '6': //hangtime in seconds, default is 1;
          sscanf (optarg, "%d", &opts.trunk_hangtime);
          break;
        
        case 'q': //New letter assignment for Reverse Mute, flow down to allow temp numbers
        case '8':
          opts.reverse_mute = 1;
          fprintf (stderr, "Reverse Mute\n");
          break;
        
        case 'B': //New letter assignment for RIGCTL SetMod BW, flow down to allow temp numbers
        case '9': //rigctl setmod bandwidth;
          sscanf (optarg, "%d", &opts.setmod_bw);
          if (opts.setmod_bw > 25000) opts.setmod_bw = 25000; //not too high
          break;

        case 's':
          sscanf (optarg, "%d", &opts.wav_sample_rate);
          opts.wav_interpolator = opts.wav_sample_rate / opts.wav_decimator;
          state.samplesPerSymbol = state.samplesPerSymbol * opts.wav_interpolator;
          state.symbolCenter = state.symbolCenter * opts.wav_interpolator;
          break;

        case 'v':
          sscanf (optarg, "%d", &opts.verbose);
          break;

        case 'n': //disable or reclaim?
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
          //new handling for 10/32/64 Char Key
          
          strncpy(opts.szNumbers, optarg, 1023);
          opts.szNumbers[1023] = '\0';
          state.K1 = strtoull (opts.szNumbers, &pEnd, 16);
          state.K2 = strtoull (pEnd, &pEnd, 16);
          state.K3 = strtoull (pEnd, &pEnd, 16);
          state.K4 = strtoull (pEnd, &pEnd, 16); 
          fprintf (stderr, "**tera Key = %016llX %016llX %016llX %016llX\n", state.K1, state.K2, state.K3, state.K4);
          opts.dmr_mute_encL = 0;
          opts.dmr_mute_encR = 0;
          if (state.K1 == 0 && state.K2 == 0 && state.K3 == 0 && state.K4 == 0)
          {
            opts.dmr_mute_encL = 1;
            opts.dmr_mute_encR = 1;
          }
          state.H = state.K1; //shim still required?
          break;

        case '4':
          state.M = 1;
          fprintf (stderr,"Force Privacy Key Priority over FID and SVC bits.\n");
          break;

        //manually set Phase 2 TDMA WACN/SYSID/CC
        case 'X':
          sscanf (optarg, "%llX", &p2vars);
          if (p2vars > 0)
          {
           state.p2_wacn = p2vars >> 24;
           state.p2_sysid = (p2vars >> 12) & 0xFFF;
           state.p2_cc = p2vars & 0xFFF;
          }
          if (state.p2_wacn != 0 && state.p2_sysid != 0 && state.p2_cc != 0)
          {
            state.p2_hardset = 1;
          }
          break;

        case 'N':
          opts.use_ncurses_terminal = 1;
          fprintf (stderr,"Enabling NCurses Terminal.\n");
          break;

        case 'Z':
          opts.payload = 1;
          fprintf (stderr,"Logging Frame Payload to console\n");
          break;

        case 'L': //LRRP output to file
          strncpy(opts.lrrp_out_file, optarg, 1023);
          opts.lrrp_out_file[1023] = '\0';
          opts.lrrp_file_output = 1;
          fprintf (stderr,"Writing + Appending LRRP data to file %s\n", opts.lrrp_out_file);
          break;

        case 'P': //TDMA/NXDN Per Call - was T, now is P
        sprintf (wav_file_directory, "./WAV"); 
        wav_file_directory[1023] = '\0';
        if (stat(wav_file_directory, &st) == -1)
        {
          fprintf (stderr, "-P %s WAV file directory does not exist\n", wav_file_directory);
          fprintf (stderr, "Creating directory %s to save decoded wav files\n", wav_file_directory);
          mkdir(wav_file_directory, 0700); //user read write execute, needs execute for some reason or segfault
        }
        fprintf (stderr,"XDMA and NXDN Per Call Wav File Saving Enabled. (NCurses Terminal Only)\n");
        sprintf (opts.wav_out_file, "./WAV/DSD-FME-X1.wav"); 
        sprintf (opts.wav_out_fileR, "./WAV/DSD-FME-X2.wav");
        opts.dmr_stereo_wav = 1;
        openWavOutFileL (&opts, &state); 
        openWavOutFileR (&opts, &state); 
        break;

        case 'F':
          opts.aggressive_framesync = 0;
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, "Relax P25 Phase 2 MAC_SIGNAL CRC Pass/Fail\n");
          fprintf (stderr, "Relax DMR RAS/CRC CSBK/DATA Pass/Fail\n");
          fprintf (stderr, "%s", KNRM);
          break;

        case 'i':
          strncpy(opts.audio_in_dev, optarg, 2047);
          opts.audio_in_dev[2047] = '\0';
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

        case 'c': //now is symbol capture bin output like FME Lite
          strncpy(opts.symbol_out_file, optarg, 1023);
          opts.symbol_out_file[1023] = '\0';
          fprintf (stderr,"Writing + Appending symbol capture to file %s\n", opts.symbol_out_file);
          opts.symbol_out = 1;
          openSymbolOutFile (&opts, &state);
          break;

        case 'g':
          sscanf (optarg, "%f", &opts.audio_gain);
          if (opts.audio_gain < (float) 0 )
            {
              fprintf (stderr,"Disabling audio out gain setting\n");
              opts.audio_gainR = opts.audio_gain;
            }
          else if (opts.audio_gain == (float) 0)
            {
              opts.audio_gain = (float) 0;
              opts.audio_gainR = opts.audio_gain;
              fprintf (stderr,"Enabling audio out auto-gain\n");
            }
          else
            {
              fprintf (stderr,"Setting audio out gain to %f\n", opts.audio_gain);
              opts.audio_gainR = opts.audio_gain;
              state.aout_gain = opts.audio_gain;
              state.aout_gainR = opts.audio_gain;
            }
          break;

        case 'w':
          strncpy(opts.wav_out_file, optarg, 1023);
          opts.wav_out_file[1023] = '\0';
          fprintf (stderr,"Writing + Appending decoded audio to file %s\n", opts.wav_out_file);
          opts.dmr_stereo_wav = 0;
          openWavOutFile (&opts, &state);
          break;

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
              opts.frame_ysf = 0;
              opts.pulse_digi_rate_out = 8000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              opts.dmr_mono = 1;
              state.dmr_stereo = 0;
              // opts.setmod_bw = 7000;
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
              opts.frame_ysf = 0;
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              opts.dmr_mono = 0;
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
              opts.frame_ysf = 0;
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              opts.dmr_mono = 0;
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
              opts.frame_ysf = 0;
              state.samplesPerSymbol = 5;
              state.symbolCenter = 2;
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              opts.dmr_mono = 0;
              state.dmr_stereo = 0;
              opts.setmod_bw = 12500;
              sprintf (opts.output_name, "EDACS/PV");
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
              opts.frame_ysf = 0;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0; //
              opts.dmr_stereo = 0;
              opts.dmr_mono = 0;
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.setmod_bw = 12000;
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
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              opts.dmr_mono = 0;
              // opts.setmod_bw = 4000; //causing issues
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
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0;
              opts.dmr_mono = 0;
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
                  state.samplesPerSymbol = 10; 
                  state.symbolCenter = 4;
                  opts.mod_c4fm = 1;
                  opts.mod_qpsk = 0;
                  opts.mod_gfsk = 0;
                  state.rf_mod = 0;
                  opts.dmr_stereo = 1;
                  state.dmr_stereo = 0;
                  opts.dmr_mono = 0;
                  opts.setmod_bw = 12000;
                  sprintf (opts.output_name, "P25P2");
                  fprintf (stderr,"Decoding P25-P2 frames C4FM or OP25 Symbol Captures!\n");
                  }
              else if (optarg[0] == 's')
              {
                opts.frame_dstar = 0;
                opts.frame_x2tdma = 0;
                opts.frame_p25p1 = 0;
                opts.frame_p25p2 = 0;
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
                state.rf_mod = 0;
                opts.dmr_stereo = 1;
                opts.dmr_mono = 0;
                opts.setmod_bw = 7000;
                opts.pulse_digi_rate_out = 24000;
                opts.pulse_digi_out_channels = 2;
                sprintf (opts.output_name, "DMR Stereo");
                fprintf (stderr,"Decoding DMR Stereo BS/MS Simplex\n");
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
                      opts.dmr_mono = 0;
                      opts.setmod_bw = 12000; //safe default on both DMR and P25
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
              opts.frame_ysf = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_stereo = 0;
              opts.dmr_mono = 0;
              state.dmr_stereo = 0;
              // opts.setmod_bw = 7000; //causing issues
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
              opts.frame_ysf = 0;
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0; //
              state.rf_mod = 0;  //
              opts.pulse_digi_rate_out = 48000;
              opts.pulse_digi_out_channels = 1;
              opts.dmr_mono = 1;
              opts.dmr_stereo = 0;
              state.dmr_stereo = 0; //0
              opts.setmod_bw = 7000;
              sprintf (opts.output_name, "DMR Mono");
              fprintf(stderr, "Notice: DMR cannot autodetect polarity. \n Use -xr option if Inverted Signal expected.\n");
              fprintf (stderr,"Decoding only DMR Mono. \nUsing DMR Stereo or XDMA is highly encouraged.\n");
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
            opts.pulse_digi_rate_out = 48000;
            opts.pulse_digi_out_channels = 1;
            opts.dmr_stereo = 0;
            opts.dmr_mono = 0;
            state.dmr_stereo = 0;
            sprintf (opts.output_name, "dPMR");
            fprintf(stderr, "Notice: dPMR cannot autodetect polarity. \n Use -xd option if Inverted Signal expected.\n");
            fprintf(stderr, "Decoding only dPMR frames.\n");
          }
          break;
        //don't mess with the modulations unless you really need to
        case 'm':
          if (optarg[0] == 'a')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 1;
              state.rf_mod = 0;
              fprintf (stderr,"Don't use the -ma switch.\n");
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
              opts.setmod_bw = 12000;
              fprintf (stderr,"Enabling only QPSK modulation optimizations.\n");
            }
          else if (optarg[0] == '2')
            {
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 0;
              state.rf_mod = 1;
              state.samplesPerSymbol = 8; 
              state.symbolCenter = 3; 
              opts.setmod_bw = 12000;
              fprintf (stderr,"Enabling 6000 sps P25p2 CQPSK.\n");
            }
          //test
          else if (optarg[0] == '3')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              state.samplesPerSymbol = 10; 
              state.symbolCenter = 4; 
              opts.setmod_bw = 12000;
              fprintf (stderr,"Enabling 6000 sps P25p2 C4FM.\n");
            }
          else if (optarg[0] == '4')
          {
            opts.mod_c4fm = 1;
            opts.mod_qpsk = 1;
            opts.mod_gfsk = 1;
            state.rf_mod = 0;
            state.samplesPerSymbol = 8; 
            state.symbolCenter = 3; 
            opts.setmod_bw = 12000;
            fprintf (stderr,"Enabling 6000 sps P25p2 all optimizations.\n");
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
          opts.pulse_digi_rate_out = 48000;
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

    if((strncmp(opts.audio_in_dev, "tcp", 3) == 0)) //tcp socket input from SDR++ and others
    {
      fprintf (stderr, "TCP Direct Link: ");
      char * curr; 

      curr = strtok(opts.audio_in_dev, ":"); //should be 'tcp'
      if (curr != NULL) ; //continue
      else goto TCPEND; //end early with preset values

      curr = strtok(NULL, ":"); //host address
      if (curr != NULL)
      {
        strncpy (opts.tcp_hostname, curr, 1023);
        //shim to tie the hostname of the tcp input to the rigctl hostname (probably covers a vast majority of use cases)
        //in the future, I will rework part of this so that users can enter a hostname and port similar to how tcp and rtl strings work
        memcpy (opts.rigctlhostname, opts.tcp_hostname, sizeof (opts.rigctlhostname) );
      } 

      curr = strtok(NULL, ":"); //host port
      if (curr != NULL) opts.tcp_portno = atoi (curr);

      TCPEND:
      fprintf (stderr, "%s:", opts.tcp_hostname);
      fprintf (stderr, "%d \n", opts.tcp_portno);
      opts.tcp_sockfd = Connect(opts.tcp_hostname, opts.tcp_portno);
      if (opts.tcp_sockfd != 0)
      {
        opts.audio_in_type = 8;
        state.audio_smoothing = 0; //disable smoothing to prevent random crackling/buzzing
      }
      else 
      {
        opts.audio_in_type = 0;
        fprintf (stderr, "TCP Connection Failure - Using Pulse Audio Input.\n");
        sprintf (opts.audio_in_dev, "%s", "pulse");
      }
      
    }

    if (opts.use_rigctl == 1)
    {
      // opts.rigctl_sockfd = Connect(opts.rigctlhostname, opts.rigctlportno);
      opts.rigctl_sockfd = Connect(opts.rigctlhostname, opts.rigctlportno);
      if (opts.rigctl_sockfd != 0) opts.use_rigctl = 1;
      else
      {
        fprintf (stderr, "RIGCTL Connection Failure - RIGCTL Features Disabled\n");
        opts.use_rigctl = 0;
      } 
    }

    if((strncmp(opts.audio_in_dev, "rtl", 3) == 0)) //rtl dongle input
    {
      uint8_t rtl_ok = 0;
      #ifdef USE_RTLSDR
      fprintf (stderr, "RTL Input: ");
      char * curr; 

      curr = strtok(opts.audio_in_dev, ":"); //should be 'rtl'
      if (curr != NULL) ; //continue
      else goto RTLEND; //end early with preset values

      curr = strtok(NULL, ":"); //rtl device number "-D"
      if (curr != NULL) opts.rtl_dev_index = atoi (curr);
      else goto RTLEND;

      curr = strtok(NULL, ":"); //rtl freq "-c"
      if (curr != NULL) opts.rtlsdr_center_freq = (uint32_t)atofs(curr);
      else goto RTLEND;

      curr = strtok(NULL, ":"); //rtl gain value "-G"
      if (curr != NULL) opts.rtl_gain_value = atoi (curr);
      else goto RTLEND;

      curr = strtok(NULL, ":"); //rtl ppm err "-P"
      if (curr != NULL) opts.rtlsdr_ppm_error = atoi (curr);
      else goto RTLEND;

      curr = strtok(NULL, ":"); //rtl bandwidth "-Y"
      if (curr != NULL)
      {
        int bw = 0;
        bw = atoi (curr);
        //check for proper values (6,8,12,24)
        if (bw == 6 || bw == 8 || bw == 12 || bw == 24)
        {
          opts.rtl_bandwidth = bw;
        }
        else 
          opts.rtl_bandwidth = 12; //safe default
      }
      else goto RTLEND; 

      curr = strtok(NULL, ":"); //rtl squelch level "-L"
      if (curr != NULL) opts.rtl_squelch_level = atoi (curr);
      else goto RTLEND;

      curr = strtok(NULL, ":"); //rtl udp port "-U"
      if (curr != NULL) opts.rtl_udp_port = atoi (curr);
      else goto RTLEND;

      RTLEND:
      fprintf (stderr, "Dev %d ", opts.rtl_dev_index);
      fprintf (stderr, "Freq %d ", opts.rtlsdr_center_freq);
      fprintf (stderr, "Gain %d ", opts.rtl_gain_value);
      fprintf (stderr, "PPM %d ", opts.rtlsdr_ppm_error);
      fprintf (stderr, "BW %d ", opts.rtl_bandwidth);
      fprintf (stderr, "SQ %d ", opts.rtl_squelch_level);
      fprintf (stderr, "UDP %d \n", opts.rtl_udp_port);
      opts.audio_in_type = 3;
      state.audio_smoothing = 0; //disable smoothing to prevent random crackling/buzzing
      rtl_ok = 1;
      #endif

      if (rtl_ok == 0) //not set, means rtl support isn't compiled/available
      {
        fprintf (stderr, "RTL Support not enabled/compiled, falling back to Pulse Audio Input.\n");
        sprintf (opts.audio_in_dev, "%s", "pulse");
        opts.audio_in_type = 0;
      }
    }

    //still need to work out why I can't use this
    //issues with samples received, may be using UDP DGRAMS incorrectly, incorrect procedure?
    if((strncmp(opts.audio_in_dev, "udp", 3) == 0)) //udp socket input from SDR++, GQRX, and others
    {
      //also still needs err handling
      opts.udp_sockfd = UDPBind(opts.udp_hostname, opts.udp_portno);
      opts.audio_in_type = 6;
    }

    if((strncmp(opts.audio_in_dev, "/dev/dsp", 8) == 0))
    {
      sprintf (opts.audio_in_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_in_type = 0;
    }

    if((strncmp(opts.audio_in_dev, "/dev/audio", 10) == 0))
    {
      sprintf (opts.audio_in_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_in_type = 0;
    }

    if((strncmp(opts.audio_out_dev, "/dev/dsp", 8) == 0))
    {
      sprintf (opts.audio_out_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_out_type = 0;
    }

    if((strncmp(opts.audio_out_dev, "/dev/audio", 10) == 0))
    {
      sprintf (opts.audio_out_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_out_type = 0;
    }

    if((strncmp(opts.audio_out_dev, "pa", 2) == 0))
    {
      sprintf (opts.audio_out_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_out_type = 0;
    }

    if((strncmp(opts.audio_in_dev, "pa", 2) == 0))
    {
      sprintf (opts.audio_in_dev, "%s", "pulse");
      fprintf (stderr, "OSS and PA Handling Disabled; Using Pulse Audio.\n");
      opts.audio_out_type = 0;
    }

    if((strncmp(opts.audio_in_dev, "pulse", 5) == 0))
    {
      opts.audio_in_type = 0;
    }

    if((strncmp(opts.audio_out_dev, "pulse", 5) == 0))
    {
      opts.audio_out_type = 0;
    }

    if((strncmp(opts.audio_out_dev, "null", 4) == 0))
    {
      opts.audio_out_type = 9; //9 for NULL, or mute output
      opts.audio_out = 0; //turn off so we won't playSynthesized
    }

    if (opts.playfiles == 1)
    {
      opts.split = 1;
      opts.playoffset = 0;
      opts.playoffsetR = 0;
      opts.delay = 0;
      if (strlen(opts.wav_out_file) > 0 && opts.dmr_stereo_wav == 0)
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
      if (strlen(opts.wav_out_file) > 0 && opts.dmr_stereo_wav == 0)
        {
          openWavOutFile (&opts, &state);
        }
      else
        {
          openAudioOutDevice (&opts, SAMPLE_RATE_OUT);
        }
      openAudioInDevice (&opts);


      fprintf (stderr,"Press CTRL + C to close.\n");
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
      opts.pulse_digi_rate_out = 48000;
      opts.pulse_digi_out_channels = 1;
      openPulseOutput(&opts); //need to open it up for output
      state.aout_gain = 25; //BUGFIX: No audio output when playing back .amb/.imb files
      playMbeFiles (&opts, &state, argc, argv);
    }

  else
    {
        liveScanner (&opts, &state);
    }

  cleanupAndExit (&opts, &state);

  return (0);
}
