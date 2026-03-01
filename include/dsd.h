#ifndef DSD_H
#define DSD_H
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

//ANSII Color Characters in Terminal -- Disable by using cmake -DCOLORSLOGS=OFF ..
#ifdef PRETTY_COLORS_LOGS
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#else
#define KNRM  ""
#define KRED  ""
#define KGRN  ""
#define KYEL  ""
#define KBLU  ""
#define KMAG  ""
#define KCYN  ""
#define KWHT  ""
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>
#include <mbelib.h>
#include <sndfile.h>

#include "p25p1_heuristics.h"

//OSS support
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__CYGWIN__)
#include <sys/soundcard.h>
#endif
#include <pulse/pulseaudio.h> //PULSE AUDIO
#include <pulse/simple.h>     //PULSE AUDIO
#include <pulse/error.h>      //PULSE AUDIO
#include <pulse/introspect.h> //PULSE AUDIO

#define SAMPLE_RATE_IN 48000 //48000
#define SAMPLE_RATE_OUT 8000 //8000

#ifdef USE_RTLSDR
#include <rtl-sdr.h>
#endif

#include <locale.h>
#include <ncurses.h>

#define UNUSED(x)                       ((void)x)
#define UNUSED2(x1, x2)                 (UNUSED(x1), UNUSED(x2))
#define UNUSED3(x1, x2, x3)             (UNUSED(x1), UNUSED(x2), UNUSED(x3))
#define UNUSED4(x1, x2, x3, x4)         (UNUSED(x1), UNUSED(x2), UNUSED(x3), UNUSED(x4))
#define UNUSED5(x1, x2, x3, x4, x5)     (UNUSED(x1), UNUSED(x2), UNUSED(x3), UNUSED(x4), UNUSED(x5))

#ifdef USE_CODEC2
#include <codec2/codec2.h>
#endif

extern volatile uint8_t exitflag; //fix for issue #136

//event history (each item)
typedef struct {
  uint8_t write;      //if this event needs to be written to a log file
  uint8_t color_pair; //this value corresponds to which color pair the line should be in ncurses
  int8_t systype;     //indentifier of which decoded system type this is from (P25, DMR, etc)
  int8_t subtype;     //subtype of systpe (VLC, TLC, PDU data, System Event, etc)
  uint32_t sys_id1;   //sys_id1 through 5 will be a hierarchy of system identifiers
  uint32_t sys_id2;   //for example, trunked P25 has WACN:SYS:CC:SITE_ID:RFSS_ID
  uint32_t sys_id3;   //conventional may only use NAC, RAN, or Color Codes
  uint32_t sys_id4;   //
  uint32_t sys_id5;   //
  int8_t gi;          //group or individual
  uint8_t enc;        //clear or encrypted
  uint8_t enc_alg;    //alg if encrypted
  uint16_t enc_key;   //enc key id value, if encrypted (not key value or key variable)
  uint64_t mi;        //mi, or iv base value from OTA if provided
  uint16_t svc;       //other relevant svc opts if applicable
  uint32_t source_id; //source radio id or other source value
  uint32_t target_id; //group or individual target, or destination value
  char src_str[200];  //source, expressed as a string for M17, YSF, DSTAR, dPMR
  char tgt_str[200];  //target, expressed as a string for M17, YSF, DSTAR, dPMR
  char t_name[200];   //this is the string present from any csv groupName import 
  char s_name[200];   //same as above, but if loaded from a src value and not tg value
  char t_mode[200];   //mode, or A,B,D,DE from csv group import file
  char s_mode[200];   //mode, or A,B,D,DE from csv group import file
  uint32_t channel;   //if this occurs on a trunking channel, which channel
  time_t event_time;  //time event occurred

  uint8_t pdu[128*24];     //relevant link control, or full PDU if data call (in bytes)
  char sysid_string[200];  //string comprised of system unique identifiers
  char alias[2000];        //if this event has a source radio talker alias or similar
  char gps_s[2000];        //gps, if returned, expressed as a string
  char text_message[2000]; //if this event is a decoded text message, then it goes here
  char event_string[2000]; //user legible and printable string for the event that happened
  char internal_str[2000]; //string that relates to a DSD-FME generated event (ENC LO, error notices, etc)
} Event_History;

//event history for number of each items above
typedef struct {
  Event_History Event_History_Items[255];
} Event_History_I;

//new audio filter stuff from: https://github.com/NedSimao/FilteringLibrary
typedef struct {
    float coef[2];
    float v_out[2];
}LPFilter;

typedef struct {
    float coef;
    float v_out[2];
    float v_in[2];

}HPFilter;

typedef struct {
    LPFilter lpf;
    HPFilter hpf;
    float out_in;
}PBFilter;

typedef struct {
    float alpha;
    float beta;

    float vin[3];
    float vout[3];

}NOTCHFilter;
//end new filters

// Field list is here: http://0pointer.de/lennart/projects/pulseaudio/doxygen/structpa__sink__info.html
typedef struct pa_devicelist
{
  uint8_t initialized;
  char name[512];
  uint32_t index;
  char description[256];
} pa_devicelist_t;

//group csv import struct
typedef struct
{
  unsigned long int groupNumber;
  char groupMode[8]; //char *?
  char groupName[50];
} groupinfo;


typedef struct
{
  uint8_t  F1;
  uint8_t  F2;
  uint8_t  MessageType;

  /****************************/
  /***** VCALL parameters *****/
  /****************************/
  uint8_t  CCOption;
  uint8_t  CallType;
  uint8_t  VoiceCallOption;
  uint16_t SourceUnitID;
  uint16_t DestinationID;  /* May be a Group ID or a Unit ID */
  uint8_t  CipherType;
  uint8_t  KeyID;
  uint8_t  VCallCrcIsGood;

  /*******************************/
  /***** VCALL_IV parameters *****/
  /*******************************/
  uint8_t  IV[8];
  uint8_t  VCallIvCrcIsGood;

  /*****************************/
  /***** Custom parameters *****/
  /*****************************/

  /* Specifies if the "CipherType" and the "KeyID" parameter are valid
   * 1 = Valid ; 0 = CRC error */
  uint8_t  CipherParameterValidity;

  /* Used on DES and AES encrypted frames */
  uint8_t  PartOfCurrentEncryptedFrame;  /* Could be 1 or 2 */
  uint8_t  PartOfNextEncryptedFrame;     /* Could be 1 or 2 */
  uint8_t  CurrentIVComputed[8];
  uint8_t  NextIVComputed[8];
} NxdnElementsContent_t;

//Reed Solomon (12,9) constant
#define RS_12_9_DATASIZE        9
#define RS_12_9_CHECKSUMSIZE    3

//Reed Solomon (12,9) struct
typedef struct
{
  uint8_t data[RS_12_9_DATASIZE+RS_12_9_CHECKSUMSIZE];
} rs_12_9_codeword_t;

// Maximum degree of various polynomials.
#define RS_12_9_POLY_MAXDEG (RS_12_9_CHECKSUMSIZE*2)

typedef struct
{
  uint8_t data[RS_12_9_POLY_MAXDEG];
} rs_12_9_poly_t;

#define RS_12_9_CORRECT_ERRORS_RESULT_NO_ERRORS_FOUND           0
#define RS_12_9_CORRECT_ERRORS_RESULT_ERRORS_CORRECTED          1
#define RS_12_9_CORRECT_ERRORS_RESULT_ERRORS_CANT_BE_CORRECTED  2
typedef uint8_t rs_12_9_correct_errors_result_t;

typedef struct
{
  uint8_t bytes[3];
} rs_12_9_checksum_t;

//dPMR
/* Could only be 2 or 4 */
#define NB_OF_DPMR_VOICE_FRAME_TO_DECODE 2

typedef struct
{
  unsigned char RawVoiceBit[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4][72];
  unsigned int  errs1[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4];  /* 8 x errors #1 computed when demodulate the AMBE voice bit of the frame */
  unsigned int  errs2[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4];  /* 8 x errors #2 computed when demodulate the AMBE voice bit of the frame */
  unsigned char AmbeBit[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4][49];  /* 8 x 49 bit of AMBE voice of the frame */
  unsigned char CCHData[NB_OF_DPMR_VOICE_FRAME_TO_DECODE][48];
  unsigned int  CCHDataHammingOk[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned char CCHDataCRC[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  CCHDataCrcOk[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned char CalledID[8];
  unsigned int  CalledIDOk;
  unsigned char CallingID[8];
  unsigned int  CallingIDOk;
  unsigned int  FrameNumbering[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  CommunicationMode[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  Version[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  CommsFormat[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  EmergencyPriority[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  Reserved[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned char SlowData[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
  unsigned int  ColorCode[NB_OF_DPMR_VOICE_FRAME_TO_DECODE / 2];
} dPMRVoiceFS2Frame_t;
//
typedef struct
{
  int onesymbol;
  char mbe_in_file[1024];
  FILE *mbe_in_f;
  int errorbars;
  int datascope;
  int symboltiming;
  int verbose;
  int p25enc;
  int p25lc;
  int p25status;
  int p25tg;
  int scoperate;
  char audio_in_dev[2048]; //increase size for super long directory/file names
  int audio_in_fd;
  SNDFILE *audio_in_file;
  SF_INFO *audio_in_file_info;

  uint32_t rtlsdr_center_freq;
  int rtlsdr_ppm_error;
  int audio_in_type;
  char audio_out_dev[1024];
  int audio_out_fd;
  int audio_out_fdR; //right channel audio for OSS hack
  SNDFILE *audio_out_file;
  SF_INFO *audio_out_file_info;

  int audio_out_type; // 0 for device, 1 for file,
  int split;
  int playoffset;
  int playoffsetR;
  char mbe_out_dir[1024];
  char mbe_out_file[1024];
  char mbe_out_fileR[1024]; //second slot on a TDMA system
  char mbe_out_path[2048]; //1024
  FILE *mbe_out_f;
  FILE *mbe_out_fR; //second slot on a TDMA system
  FILE *symbol_out_f;
  time_t symbol_out_file_creation_time; //time the symbol out file was created
  uint8_t symbol_out_file_is_auto; //if the user hit the R key
  float audio_gain;
  float audio_gainR;
  float audio_gainA;
  int audio_out;
  int dmr_stereo_wav;  //per-call wav file use (rename later)
  int static_wav_file; //single static wav file for decoding duration
  char wav_out_dir[512];
  char wav_out_file[1024];
  char wav_out_fileR[1024];
  char wav_out_file_raw[1024];
  char symbol_out_file[1024];
  char lrrp_out_file[1024];
  char event_out_file[1024];
  char szNumbers[1024]; //**tera 10/32/64 char str
  short int mbe_out; //flag for mbe out, don't attempt fclose more than once
  short int mbe_outR; //flag for mbe out, don't attempt fclose more than once
  SNDFILE *wav_out_f;
  SNDFILE *wav_out_fR;
  SNDFILE *wav_out_raw;
  //int wav_out_fd;
  int serial_baud;
  char serial_dev[1024];
  int serial_fd;
  int resume;
  int frame_dstar;
  int frame_x2tdma;
  int frame_p25p1;
  int frame_p25p2;
  int inverted_p2;
  int p2counter;
  int frame_nxdn48;
  int frame_nxdn96;
  int frame_dmr;
  int frame_provoice;
  int mod_c4fm;
  int mod_qpsk;
  int mod_gfsk;
  int uvquality;
  int inverted_x2tdma;
  int inverted_dmr;
  int inverted_nxdn;
  int mod_threshold;
  int ssize;
  int msize;
  int playfiles;
  int delay;
  int use_cosine_filter;
  int unmute_encrypted_p25;
  int rtl_dev_index;
  int rtl_gain_value;
  int rtl_gain_actual;
  int rtl_squelch_level;
  int rtl_volume_multiplier;
  int rtl_udp_port;
  int rtl_bandwidth;
  int rtl_started;
  long int rtl_rms;
  int monitor_input_audio;
  int analog_only;
  int pulse_raw_rate_in;
  int pulse_raw_rate_out;
  int pulse_digi_rate_in;
  int pulse_digi_rate_out;
  int pulse_raw_in_channels;
  int pulse_raw_out_channels;
  int pulse_digi_in_channels;
  int pulse_digi_out_channels;
  int pulse_flush;
  pa_simple *pulse_raw_dev_in;
  pa_simple *pulse_raw_dev_out;
  pa_simple *pulse_digi_dev_in;
  pa_simple *pulse_digi_dev_out;
  pa_simple *pulse_digi_dev_outR;
  char pa_input_idx[100];
  char pa_output_idx[100];
  uint8_t use_ncurses_terminal;
  uint8_t ncurses_compact;
  uint8_t ncurses_history;
  int reset_state;
  int payload;
  char output_name[1024];

  unsigned int dPMR_curr_frame_is_encrypted;
  int dPMR_next_part_of_superframe;
  int inverted_dpmr;
  int frame_dpmr;

  short int dmr_mono;
  short int dmr_stereo;
  short int lrrp_file_output;

  short int dmr_mute_encL;
  short int dmr_mute_encR;

  int frame_ysf;
  int inverted_ysf;
  short int aggressive_framesync;

  int frame_m17;

  FILE *symbolfile;
  int call_alert;

  //rigctl opt
  int rigctl_sockfd;
  int use_rigctl;
  int rigctlportno;
  char rigctlhostname[1024];

  //UDP Socket Blaster Audio
  int udp_sockfd;   //digital
  int udp_sockfdA; //analog 48k1
  int udp_portno;
  char udp_hostname[1024];

  //tcp socket for SDR++, etc
  int tcp_sockfd;
  int tcp_portno;
  char tcp_hostname[1024];
  SNDFILE *tcp_file_in;

  //wav file sample rate, interpolator and decimator
  int wav_sample_rate;
  int wav_interpolator;
  int wav_decimator;

  int p25_trunk; //experimental P25 trunking with RIGCTL (or RTLFM)
  int p25_is_tuned; //set to 1 if currently on VC, set back to 0 if on CC
  float trunk_hangtime; //hangtime in seconds before tuning back to CC

  int scanner_mode; //experimental -- use the channel map as a conventional scanner, quicker tuning, but no CC

  //csv import filenames
  char group_in_file[1024];
  char lcn_in_file[1024];
  char chan_in_file[1024];
  char key_in_file[1024];
  //end import filenames

  //reverse mute
  uint8_t reverse_mute;

  //setmod bandwidth
  int setmod_bw;

  //DMR Location Area - DMRLA B***S***
  uint8_t dmr_dmrla_is_set; //flag to tell us dmrla is set by the user
  uint8_t dmr_dmrla_n; //n value for dmrla

  //DMR Late Entry
  uint8_t dmr_le; //user option to turn on or turn off late entry for enc identifiers

  //Trunking - Use Group List as Allow List
  uint8_t trunk_use_allow_list;

  //Trunking - Tune Group Calls
  uint8_t trunk_tune_group_calls;

  //Trunking - Tune Private Calls
  uint8_t trunk_tune_private_calls;

  //Trunking - Tune Data Calls
  uint8_t trunk_tune_data_calls;

  //Trunking - Tune Enc Calls (P25 only on applicable grants with svc opts)
  uint8_t trunk_tune_enc_calls;

  //OSS audio 48k/1 - slot preference
  int slot_preference;

  //hard set slots to synthesize
  int slot1_on;
  int slot2_on;

  //enable filter options
  int use_lpf;
  int use_hpf;
  int use_pbf;
  int use_hpf_d;

  //'DSP' Format Output
  uint8_t use_dsp_output;
  char dsp_out_file[2048];

  //Use P25p1 heuristics
  uint8_t use_heuristics;

  //Use floating point audio output
  int floating_point;

} dsd_opts;

typedef struct
{
  int *dibit_buf;
  int *dibit_buf_p;
  int *dmr_payload_buf;
  int *dmr_payload_p;
  int repeat;
  short *audio_out_buf;
  short *audio_out_buf_p;
  short *audio_out_bufR;
  short *audio_out_buf_pR;
  float *audio_out_float_buf;
  float *audio_out_float_buf_p;
  float *audio_out_float_bufR;
  float *audio_out_float_buf_pR;
  float audio_out_temp_buf[160];
  float *audio_out_temp_buf_p;
  float audio_out_temp_bufR[160];
  float *audio_out_temp_buf_pR;
  //analog/raw signal audio buffers
  short analog_out[960];
  int analog_sample_counter;
  //new stereo float sample storage
  float f_l[160]; //single sample left
  float f_r[160]; //single sample right
  float f_l4[4][160]; //quad sample for up to a P25p2 4V
  float f_r4[4][160]; //quad sample for up to a P25p2 4V
  //new stereo short sample storage
  short s_l[160]; //single sample left
  short s_r[160]; //single sample right
  short s_l4[18][160]; //quad sample for up to a P25p2 4V
  short s_r4[18][160]; //quad sample for up to a P25p2 4V
  //new stereo short sample storage tapped from 48_k internal upsampling
  short s_lu[160*6]; //single sample left
  short s_ru[160*6]; //single sample right
  short s_l4u[4][160*6]; //quad sample for up to a P25p2 4V
  short s_r4u[4][160*6]; //quad sample for up to a P25p2 4V
  //end
  int audio_out_idx;
  int audio_out_idx2;
  int audio_out_idxR;
  int audio_out_idx2R;
  int center;
  int jitter;
  int synctype;
  int min;
  int max;
  int lmid;
  int umid;
  int minref;
  int maxref;
  int lastsample;
  int sbuf[128];
  int sidx;
  int maxbuf[1024];
  int minbuf[1024];
  int midx;
  char err_str[64];
  char err_buf[64];
  char err_strR[64];
  char err_bufR[64];
  char fsubtype[16];
  char ftype[16];
  int symbolcnt;
  int symbolc;

  int rf_mod;
  int numflips;
  int lastsynctype;
  int lastp25type;
  int offset;
  int carrier;
  char tg[25][16];
  int tgcount;
  int lasttg;
  int lasttgR;
  int lastsrc;
  int lastsrcR;
  int8_t gi[2]; //group, or private call, per slot
  uint8_t eh_index;
  uint8_t eh_slot;
  int nac;
  int errs;
  int errs2;
  int errsR;
  int errs2R;
  int mbe_file_type;
  int optind;
  int numtdulc;
  int firstframe;
  char slot0light[8];
  float aout_gain;
  float aout_gainR;
  float aout_gainA;
  float aout_max_buf[200];
  float aout_max_bufR[200];
  float *aout_max_buf_p;
  float *aout_max_buf_pR;
  int aout_max_buf_idx;
  int aout_max_buf_idxR;
  int samplesPerSymbol;
  int symbolCenter;
  char algid[9];
  char keyid[17];
  int currentslot;
  int hardslot;
  mbe_parms *cur_mp;
  mbe_parms *prev_mp;
  mbe_parms *prev_mp_enhanced;
  mbe_parms *cur_mp2;
  mbe_parms *prev_mp2;
  mbe_parms *prev_mp_enhanced2;
  int p25kid;
  int payload_algid;
  int payload_algidR;
  int payload_keyid;
  int payload_keyidR;
  int payload_mfid;
  int payload_mfidR;
  unsigned long long int payload_mi;
  unsigned long long int payload_miR;
  unsigned long long int payload_miN;
  unsigned long long int payload_miP;
  int p25vc;
  unsigned long long int K;
  unsigned long long int K1;
  unsigned long long int K2;
  unsigned long long int K3;
  unsigned long long int K4;
  uint8_t forced_alg_id;
  int menuopen;

  //AES Key Segments
  unsigned long long int A1[2];
  unsigned long long int A2[2];
  unsigned long long int A3[2];
  unsigned long long int A4[2];
  int aes_key_loaded[2];

  //xl specific, we need to know if the ESS is from HDU, or from LDU2
  int xl_is_hdu;

  unsigned int debug_audio_errors;
  unsigned int debug_audio_errorsR;
  unsigned int debug_header_errors;
  unsigned int debug_header_critical_errors;
  int debug_mode; //debug misc things

  // Last dibit read
  int last_dibit;

  // Heuristics state data for +P25 signals
  P25Heuristics p25_heuristics;

  // Heuristics state data for -P25 signals
  P25Heuristics inv_p25_heuristics;

  //input sample buffer for monitoring Input
  short input_sample_buffer; //HERE HERE
  short pulse_raw_out_buffer; //HERE HERE

  unsigned int dmr_color_code;
  unsigned int dmr_t3_syscode;
  unsigned int nxdn_last_ran;
  unsigned int nxdn_last_rid;
  unsigned int nxdn_last_tg;
  unsigned int nxdn_cipher_type;
  unsigned int nxdn_key;
  char nxdn_call_type[1024];

  unsigned long long int dmr_lrrp_source[2];
  unsigned long long int dmr_lrrp_target[2];

  NxdnElementsContent_t NxdnElementsContent;

 char ambe_ciphered[49];
 char ambe_deciphered[49];

  unsigned int color_code;
  unsigned int color_code_ok;
  unsigned int PI;
  unsigned int PI_ok;
  unsigned int LCSS;
  unsigned int LCSS_ok;

  unsigned int dmr_fid;
  unsigned int dmr_so;
  unsigned int dmr_flco;

  unsigned int dmr_fidR;
  unsigned int dmr_soR;
  unsigned int dmr_flcoR;

  char slot1light[8];
  char slot2light[8];
  int directmode;

  int dmr_stereo_payload[144];    //load up 144 dibit buffer for every single DMR TDMA frame
  int data_header_blocks[2];      //collect number of blocks to follow from data header per slot
  int data_block_counter[2];      //counter for number of data blocks collected
  uint8_t data_header_valid[2];   //flag for verifying the data header if still valid (in case of tact/burst fec errs)
  uint8_t data_header_padding[2]; //collect number of padding octets in last block per slot
  uint8_t data_header_format[2];  //collect format of data header (conf or unconf) per slot
  uint8_t data_header_sap[2];     //collect sap info per slot
  uint8_t data_p_head[2];         //flag for dmr proprietary header to follow

  //new stuff below here
  uint8_t data_conf_data[2];            //flag for confirmed data blocks per slot
  uint8_t data_block_poc[2];            //padding octets in the header (needed for Data PDU Decryption)
  uint16_t data_byte_ctr[2];            //number of bytes acculumated
  uint8_t data_ks_start[2];             //where the start of the keystream should be applied to PDU data
  uint8_t dmr_pdu_sf[2][24*128];        //unified pdu 'superframe' //[slot][byte] -- increased capacity to 127(+1) full rate blocks
  uint8_t cap_plus_csbk_bits[2][12*8*8]; //CSBK Cap+ FL initial and appended block bit storage, by slot
  uint8_t cap_plus_block_num[2];         //received block number storage -- per timeslot
  uint8_t data_block_crc_valid[2][127]; //flag each individual block as good crc on confirmed data
  char dmr_embedded_signalling[2][7][48]; //embedded signalling 2 slots by 6 vc by 48 bits (replacing TS1SuperFrame.TimeSlotRawVoiceFrame.Sync structure)

  char dmr_cach_fragment[4][17]; //unsure of size, will need to check/verify
  int dmr_cach_counter; //counter for dmr_cach_fragments 0-3; not sure if needed yet.

  //dmr talker alias new/fixed stuff
  uint8_t dmr_alias_format[2]; //per slot
  uint8_t dmr_alias_block_len[2]; //per slot
  uint8_t dmr_alias_char_size[2]; //per slot
  char dmr_alias_block_segment[2][4][7][16]; //2 slots, by 4 blocks, by up to 7 alias bytes that are up to 16-bit chars
  char dmr_embedded_gps[2][600]; //2 slots by 99 char string for string embedded gps
  char dmr_lrrp_gps[2][600]; //2 slots by 99 char string for string lrrp gps
  char dmr_site_parms[200]; //string for site/net info depending on type of DMR system (TIII or Con+)
  char call_string[2][200]; //string for call information
  char active_channel[31][200]; //string for storing and displaying active trunking channels

  //Generic Talker Alias String
  char generic_talker_alias[2][500];

  dPMRVoiceFS2Frame_t dPMRVoiceFS2Frame;

  //event history itemized per slot
  Event_History_I * event_history_s;

  //new audio filter structs
  LPFilter RCFilter;
  HPFilter HRCFilter;
  PBFilter PBF;
  NOTCHFilter NF;
  LPFilter RCFilterL;
  HPFilter HRCFilterL;
  LPFilter RCFilterR;
  HPFilter HRCFilterR;

  char dpmr_caller_id[20];
  char dpmr_target_id[20];

  int dpmr_color_code;

  short int dmr_stereo; //need state variable for upsample function
  short int dmr_ms_mode;
  unsigned int dmrburstL;
  unsigned int dmrburstR;
  int dropL;
  int dropR;
  unsigned long long int R;
  unsigned long long int RR;
  unsigned long long int H;
  unsigned long long int HYTL;
  unsigned long long int HYTR;
  int DMRvcL;
  int DMRvcR;

  //keystream octet and bit arrays
  uint8_t ks_octetL[129*18]; //arbitary size, but large enough for the largest packed PDUs
  uint8_t ks_octetR[129*18]; //arbitary size, but large enough for the largest packed PDUs
  uint8_t ks_bitstreamL[128*18*8]; //arbitary size, but large enough for the largest PDUs
  uint8_t ks_bitstreamR[129*18*8]; //arbitary size, but large enough for the largest PDUs
  int octet_counter;
  long int bit_counterL;
  long int bit_counterR;

  //AES Specific Variables
  uint8_t aes_key[32]; //was 64 for some reason
  uint8_t aes_iv[16];
  uint8_t aes_ivR[16];

  //NXDN DES and AES, signal new VCALL_IV and new IV
  uint8_t nxdn_new_iv; //1 when a new IV comes in, else 0

  short int dmr_encL;
  short int dmr_encR;

  //P2 variables
  unsigned long long int p2_wacn;
  unsigned long long int p2_sysid;
  unsigned long long int p2_cc;    //p1 NAC
  unsigned long long int p2_siteid;
  unsigned long long int p2_rfssid;
  int p2_hardset; //flag for checking whether or not P2 wacn and sysid are hard set by user
  int p2_scramble_offset; //offset counter for scrambling application
  int p2_vch_chan_num; //vch channel number (0 or 1, not the 0-11 TS)
  int ess_b[2][96]; //external storage for ESS_B fragments
  int fourv_counter[2]; //external reference counter for ESS_B fragment collection
  int voice_counter[2]; //external reference counter for 18V x 2 P25p2 Superframe
  int p2_is_lcch; //flag to tell us when a frame is lcch and not sacch

  //iden freq storage for frequency calculations
  int p25_chan_tdma[16]; //set from iden_up vs iden_up_tdma
  int p25_chan_iden;
  int p25_chan_type[16];
  int p25_trans_off[16];
  int p25_chan_spac[16];
  long int p25_base_freq[16];

  //p25 frequency storage for trunking and display in ncurses
  long int p25_cc_freq;     //cc freq from net_stat
  long int p25_vc_freq[2]; //vc freq from voice grant updates, etc
  int p25_cc_is_tdma; //flag to tell us that the P25 control channel is TDMA so we can change symbol rate when required


  //experimental symbol file capture read throttle
  int symbol_throttle; //throttle speed
  int use_throttle; //only use throttle if set to 1

  //dmr trunking stuff
  int dmr_rest_channel;
  int dmr_mfid; //just when 'fid' is used as a manufacturer ID and not a feature set id
  int dmr_vc_lcn;
  int dmr_vc_lsn;
  int dmr_tuned_lcn;
  uint16_t dmr_cc_lpcn; //dmr t3 logical physical channel number
  uint32_t tg_hold; //single TG to hold on when enabled

  //edacs
  int ea_mode;

  unsigned short esk_mask;
  unsigned long long int edacs_site_id;
  uint32_t edacs_sys_id;
  uint32_t edacs_area_code;
  int edacs_lcn_count; //running tally of lcn's observed on edacs system
  int edacs_cc_lcn; //current lcn for the edacs control channel
  int edacs_vc_lcn; //current lcn for any active vc (not the one we are tuned/tuning to)
  int edacs_tuned_lcn; //the vc we are currently tuned to...above variable is for updating all in the matrix
  int edacs_vc_call_type; //the type of call on the given VC - see defines below
  int edacs_a_bits;  //  Agency Significant Bits
  int edacs_f_bits;  //   Fleet Significant Bits
  int edacs_s_bits;  //Subfleet Significant Bits
  int edacs_a_shift; //Calculated Shift for A Bits
  int edacs_f_shift; //Calculated Shift for F Bits
  int edacs_a_mask;  //Calculated Mask for A Bits
  int edacs_f_mask;  //Calculated Mask for F Bits
  int edacs_s_mask;  //Calculated Mask for S Bits

  //flags for EDACS call type
  #define EDACS_IS_VOICE         0x01
  #define EDACS_IS_DIGITAL       0x02
  #define EDACS_IS_EMERGENCY     0x04
  #define EDACS_IS_GROUP         0x08
  #define EDACS_IS_INDIVIDUAL    0x10
  #define EDACS_IS_ALL_CALL      0x20
  #define EDACS_IS_INTERCONNECT  0x40
  #define EDACS_IS_TEST_CALL     0x80
  #define EDACS_IS_AGENCY_CALL  0x100
  #define EDACS_IS_FLEET_CALL   0x200

  //trunking group and lcn freq list
  long int trunk_lcn_freq[26]; //max number on an EDACS system, should be enough on DMR too hopefully
  long int trunk_chan_map[0xFFFF]; //NXDN - 10 bit; P25 - 16 bit; DMR up to 12 bit (standard TIII)
  groupinfo group_array[0x3FF]; //max supported by Cygwin is 3FFF, I hope nobody actually tries to import this many groups
  unsigned int group_tally; //tally number of groups imported from CSV file for referencing later
  int lcn_freq_count;
  int lcn_freq_roll; //number we have 'rolled' to in search of the CC
  time_t last_cc_sync_time; //use this to start hunting for CC after signal lost
  time_t last_vc_sync_time; //flag for voice activity bursts, tune back on con+ after more than x seconds no voice
  time_t last_active_time; //time the a 'call grant' was received, used to clear the active_channel strings after x seconds
  time_t last_t3_tune_time; //last time a DMR T3 grant was received, this is used to prevent a rogue p_clear condition from immediately sending back to CC
  int is_con_plus; //con_plus flag for knowing its safe to skip payload channel after x seconds of no voice sync

  //new nxdn stuff
  int nxdn_part_of_frame;
  int nxdn_ran;
  int nxdn_sf;
  bool nxdn_sacch_non_superframe; //flag to indicate whether or not a sacch is a part of a superframe, or an individual piece
  uint8_t nxdn_sacch_frame_segment[4][18]; //part of frame by 18 bits
  uint8_t nxdn_sacch_frame_segcrc[4];
  uint8_t nxdn_alias_block_number;
  char nxdn_alias_block_segment[4][4][8];
  uint16_t nxdn_pn95_seed;

  //site/srv/cch info
  char nxdn_location_category[14];
  uint32_t nxdn_location_sys_code;
  uint16_t nxdn_location_site_code;

  //channel access information
  uint8_t nxdn_rcn;
  uint8_t nxdn_base_freq;
  uint8_t nxdn_step;
  uint8_t nxdn_bw;

  //multi-key array
  unsigned long long int rkey_array[0x1FFFF];
  int keyloader; //let us know the keyloader is active

  //dmr late entry mi
  uint64_t late_entry_mi_fragment[2][8][3];

  //dmr manufacturer branding and sub_branding (i.e., Motorola and Con+)
  char dmr_branding[20];
  char dmr_branding_sub[80];

  //Remus DMR End Call Alert Beep
  int dmr_end_alert[2]; //dmr TLC end call alert beep has already played once flag

  //Bitmap Filtering Options
  int audio_smoothing;

  //YSF Fusion Call Strings and Info
  uint8_t ysf_dt; //data type -- VD1, VD2, Full Rate, etc.
  uint8_t ysf_fi; //frame information -- HC, CC, TC
  uint8_t ysf_cm; //group or private call
  char ysf_tgt[11];
  char ysf_src[11];
  char ysf_upl[11];
  char ysf_dnl[11];
  char ysf_rm1[6];
  char ysf_rm2[6];
  char ysf_rm3[6];
  char ysf_rm4[6];
  char ysf_txt[21][21]; //text storage blocks

  //DSTAR Call Strings and Info
  char dstar_rpt1[9];
  char dstar_rpt2[9];
  char dstar_dst[9];
  char dstar_src[13];
  char dstar_txt[60];
  char dstar_gps[60];

  //M17 Storage
  uint8_t m17_lsf[360];
  uint8_t m17_pkt[850];
  uint8_t m17_str_dt; //stream contents

  unsigned long long int m17_dst;
  unsigned long long int m17_src;
  uint8_t m17_can; //can value that was decoded from signal

  char m17_dst_csd[20];
  char m17_src_csd[20];

  char m17_src_str[50];
  char m17_dst_str[50];

  uint8_t m17_meta[16];    //packed meta
  uint8_t m17_aes_iv[16]; //aes iv
  uint8_t m17_enc;        //enc type
  uint8_t m17_enc_st;    //scrambler or data subtye

  char m17_text_string[1024];
  char m17_gnss_string[1024];
  char m17_data_string[1024];
  char m17_meta_string[1024];

  float m17_viterbi_err;

  //misc str storage
  char str50a[50];
  char str50b[50];
  char str50c[50];

  //Codec2
  #ifdef USE_CODEC2
  struct CODEC2 *codec2_3200; //M17 fullrate
  struct CODEC2 *codec2_1600; //M17 halfrate
  #endif

  //tyt_ap=1 active  
  int tyt_ap;
  int tyt_bp;
  int tyt_ep;
  // retrevis rc2
  int retevis_ap;
  void *rc2_context;

  //kenwood scrambler on DMR with forced application
  int ken_sc;

  //anytone bp
  int any_bp;

  //baofeng ap
  int baofeng_ap;

  //connect systems ee
  int csi_ee;

  //generic ks
  int straight_ks;
  int straight_mod;

  uint8_t static_ks_bits[2][882];
  int static_ks_counter[2];

} dsd_state;

/*
 * Frame sync patterns
 */

//M17 Sync Patterns
#define M17_LSF     "11113313"
#define M17_STR     "33331131"

#define FUSION_SYNC     "31111311313113131131"
#define INV_FUSION_SYNC "13333133131331313313"

#define INV_P25P1_SYNC "333331331133111131311111"
#define P25P1_SYNC     "111113113311333313133333"

#define P25P2_SYNC     "11131131111333133333"
#define INV_P25P2_SYNC "33313313333111311111"

#define X2TDMA_BS_VOICE_SYNC "113131333331313331113311"
#define X2TDMA_BS_DATA_SYNC  "331313111113131113331133"
#define X2TDMA_MS_DATA_SYNC  "313113333111111133333313"
#define X2TDMA_MS_VOICE_SYNC "131331111333333311111131"

#define DSTAR_HD       "131313131333133113131111"
#define INV_DSTAR_HD   "313131313111311331313333"
#define DSTAR_SYNC     "313131313133131113313111"
#define INV_DSTAR_SYNC "131313131311313331131333"

#define NXDN_MS_DATA_SYNC      "313133113131111333"
#define INV_NXDN_MS_DATA_SYNC  "131311331313333111"
#define INV_NXDN_BS_DATA_SYNC  "131311331313333131"
#define NXDN_BS_DATA_SYNC      "313133113131111313"
#define NXDN_MS_VOICE_SYNC     "313133113131113133"
#define INV_NXDN_MS_VOICE_SYNC "131311331313331311"
#define INV_NXDN_BS_VOICE_SYNC "131311331313331331"
#define NXDN_BS_VOICE_SYNC     "313133113131113113"

#define DMR_BS_DATA_SYNC  "313333111331131131331131"
#define DMR_BS_VOICE_SYNC "131111333113313313113313"
#define DMR_MS_DATA_SYNC  "311131133313133331131113"
#define DMR_MS_VOICE_SYNC "133313311131311113313331"

//Part 1-A CAI 4.4.4 (FSW only - Late Entry - Marginal Signal)
#define NXDN_FSW      "3131331131"
#define INV_NXDN_FSW  "1313113313"
//Part 1-A CAI 4.4.3 Preamble Last 9 plus FSW (start of RDCH)
#define NXDN_PANDFSW      "3131133313131331131" //19 symbols
#define INV_NXDN_PANDFSW  "1313311131313113313" //19 symbols

#define DMR_RESERVED_SYNC  "131331111133133133311313"

#define DMR_DIRECT_MODE_TS1_DATA_SYNC  "331333313111313133311111"
#define DMR_DIRECT_MODE_TS1_VOICE_SYNC "113111131333131311133333"
#define DMR_DIRECT_MODE_TS2_DATA_SYNC  "311311111333113333133311"
#define DMR_DIRECT_MODE_TS2_VOICE_SYNC "133133333111331111311133"

#define INV_PROVOICE_SYNC    "31313111333133133311331133113311"
#define PROVOICE_SYNC        "13131333111311311133113311331133"
#define INV_PROVOICE_EA_SYNC "13313133113113333311313133133311"
#define PROVOICE_EA_SYNC     "31131311331331111133131311311133"

//EDACS/PV EOT dotting sequence
#define DOTTING_SEQUENCE_A  "131313131313131313131313131313131313131313131313" //0xAAAA...
#define DOTTING_SEQUENCE_B  "313131313131313131313131313131313131313131313131" //0x5555...

//define the provoice conventional string pattern to default 85/85 if not enabled, else mute it so we won't double sync on accident in frame_sync
#ifdef PVCONVENTIONAL
#define PROVOICE_CONV        "00000000000000000000000000000000" //all zeroes should be unobtainable string in the frame_sync synctests
#define INV_PROVOICE_CONV    "00000000000000000000000000000000" //all zeroes should be unobtainable string in the frame_sync synctests
#else
#define PROVOICE_CONV        "13131333111311311313131313131313" //TX 85 RX 85 (default programming value)
#define INV_PROVOICE_CONV    "31313111333133133131313131313131" //TX 85 RX 85 (default programming value)
#endif
//we use the short sync instead of the default 85/85 when PVCONVENTIONAL is defined by cmake
#define PROVOICE_CONV_SHORT                 "1313133311131131" //16-bit short pattern, last 16-bits change based on TX an RX values
#define INV_PROVOICE_CONV_SHORT             "3131311133313313"
//In this pattern (inverted polarity, the norm for PV) 3 is bit 0, and 1 is bit 1 (2 level GFSK)
                  //same pattern   //TX     //RX
// Sync Pattern = 3131311133313313 31331131 31331131 TX/RX 77  -- 31331131 symbol = 01001101 binary = 77 decimal
// Sync Pattern = 3131311133313313 33333333 33333333 TX/RX 0   -- 33333333 symbol = 00000000 binary = 0 decimal
// Sync Pattern = 3131311133313313 33333331 33333331 TX/RX 1   -- 33333331 symbol = 00000001 binary = 1 decimal
// Sync Pattern = 3131311133313313 13131133 13131133 TX/RX 172 -- 13131133 symbol = 10101100 binary = 172 decimal
// Sync Pattern = 3131311133313313 11333111 11333111 TX/RX 199 -- 11333111 symbol = 11000111 binary = 199 decimal
// Sync Pattern = 3131311133313313 31313131 31313131 TX/RX 85  -- 31313131 symbol = 01010101 binary = 85 decimal

#define EDACS_SYNC      "313131313131313131313111333133133131313131313131"
#define INV_EDACS_SYNC  "131313131313131313131333111311311313131313131313"

#define DPMR_FRAME_SYNC_1     "111333331133131131111313"
#define DPMR_FRAME_SYNC_2     "113333131331"
#define DPMR_FRAME_SYNC_3     "133131333311"
#define DPMR_FRAME_SYNC_4     "333111113311313313333131"

/* dPMR Frame Sync 1 to 4 - Inverted */
#define INV_DPMR_FRAME_SYNC_1 "333111113311313313333131"
#define INV_DPMR_FRAME_SYNC_2 "331111313113"
#define INV_DPMR_FRAME_SYNC_3 "311313111133"
#define INV_DPMR_FRAME_SYNC_4 "111333331133131131111313"

/*
 * function prototypes
 */

void processdPMRvoice (dsd_opts * opts, dsd_state * state);
void processAudio (dsd_opts * opts, dsd_state * state);
void processAudioR (dsd_opts * opts, dsd_state * state);
void openPulseInput (dsd_opts * opts);
void openPulseOutput (dsd_opts * opts);
void openOSSOutput (dsd_opts * opts);
void closePulseInput (dsd_opts * opts);
void closePulseOutput (dsd_opts * opts);
void writeSynthesizedVoice (dsd_opts * opts, dsd_state * state);
void writeSynthesizedVoiceR (dsd_opts * opts, dsd_state * state);
void writeSynthesizedVoiceMS (dsd_opts * opts, dsd_state * state); //short mono to stereo wav file
void playSynthesizedVoice (dsd_opts * opts, dsd_state * state);   //short mono output slot 1
void playSynthesizedVoiceR (dsd_opts * opts, dsd_state * state);  //short mono output slot 2
//new float stuff
void playSynthesizedVoiceFS (dsd_opts * opts, dsd_state * state);  //float stereo mix
void playSynthesizedVoiceFS3 (dsd_opts * opts, dsd_state * state); //float stereo mix 3v2 DMR
void playSynthesizedVoiceFS4 (dsd_opts * opts, dsd_state * state); //float stereo mix 4v2 P25p2
void playSynthesizedVoiceFM (dsd_opts * opts, dsd_state * state);  //float mono
void agf (dsd_opts * opts, dsd_state * state, float samp[160], int slot); //float gain control
void agsm (dsd_opts * opts, dsd_state * state, short * input, int len); //short gain control for analog things
void analog_gain (dsd_opts * opts, dsd_state * state, short * input, int len); //manual gain handling for analong things
//new short stuff
void playSynthesizedVoiceMS (dsd_opts * opts, dsd_state * state);    //short mono mix
void playSynthesizedVoiceMSR (dsd_opts * opts, dsd_state * state);   //short mono mix R (needed for OSS 48k input/output)
void playSynthesizedVoiceSS (dsd_opts * opts, dsd_state * state);   //short stereo mix
void playSynthesizedVoiceSS3 (dsd_opts * opts, dsd_state * state);  //short stereo mix 3v2 DMR
void playSynthesizedVoiceSS4 (dsd_opts * opts, dsd_state * state);  //short stereo mix 4v2 P25p2
void playSynthesizedVoiceSS18 (dsd_opts * opts, dsd_state * state); //short stereo mix 18V Superframe
void upsampleS (short invalue, short prev, short outbuf[6]); //upsample 8k to 48k short
//
void openAudioOutDevice (dsd_opts * opts, int speed);
void openAudioInDevice (dsd_opts * opts);

//pulse sources and sinks
void parse_pulse_input_string  (dsd_opts * opts, char * input);
void parse_pulse_output_string (dsd_opts * opts, char * input);
void pa_state_cb (pa_context *c, void *userdata);
void pa_sinklist_cb (pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb (pa_context *c, const pa_source_info *l, int eol, void *userdata);
int pa_get_devicelist (pa_devicelist_t *input, pa_devicelist_t *output);
int pulse_list();

//parse a user string into a uint8_t array
uint16_t parse_raw_user_string (char * input, uint8_t * output);

int getDibit (dsd_opts * opts, dsd_state * state);
int get_dibit_and_analog_signal (dsd_opts * opts, dsd_state * state, int * out_analog_signal);
int digitize (dsd_opts* opts, dsd_state* state, int symbol);

void skipDibit (dsd_opts * opts, dsd_state * state, int count);
void saveImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d);
void saveAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d);
void saveAmbe2450DataR (dsd_opts * opts, dsd_state * state, char *ambe_d); //tdma slot 2
void PrintAMBEData (dsd_opts * opts, dsd_state * state, char *ambe_d);
void PrintIMBEData (dsd_opts * opts, dsd_state * state, char *imbe_d);
int readImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d);
int readAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d);
void keyring(dsd_opts * opts, dsd_state * state);
void read_sdrtrunk_json_format (dsd_opts * opts, dsd_state * state);
void ambe2_codeword_print_f (dsd_opts * opts, char ambe_fr[4][24]);
void ambe2_codeword_print_b (dsd_opts * opts, char ambe_fr[4][24]);
void ambe2_codeword_print_i (dsd_opts * opts, char ambe_fr[4][24]);
void openMbeInFile (dsd_opts * opts, dsd_state * state);
void closeMbeOutFile (dsd_opts * opts, dsd_state * state);
void closeMbeOutFileR (dsd_opts * opts, dsd_state * state); //tdma slot 2
void openMbeOutFile (dsd_opts * opts, dsd_state * state);
void openMbeOutFileR (dsd_opts * opts, dsd_state * state); //tdma slot 2
void openWavOutFile (dsd_opts * opts, dsd_state * state);
void openWavOutFileL (dsd_opts * opts, dsd_state * state);
void openWavOutFileR (dsd_opts * opts, dsd_state * state);
void openWavOutFileLR (dsd_opts * opts, dsd_state * state); //stereo wav file for tdma decoded speech
void openWavOutFileRaw (dsd_opts * opts, dsd_state * state);
SNDFILE * open_wav_file (char * dir, char * temp_filename, uint16_t sample_rate, uint8_t ext);
SNDFILE * close_wav_file(SNDFILE * wav_file);
SNDFILE * close_and_rename_wav_file(SNDFILE * wav_file, char * wav_out_filename, char * dir, Event_History_I * event_struct);
SNDFILE * close_and_delete_wav_file(SNDFILE * wav_file, char * wav_out_filename);
void openSymbolOutFile (dsd_opts * opts, dsd_state * state);
void closeSymbolOutFile (dsd_opts * opts, dsd_state * state);
void rotate_symbol_out_file (dsd_opts * opts, dsd_state * state);
void writeRawSample (dsd_opts * opts, dsd_state * state, short sample);
void closeWavOutFile (dsd_opts * opts, dsd_state * state);
void closeWavOutFileL (dsd_opts * opts, dsd_state * state);
void closeWavOutFileR (dsd_opts * opts, dsd_state * state);
void closeWavOutFileRaw (dsd_opts * opts, dsd_state * state);
void printFrameInfo (dsd_opts * opts, dsd_state * state);
void processFrame (dsd_opts * opts, dsd_state * state);
void printFrameSync (dsd_opts * opts, dsd_state * state, char *frametype, int offset, char *modulation);
int getFrameSync (dsd_opts * opts, dsd_state * state);
int comp (const void *a, const void *b);
void noCarrier (dsd_opts * opts, dsd_state * state);
void initOpts (dsd_opts * opts);
void initState (dsd_state * state);
void usage ();
void liveScanner (dsd_opts * opts, dsd_state * state);
void cleanupAndExit (dsd_opts * opts, dsd_state * state);
int main (int argc, char **argv);
void playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv);
void processMbeFrame (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24]);
void openSerial (dsd_opts * opts, dsd_state * state);
void resumeScan (dsd_opts * opts, dsd_state * state);
int getSymbol (dsd_opts * opts, dsd_state * state, int have_sync);
void upsample (dsd_state * state, float invalue);
void processDSTAR (dsd_opts * opts, dsd_state * state);

//new cleaner, sleaker, nicer mbe handler...maybe -- wrap around ifdef later on with cmake options
void soft_mbe (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24]);
void soft_tonef (float samp[160], int n, int ID, int AD);

//new p25lcw
void p25_lcw (dsd_opts * opts, dsd_state * state, uint8_t LCW_bits[], uint8_t irrecoverable_errors);
//new p25 1/2 rate decoder
int p25_12(uint8_t * input, uint8_t treturn[12]);
//new p25 lsd fec function
int p25p1_lsd_fec(uint8_t * input);

void processP25lcw (dsd_opts * opts, dsd_state * state, char *lcformat, char *mfid, char *lcinfo);
void processHDU (dsd_opts * opts, dsd_state * state);
void processLDU1 (dsd_opts * opts, dsd_state * state);
void processLDU2 (dsd_opts * opts, dsd_state * state);
void processTDU (dsd_opts * opts, dsd_state * state);
void processTDULC (dsd_opts * opts, dsd_state * state);
void processProVoice (dsd_opts * opts, dsd_state * state);
void processX2TDMAdata (dsd_opts * opts, dsd_state * state);
void processX2TDMAvoice (dsd_opts * opts, dsd_state * state);
void processDSTAR_HD (dsd_opts * opts, dsd_state * state); //DSTAR Header
void processDSTAR_SD (dsd_opts * opts, dsd_state * state, uint8_t * sd); //DSTAR Slow Data
void processYSF(dsd_opts * opts, dsd_state * state); //YSF
void processM17STR(dsd_opts * opts, dsd_state * state); //M17 (STR)
void processP2(dsd_opts * opts, dsd_state * state); //P2
void processTSBK(dsd_opts * opts, dsd_state * state); //P25 Trunking Single Block
void processMPDU(dsd_opts * opts, dsd_state * state); //P25 Multi Block PDU (SAP 0x61 FMT 0x15 or 0x17 for Trunking Blocks)
short dmr_filter(short sample);
short nxdn_filter(short sample);
short dpmr_filter(short sample);
short m17_filter(short sample);

//utility functions
uint64_t ConvertBitIntoBytes(uint8_t * BufferIn, uint32_t BitLength);
uint64_t convert_bits_into_output (uint8_t * input, int len);
void pack_bit_array_into_byte_array (uint8_t * input, uint8_t * output, int len);
void pack_bit_array_into_byte_array_asym (uint8_t * input, uint8_t * output, int len);
void unpack_byte_array_into_bit_array (uint8_t * input, uint8_t * output, int len);

//ambe pack and unpack functions
void pack_ambe (char * input, uint8_t * output, int len);
void unpack_ambe (uint8_t * input, char * ambe);

void ncursesOpen (dsd_opts * opts, dsd_state * state);
void ncursesPrinter (dsd_opts * opts, dsd_state * state);
void ncursesMenu (dsd_opts * opts, dsd_state * state);
uint8_t ncurses_input_handler(dsd_opts * opts, dsd_state * state, int c);
void ncursesClose ();

//NXDN Functions Start Here
void nxdn_frame (dsd_opts * opts, dsd_state * state);
void nxdn_pn95_dibit_scrambler(dsd_state * state, uint8_t * dibits, int len);
void nxdn_message_type (dsd_opts * opts, dsd_state * state, uint8_t MessageType);
void nxdn_voice (dsd_opts * opts, dsd_state * state, int voice, uint8_t * dbuf);

//All-in-One NXDN Soft Decision Viterbi Function based on libM17
uint32_t nxdn_soft_decision_viterbi(uint8_t * bits, const uint16_t * interleave, uint8_t * puncture, int d_len, int p_len, int num_bytes, int offset, uint8_t * viterbi_bits, uint8_t * viterbi_bytes);
//NXDN Conventional and Type-C
void nxdn_facch1(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t frame);
void nxdn_sacch(dsd_opts * opts, dsd_state * state, uint8_t * bits);
void nxdn_cac(dsd_opts * opts, dsd_state * state, uint8_t * bits);
void nxdn_facch2_udch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t type);
//type-d 'idas'
void idas_scch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t direction);
void idas_facch3_udch2(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t type);
//Japanese DCR
void dcr_sacch(dsd_opts * opts, dsd_state * state, uint8_t * bits);
void dcr_pich_tch(dsd_opts * opts, dsd_state * state, uint8_t * bits, uint8_t lich);

//OP25 NXDN CRC functions
int load_i(const uint8_t val[], int len);
uint8_t crc6(const uint8_t buf[], int len);
uint16_t crc12f(const uint8_t buf[], int len);
uint16_t crc15(const uint8_t buf[], int len);
uint16_t crc16cac(const uint8_t buf[], int len);
uint8_t crc7_scch(uint8_t bits[], int len); //converted from op25 crc6

//YSF Soft Decision Viterbi
uint32_t ysf_soft_decision_viterbi(uint8_t * dbuf, int d_len, int num_bytes, int offset, uint8_t * viterbi_bits, uint8_t * viterbi_bytes);

//libM17 viterbi decoder

//libm17 magic soft decision based viterbi
#define SYM_PER_PLD 184
void slice_symbols(uint16_t out[2*SYM_PER_PLD], const float inp[SYM_PER_PLD]);
void randomize_soft_bits(uint16_t inp[SYM_PER_PLD*2]);
void reorder_soft_bits(uint16_t outp[SYM_PER_PLD*2], const uint16_t inp[SYM_PER_PLD*2]);
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, const uint16_t len);
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, const uint16_t in_len, const uint16_t p_len);
void viterbi_decode_bit(uint16_t s0, uint16_t s1, const size_t pos);
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len);
void viterbi_reset(void);
uint16_t q_abs_diff(const uint16_t v1, const uint16_t v2);

//libm17 based soft decision viterbi for variable len and new punctures
void slice_symbols_to_len(uint16_t * out, const float * inp, int len);

//keeping these
void NXDN_SACCH_Full_decode(dsd_opts * opts, dsd_state * state);
void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state,
                                  uint8_t CrcCorrect, uint8_t * ElementsContent);
void NXDN_decode_VCALL(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_VCALL_IV(dsd_opts * opts, dsd_state * state, uint8_t * Message);
char * NXDN_Call_Type_To_Str(uint8_t CallType);
void NXDN_Voice_Call_Option_To_Str(uint8_t VoiceCallOption, uint8_t * Duplex, uint8_t * TransmissionMode);
char * NXDN_Cipher_Type_To_Str(uint8_t CipherType);
//added these
void NXDN_decode_Prop(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_Alias(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_Alias16(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_VCALL_ASSGN(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_cch_info(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_srv_info(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_site_info(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void nxdn_decode_dst_info(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_adj_site(dsd_opts * opts, dsd_state * state, uint8_t * Message);
//Type-D SCCH Message Decoder
void NXDN_decode_scch(dsd_opts * opts, dsd_state * state, uint8_t * Message, uint8_t direction);
void NXDN_decode_VCALL_ARIB(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_ALIAS_ARIB(dsd_opts * opts, dsd_state * state, uint8_t * Message);
uint32_t sjis_char_to_unicode(uint16_t sjis);
uint32_t big5_char_to_unicode(uint16_t big5);
int utf8_encode(int c, char *buf);

void dPMRVoiceFrameProcess(dsd_opts * opts, dsd_state * state);

//dPMR functions
void ScrambledPMRBit(uint32_t * LfsrValue, uint8_t * BufferIn, uint8_t * BufferOut, uint32_t NbOfBitToScramble);
void DeInterleave6x12DPmrBit(uint8_t * BufferIn, uint8_t * BufferOut);
uint8_t CRC7BitdPMR(uint8_t * BufferIn, uint32_t BitLength);
uint8_t CRC8BitdPMR(uint8_t * BufferIn, uint32_t BitLength);
void ConvertAirInterfaceID(uint32_t AI_ID, uint8_t ID[8]);
int32_t GetdPmrColorCode(uint8_t ChannelCodeBit[24]);

//BPTC (Block Product Turbo Code) functions
void BPTCDeInterleaveDMRData(uint8_t * Input, uint8_t * Output);
uint32_t BPTC_196x96_Extract_Data(uint8_t InputDeInteleavedData[196], uint8_t DMRDataExtracted[96], uint8_t R[3]);
uint32_t BPTC_128x77_Extract_Data(uint8_t InputDataMatrix[8][16], uint8_t DMRDataExtracted[77]);
uint32_t BPTC_16x2_Extract_Data(uint8_t InputInterleavedData[32], uint8_t DMRDataExtracted[32], uint32_t ParityCheckTypeOdd);

//Reed Solomon (12,9) functions
void rs_12_9_calc_syndrome(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome);
uint8_t rs_12_9_check_syndrome(rs_12_9_poly_t *syndrome);
rs_12_9_correct_errors_result_t rs_12_9_correct_errors(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome, uint8_t *errors_found);
rs_12_9_checksum_t *rs_12_9_calc_checksum(rs_12_9_codeword_t *codeword);

//DMR CRC Functions
uint16_t ComputeCrcCCITT(uint8_t * DMRData);
uint16_t ComputeCrcCCITT16d(const uint8_t * buf, uint32_t len);
uint32_t ComputeAndCorrectFullLinkControlCrc(uint8_t * FullLinkControlDataBytes, uint32_t * CRCComputed, uint32_t CRCMask);
uint8_t ComputeCrc5Bit(uint8_t * DMRData);
uint16_t ComputeCrc9Bit(uint8_t * DMRData, uint32_t NbData);
uint32_t ComputeCrc32Bit(uint8_t * DMRData, uint32_t NbData);

//new simplified dmr functions
void dmr_data_burst_handler(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t databurst);
void dmr_data_sync (dsd_opts * opts, dsd_state * state);
void dmr_pi (dsd_opts * opts, dsd_state * state, uint8_t PI_BYTE[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors);
void dmr_flco (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[], uint32_t CRCCorrect, uint32_t * IrrecoverableErrors, uint8_t type);
void dmr_cspdu (dsd_opts * opts, dsd_state * state, uint8_t cs_pdu_bits[], uint8_t cs_pdu[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors);
void dmr_slco (dsd_opts * opts, dsd_state * state, uint8_t slco_bits[]);
uint8_t dmr_cach (dsd_opts * opts, dsd_state * state, uint8_t cach_bits[25]);
uint32_t viterbi_r34 (uint8_t * input, uint8_t * output);
void beeper (dsd_opts * opts, dsd_state * state, int lr, int id, int ad, int len);
void dmr_gateway_identifier (uint32_t source, uint32_t target); //translate special addresses

//Embedded Alias and GPS reports
void dmr_talker_alias_lc_header (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void dmr_talker_alias_lc_blocks (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t block_num, uint8_t * lc_bits);
void dmr_talker_alias_lc_decode (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t block_num, uint8_t char_size, uint16_t end);
void apx_embedded_alias_test_phase1 (dsd_opts * opts, dsd_state * state);
void apx_embedded_alias_header_phase1 (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void apx_embedded_alias_header_phase2 (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void apx_embedded_alias_blocks_phase1 (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void apx_embedded_alias_blocks_phase2 (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void apx_embedded_alias_decode (dsd_opts * opts, dsd_state * state, uint8_t slot, int16_t num_bits, uint8_t * input);
void apx_embedded_alias_dump (dsd_opts * opts, dsd_state * state, uint8_t slot, uint16_t num_bytes, uint8_t * input, uint8_t * decoded);
void l3h_embedded_alias_blocks_phase1 (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t * lc_bits);
void l3h_embedded_alias_decode (dsd_opts * opts, dsd_state * state, uint8_t slot, int16_t len, uint8_t * input);
void tait_iso7_embedded_alias_decode (dsd_opts * opts, dsd_state * state, uint8_t slot, int16_t len, uint8_t * input);
void dmr_embedded_gps (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[]);
void apx_embedded_gps (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[]);
void lip_protocol_decoder (dsd_opts * opts, dsd_state * state, uint8_t * input);
void nmea_iec_61162_1 (dsd_opts * opts, dsd_state * state, uint8_t * input, uint32_t src, int type);
void harris_lptt (dsd_opts * opts, dsd_state * state, uint8_t * input, uint32_t src, int slot, int phase);
void utf16_to_text (dsd_state * state, uint8_t wr, uint16_t len, uint8_t * input);
void utf8_to_text (dsd_state * state, uint8_t wr, uint16_t len, uint8_t * input);

//"DMR STEREO"
void dmrBSBootstrap (dsd_opts * opts, dsd_state * state);
void dmrBS (dsd_opts * opts, dsd_state * state);
void dmrMS (dsd_opts * opts, dsd_state * state);
void dmrMSData (dsd_opts * opts, dsd_state * state);
void dmrMSBootstrap (dsd_opts * opts, dsd_state * state);

//dmr data header and multi block types (header, 1/2, 3/4, 1, Unified)
void dmr_dheader (dsd_opts * opts, dsd_state * state, uint8_t dheader[], uint8_t dheader_bits[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors);
void dmr_block_assembler (dsd_opts * opts, dsd_state * state, uint8_t block_bytes[], uint8_t block_len, uint8_t databurst, uint8_t type);

//dmr pdu handling
void dmr_sd_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU);
void dmr_udp_comp_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU);
void dmr_reset_blocks (dsd_opts * opts, dsd_state * state);
void dmr_lrrp (dsd_opts * opts, dsd_state * state, uint16_t len, uint32_t source, uint32_t dest, uint8_t * DMR_PDU);
void dmr_locn (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU);

//p25 pdu handling
uint8_t p25_decrypt_pdu(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t alg_id, uint16_t key_id, unsigned long long int mi, int len);
uint8_t p25_decode_es_header(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t * sap, int * ptr, int len);
uint8_t p25_decode_es_header_2(dsd_opts * opts, dsd_state * state, uint8_t * input, int * ptr, int len);
void p25_decode_extended_address(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t * sap, int * ptr);
void p25_decode_pdu_trunking(dsd_opts * opts, dsd_state * state, uint8_t * mpdu_byte);
void p25_decode_pdu_header(dsd_opts * opts, dsd_state * state, uint8_t * input);
void p25_decode_pdu_data(dsd_opts * opts, dsd_state * state, uint8_t * input, int len);
void p25_decode_rsp(uint8_t C, uint8_t T, uint8_t S, char * rsp_string);
void p25_decode_sap(uint8_t SAP, char * sap_string);

//misc pdu
void decode_ip_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * input);
void decode_cellocator(dsd_opts * opts, dsd_state * state, uint8_t * input, int len);
void decode_ars(dsd_opts * opts, dsd_state * state, uint8_t * input, int len);

//Time and Date Functions
char * getTime();
char * getTimeC();
char * getTimeN(time_t t);
char * getTimeF(time_t t);
char * getDate();
char * getDateH();
char * getDateS();
char * getDateN(time_t t);
char * getDateF(time_t t);

//event history functions
void init_event_history (Event_History_I * event_struct, uint8_t start, uint8_t stop);
void push_event_history (Event_History_I * event_struct);
void write_event_to_log_file (dsd_opts * opts, dsd_state * state, uint8_t slot, uint8_t swrite, char * event_string);
void watchdog_event_history (dsd_opts * opts, dsd_state * state, uint8_t slot);
void watchdog_event_current (dsd_opts * opts, dsd_state * state, uint8_t slot);
void watchdog_event_datacall (dsd_opts * opts, dsd_state * state, uint32_t src, uint32_t dst, char * data_string, uint8_t slot);

//edacs AFS things
int isCustomAfsString(dsd_state * state);
int getAfsStringLength(dsd_state * state);
int getAfsString(dsd_state * state, char * buffer, int a, int f, int s);

//dmr alg stuff
void dmr_alg_reset (dsd_opts * opts, dsd_state * state);
void dmr_alg_refresh (dsd_opts * opts, dsd_state * state);
void dmr_late_entry_mi_fragment (dsd_opts * opts, dsd_state * state, uint8_t vc, uint8_t ambe_fr[4][24], uint8_t ambe_fr2[4][24], uint8_t ambe_fr3[4][24]);
void dmr_late_entry_mi (dsd_opts * opts, dsd_state * state);
void dmr_decode_syscode(dsd_opts * opts, dsd_state * state, uint8_t * cs_pdu_bits, int csbk_fid, int type);

//handle Single Burst (Voice Burst F) or Reverse Channel Signalling
void dmr_sbrc (dsd_opts * opts, dsd_state * state, uint8_t power);

//DMR FEC/CRC from Boatbod - OP25
bool Hamming17123(uint8_t* d);
uint8_t crc8(uint8_t bits[], unsigned int len);
bool crc8_ok(uint8_t bits[], unsigned int len);
//modified CRC functions for SB/RC
uint8_t crc7(uint8_t bits[], unsigned int len);
uint8_t crc3(uint8_t bits[], unsigned int len);
uint8_t crc4(uint8_t bits[], unsigned int len);

//LFSR and LFSRP code courtesy of https://github.com/mattames/LFSR/
void LFSR(dsd_state * state);
void LFSRP(dsd_state * state);

void LFSRN (char * BufferIn, char * BufferOut, dsd_state * state);
void LFSR64(dsd_state * state);

void Hamming_7_4_init();
void Hamming_7_4_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Hamming_7_4_decode(unsigned char *rxBits);

void Hamming_12_8_init();
void Hamming_12_8_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Hamming_12_8_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Hamming_13_9_init();
void Hamming_13_9_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Hamming_13_9_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Hamming_15_11_init();
void Hamming_15_11_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Hamming_15_11_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Hamming_16_11_4_init();
void Hamming_16_11_4_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Hamming_16_11_4_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Golay_20_8_init();
void Golay_20_8_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Golay_20_8_decode(unsigned char *rxBits);

void Golay_23_12_init();
void Golay_23_12_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Golay_23_12_decode(unsigned char *rxBits);

void Golay_24_12_init();
void Golay_24_12_encode(unsigned char *origBits, unsigned char *encodedBits);
bool Golay_24_12_decode(unsigned char *rxBits);

void QR_16_7_6_init();
void QR_16_7_6_encode(unsigned char *origBits, unsigned char *encodedBits);
bool QR_16_7_6_decode(unsigned char *rxBits);

void InitAllFecFunction(void);
void resetState (dsd_state * state);
void reset_dibit_buffer(dsd_state * state);
void dstar_header_decode(dsd_state * state, int radioheaderbuffer[660]);

//P25 PDU Handler
void process_MAC_VPDU(dsd_opts * opts, dsd_state * state, int type, unsigned long long int MAC[24]);

//P25 xCCH Handlers (SACCH, FACCH, LCCH)
void process_SACCH_MAC_PDU (dsd_opts * opts, dsd_state * state, int payload[180]);
void process_FACCH_MAC_PDU (dsd_opts * opts, dsd_state * state, int payload[156]);

//P25 Channel to Frequency
long int process_channel_to_freq (dsd_opts * opts, dsd_state * state, int channel);

//P25 CRC Functions
int crc16_lb_bridge (int payload[190], int len);
int crc12_xb_bridge (int payload[190], int len);

//NXDN Channel to Frequency, Courtesy of IcomIcR20 on RR Forums
long int nxdn_channel_to_frequency (dsd_opts * opts, dsd_state * state, uint16_t channel);

//rigctl functions and TCP/UDP functions
void error(char *msg);
int Connect (char *hostname, int portno);
bool Send(int sockfd, char *buf);
bool Recv(int sockfd, char *buf);

//rtl_fm udp tuning function
void rtl_udp_tune(dsd_opts * opts, dsd_state * state, long int frequency);

long int GetCurrentFreq(int sockfd);
bool SetFreq(int sockfd, long int freq);
bool SetModulation(int sockfd, int bandwidth);
//commands below unique to GQRX only, not usable on SDR++
bool GetSignalLevel(int sockfd, double *dBFS);
bool GetSquelchLevel(int sockfd, double *dBFS);
bool SetSquelchLevel(int sockfd, double dBFS);
bool GetSignalLevelEx(int sockfd, double *dBFS, int n_samp);
//end gqrx-scanner

//UDP socket connection
int UDPBind (char *hostname, int portno);

//EDACS
void edacs(dsd_opts * opts, dsd_state * state);
unsigned long long int edacs_bch (unsigned long long int message);
void eot_cc(dsd_opts * opts, dsd_state * state); //end of TX return to CC

//Generic Tuning Functions
void return_to_cc (dsd_opts * opts, dsd_state * state);

//initialize static float filter memory
void init_rrc_filter_memory();

//misc audio filtering for analog
long int raw_rms(short *samples, int len, int step);
void init_audio_filters(dsd_state * state);
void lpf(dsd_state * state, short * input, int len);
void hpf(dsd_state * state, short * input, int len);
void pbf(dsd_state * state, short * input, int len);
void nf(dsd_state * state, short * input, int len);
void hpf_dL(dsd_state * state, short * input, int len);
void hpf_dR(dsd_state * state, short * input, int len);
//from: https://github.com/NedSimao/FilteringLibrary
void LPFilter_Init(LPFilter *filter, float cutoffFreqHz, float sampleTimeS);
float LPFilter_Update(LPFilter *filter, float v_in);
void HPFilter_Init(HPFilter *filter, float cutoffFreqHz, float sampleTimeS);
float HPFilter_Update(HPFilter *filter, float v_in);
void PBFilter_Init(PBFilter *filter, float HPF_cutoffFreqHz, float LPF_cutoffFreqHz, float sampleTimeS);
float PBFilter_Update(PBFilter *filter, float v_in);
void NOTCHFilter_Init(NOTCHFilter *filter, float centerFreqHz, float notchWidthHz, float sampleTimeS);
float NOTCHFilter_Update(NOTCHFilter *filter, float vin);

//csv imports
int csvGroupImport(dsd_opts * opts, dsd_state * state);
int csvLCNImport(dsd_opts * opts, dsd_state * state);
int csvChanImport(dsd_opts * opts, dsd_state * state);
int csvKeyImportDec(dsd_opts * opts, dsd_state * state);
int csvKeyImportHex(dsd_opts * opts, dsd_state * state);

//UDP Socket Connect and UDP Socket Blaster (audio output)
int udp_socket_connect(dsd_opts * opts, dsd_state * state);
int udp_socket_connectA(dsd_opts * opts, dsd_state * state);
void udp_socket_blaster(dsd_opts * opts, dsd_state * state, size_t nsam, void * data);
void udp_socket_blasterA(dsd_opts * opts, dsd_state * state, size_t nsam, void * data);

//RC4 function prototypes
void rc4_voice_decrypt (int drop, uint8_t keylength, uint8_t messagelength, uint8_t key[], uint8_t cipher[], uint8_t plain[]);
void rc4_block_output (int drop, int keylen, int meslen, uint8_t * key, uint8_t * output_blocks);

//DES function prototypes
void des_multi_keystream_output (unsigned long long int mi, unsigned long long int key_ulli, uint8_t * output, int type, int len);
void tdea_multi_keystream_output (unsigned long long int mi, uint8_t * key, uint8_t * output, int type, int len);

//AES function prototypes
void aes_ofb_keystream_output (uint8_t * iv, uint8_t * key, uint8_t * output, int type, int nblocks);
void aes_ecb_bytewise_payload_crypt (uint8_t * input, uint8_t * key, uint8_t * output, int type, int de);
void aes_cbc_bytewise_payload_crypt (uint8_t * iv, uint8_t * key, uint8_t * in, uint8_t * out, int type, int nblocks, int de);
void aes_cfb_bytewise_payload_crypt (uint8_t * iv, uint8_t * key, uint8_t * in, uint8_t * out, int type, int nblocks, int de);
void aes_ctr_bytewise_payload_crypt (uint8_t * iv, uint8_t * key, uint8_t * payload, int type);
void aes_ctr_bitwise_payload_crypt (uint8_t * iv, uint8_t * key, uint8_t * payload, int type);

//Tytera / Retevis / Anytone / Kenwood / Misc DMR Encryption Modes
void tyt16_ambe2_codeword_keystream(dsd_state * state, char ambe_fr[4][24], int fnum);
void tyt_ep_aes_keystream_creation(dsd_state * state, char * input);
void tyt_ap_pc4_keystream_creation(dsd_state * state, char * input);
void retevis_rc2_keystream_creation(dsd_state *state, char *input);
void ken_dmr_scrambler_keystream_creation(dsd_state * state, char * input);
void anytone_bp_keystream_creation(dsd_state * state, char * input);
void baofeng_ap_pc5_keystream_creation(dsd_state *state, char *input);
void csi72_ambe2_codeword_keystream(dsd_state * state, char ambe_fr[4][24]);
void straight_mod_xor_keystream_creation(dsd_state * state, char * input);

//Kirisun
uint32_t kirisun_lfsr(unsigned long long int mi);
void kirisun_adv_keystream_creation(dsd_state *state);
void kirisun_uni_keystream_creation(dsd_state *state);

//Hytera Enhanced
void hytera_enhanced_rc4_setup(dsd_opts * opts, dsd_state * state, unsigned long long int key_value, unsigned long long int mi_value);
unsigned long long int hytera_lfsr(uint8_t * mi, uint8_t * taps, uint8_t len);
void hytera_enhanced_alg_refresh(dsd_state * state);

//LFSR to expand either a DMR 32-bit or P25/NXDN 64-bit MI into a 128-bit IV for AES
void lfsr_64_to_128(uint8_t * iv);
void LFSR128(dsd_state * state);
void LFSR128n(dsd_state * state);
void LFSR128d(dsd_state * state);


#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_RTLSDR
void open_rtlsdr_stream(dsd_opts *opts);
void cleanup_rtlsdr_stream();
int get_rtlsdr_sample(int16_t *sample, dsd_opts * opts, dsd_state * state);
void rtlsdr_sighandler();
void rtl_dev_tune(dsd_opts * opts, long int frequency);
long int rtl_return_rms();
void rtl_clean_queue();
#endif


//Phase 2 RS/FEC Functions
int ez_rs28_ess (int payload[96], int parity[168]); //ezpwd bridge for FME
int ez_rs28_facch (int payload[156], int parity[114]); //ezpwd bridge for FME
int ez_rs28_sacch (int payload[180], int parity[132]); //ezpwd bridge for FME
int isch_lookup (uint64_t isch); //isch map lookup

//Phase 1 NID Check
int check_NID(char* bch_code, int* new_nac, char* new_duid, unsigned char parity);

#ifdef __cplusplus
}
#endif

#endif // DSD_H
