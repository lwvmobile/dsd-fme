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

//pretty pretty colors
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#define __USE_XOPEN //compiler warning on this, needed for strptime, seems benign
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef SOLARIS
#include <sys/audioio.h>
#endif
#if defined(BSD) && !defined(__APPLE__)
#include <sys/soundcard.h>
#endif
#include <math.h>
#include <mbelib.h>
#include <sndfile.h>

#include "p25p1_heuristics.h"

#include <pulse/simple.h>     //PULSE AUDIO
#include <pulse/error.h>      //PULSE AUDIO
//#include <pulse/pulseaudio.h> //PUlSE AUDIO

#define SAMPLE_RATE_IN 48000 //48000
#define SAMPLE_RATE_OUT 8000 //8000,

#ifdef USE_PORTAUDIO
#include "portaudio.h"
#define PA_FRAMES_PER_BUFFER 64
//Buffer needs to be large enough to prevent input buffer overruns while DSD is doing other struff (like outputting voice)
//else you get skipped samples which result in incomplete/erronous decodes and a mountain of error messages.
#define PA_LATENCY_IN 0.500
//Buffer needs to be large enough to prevent output buffer underruns while DSD is doing other stuff (like decoding input)
//else you get choppy audio and in 'extreme' cases errors.
//Buffer also needs to be as small as possible so we don't have a lot of audio delay.
#define PA_LATENCY_OUT 0.100
#endif

#ifdef USE_RTLSDR
#include <rtl-sdr.h>
#endif
//look into making this not required by doing ifdef, make new c file for methods, and CLI case option for ncurses terminal
#include <locale.h>   //move this stuff to dsd.h
#include <ncurses.h> //move this stuff to dsd.h

static volatile int exitflag;

typedef struct
{
  uint8_t RFChannelType;
  uint8_t FunctionnalChannelType;
  uint8_t Option;
  uint8_t Direction;
  uint8_t CompleteLich; /* 7 bits of LICH without parity */
  uint8_t PreviousCompleteLich; /* 7 bits of previous LICH without parity */
} NxdnLich_t;


typedef struct
{
  uint8_t StructureField;
  uint8_t RAN;
  uint8_t Data[18];
  uint8_t CrcIsGood;
} NxdnSacchRawPart_t;


typedef struct
{
  uint8_t Data[80];
  uint8_t CrcIsGood;
} NxdnFacch1RawPart_t;


typedef struct
{
  uint8_t Data[184];
  uint8_t CrcIsGood;
} NxdnFacch2RawPart_t;


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


enum
{
  MODE_UNENCRYPTED,
  MODE_BASIC_PRIVACY,
  MODE_ENHANCED_PRIVACY_ARC4,
  MODE_ENHANCED_PRIVACY_AES256,
  MODE_HYTERA_BASIC_40_BIT,
  MODE_HYTERA_BASIC_128_BIT,
  MODE_HYTERA_BASIC_256_BIT
};

//Read Solomon (12,9) constant
#define RS_12_9_DATASIZE        9
#define RS_12_9_CHECKSUMSIZE    3

//Read Solomon (12,9) struct
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

typedef struct
{
  char VoiceFrame[3][72];  /* 3 x 72 bit of voice per time slot frame    */
  char Sync[48];           /* 48 bit of data or SYNC per time slot frame */
}TimeSlotRawVoiceFrame_t;


typedef struct
{
  char DeInterleavedVoiceSample[3][4][24];  /* 3 x (4 x 24) deinterleaved bit of voice per time slot frame */
}TimeSlotDeinterleavedVoiceFrame_t;


typedef struct
{
  int  errs1[3];  /* 3 x errors #1 computed when demodulate the AMBE voice bit of the frame */
  int  errs2[3];  /* 3 x errors #2 computed when demodulate the AMBE voice bit of the frame */
  char AmbeBit[3][49];  /* 3 x 49 bit of AMBE voice of the frame */
}TimeSlotAmbeVoiceFrame_t;

typedef struct
{
  unsigned int  ProtectFlag;
  unsigned int  Reserved;
  unsigned int  FullLinkControlOpcode;
  unsigned int  FeatureSetID;
  unsigned int  ServiceOptions;
  unsigned int  GroupAddress;      /* Destination ID or TG (in Air Interface format) */
  unsigned int  SourceAddress;     /* Source ID (in Air Interface format) */
  unsigned int  DataValidity;      /* 0 = All Full LC data are incorrect ; 1 = Full LC data are correct (CRC checked OK) */
  unsigned int  LeftOvers;         //what is the 8 bits for that isn't used
}FullLinkControlPDU_t;

typedef struct
{
  TimeSlotRawVoiceFrame_t TimeSlotRawVoiceFrame[6]; /* A voice superframe contains 6 timeslot voice frame */
  TimeSlotDeinterleavedVoiceFrame_t TimeSlotDeinterleavedVoiceFrame[6];
  TimeSlotAmbeVoiceFrame_t TimeSlotAmbeVoiceFrame[6];
  FullLinkControlPDU_t FullLC;
} TimeSlotVoiceSuperFrame_t;


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
  char audio_in_dev[1024];
  int audio_in_fd;
  SNDFILE *audio_in_file;
  SF_INFO *audio_in_file_info;
#ifdef USE_PORTAUDIO
  PaStream* audio_in_pa_stream;
#endif
  uint32_t rtlsdr_center_freq;
  int rtlsdr_ppm_error; //was int, changed to float
  int audio_in_type; // 0 for device, 1 for file, 2 for portaudio, 3 for rtlsdr
  char audio_out_dev[1024];
  int audio_out_fd;
  SNDFILE *audio_out_file;
  SF_INFO *audio_out_file_info;
#ifdef USE_PORTAUDIO
  PaStream* audio_out_pa_stream;
#endif
  int audio_out_type; // 0 for device, 1 for file, 2 for portaudio
  int split;
  int playoffset;
  int playoffsetR;
  char mbe_out_dir[1024];
  char mbe_out_file[1024];
  char mbe_out_path[1024];
  FILE *mbe_out_f;
  float audio_gain;
  float audio_gainR;
  int audio_out;
  char wav_out_file[1024];
  SNDFILE *wav_out_f;
  //int wav_out_fd;
  int serial_baud;
  char serial_dev[1024];
  int serial_fd;
  int resume;
  int frame_dstar;
  int frame_x2tdma;
  int frame_p25p1;
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
  int mod_threshold;
  int ssize;
  int msize;
  int playfiles;
  int delay;
  int use_cosine_filter;
  int unmute_encrypted_p25;
  int rtl_dev_index;
  int rtl_gain_value;
  int rtl_squelch_level;
  int rtl_volume_multiplier;
  int rtl_udp_port;
  int rtl_bandwidth;
  int monitor_input_audio;
  int pulse_raw_rate_in;
  int pulse_raw_rate_out;
  int pulse_digi_rate_in;
  int pulse_digi_rate_out;
  //int pulse_digi_rate_outR;
  int pulse_raw_in_channels;
  int pulse_raw_out_channels;
  int pulse_digi_in_channels;
  int pulse_digi_out_channels;
  //int pulse_digi_out_channelsR;
  int pulse_flush;
  pa_simple *pulse_raw_dev_in;
  pa_simple *pulse_raw_dev_out;
  pa_simple *pulse_digi_dev_in;
  pa_simple *pulse_digi_dev_out;
  pa_simple *pulse_digi_dev_outR;
  int use_ncurses_terminal;
  int reset_state;
  int payload;
  char output_name[1024];

  int EncryptionMode;

  unsigned int dPMR_curr_frame_is_encrypted;
  int dPMR_next_part_of_superframe;
  int inverted_dpmr;
  int frame_dpmr;
  //
  short int dmr_stereo;

  //
  int frame_ysf;
  int inverted_ysf; //not sure if ysf comes in inverted or not, but signal could if IQ flipped
  short int aggressive_framesync; //set to 1 for more aggressive framesync, 0 for less aggressive

} dsd_opts;

typedef struct
{
  int *dibit_buf;
  int *dibit_buf_p;
  int *dmr_payload_buf; //try loading up a small amount to buffer, 144 or 288?
  int *dmr_payload_p; //position for payload buffer
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
  int audio_out_idx;
  int audio_out_idx2;
  int audio_out_idxR;
  int audio_out_idx2R;
  //int wav_out_bytes;
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
  char err_str[64]; //actual error errorbars
  char err_buf[64]; //make copy of err_str for comparison, only have string print when strcmp is different??
  char err_strR[64];
  char err_bufR[64];
  char fsubtype[16];
  char ftype[16];
  int symbolcnt;
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
  //char slot1light[8];
  float aout_gain;
  float aout_gainR;
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
  int payload_mi;
  int payload_miR; //check to see if anything tied to this still functions appropriately
  uint64_t payload_miP;
  int payload_lsfr;
  int payload_lsfrR;
  unsigned long long int K;
  int M;

  unsigned int debug_audio_errors;
  unsigned int debug_audio_errorsR;
  unsigned int debug_header_errors;
  unsigned int debug_header_critical_errors;

  // Last dibit read
  int last_dibit;

  // Heuristics state data for +P5 signals
  P25Heuristics p25_heuristics;

  // Heuristics state data for -P5 signals
  P25Heuristics inv_p25_heuristics;

  //input sample buffer for monitoring Input
  short input_sample_buffer; //HERE HERE
  short pulse_raw_out_buffer; //HERE HERE

  unsigned int dmr_color_code;
  unsigned int nxdn_last_ran;
  unsigned int nxdn_last_rid;
  unsigned int nxdn_last_tg;
  unsigned int nxdn_cipher_type;
  unsigned int nxdn_key;
  char nxdn_call_type[1024];
  char dmr_callsign[2][6][99]; //plenty of room in case of overflow;
  char dmr_lrrp[2][6][9999];

  NxdnSacchRawPart_t NxdnSacchRawPart[4];
  NxdnFacch1RawPart_t NxdnFacch1RawPart[2];
  NxdnFacch2RawPart_t NxdnFacch2RawPart;
  NxdnElementsContent_t NxdnElementsContent;
  NxdnLich_t NxdnLich;

  int printNXDNAmbeVoiceSampleHex;

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

  unsigned int dmr_fidR;
  unsigned int dmr_soR;

  char slot1light[8];
  char slot2light[8];
  int directmode;
  TimeSlotVoiceSuperFrame_t TS1SuperFrame;
  TimeSlotVoiceSuperFrame_t TS2SuperFrame;

  char dmr_branding[25];
  uint8_t  dmr_12_rate_sf[2][60]; //going five frames deep by 12 bytes //[slot][value]
  uint8_t  dmr_34_rate_sf[2][64]; //going four frames deep by 16 bytes //[slot][value]
  int dmr_stereo_payload[144]; //load up 144 dibit buffer for every single DMR TDMA frame
  //uint8_t dmr_stereo_payload[144]; //load up 144 dibit buffer for every single DMR TDMA frame


  dPMRVoiceFS2Frame_t dPMRVoiceFS2Frame;

  //These flags are used to known the DMR frame
   //printing format (hex or binary)
  int printDMRRawVoiceFrameHex;
  int printDMRRawVoiceFrameBin;
  int printDMRRawDataFrameHex;
  int printDMRRawDataFrameBin;
  int printDMRAmbeVoiceSampleHex;
  int printDMRAmbeVoiceSampleBin;

  //These flags are used to known the dPMR frame
   //orinting format (hex or binary)
  int printdPMRRawVoiceFrameHex;
  int printdPMRRawVoiceFrameBin;
  int printdPMRRawDataFrameHex;
  int printdPMRRawDataFrameBin;
  int printdPMRAmbeVoiceSampleHex;
  int printdPMRAmbeVoiceSampleBin;

  unsigned char * dpmr_caller_id;
  unsigned char * dpmr_target_id;
  int dpmr_color_code;

  short int dmr_stereo; //need state variable for upsample function
  short int dmr_ms_rc;
  short int dmr_ms_mode;
  unsigned int dmrburstL;
  unsigned int dmrburstR;
  int dropL;
  int dropR;
  unsigned long long int R;

  //dstar header for ncurses
  unsigned char dstarradioheader[41];

#ifdef TRACE_DSD
  char debug_prefix;
  char debug_prefix_2;
  unsigned int debug_sample_index;
  unsigned int debug_sample_left_edge;
  unsigned int debug_sample_right_edge;
  FILE* debug_label_file;
  FILE* debug_label_dibit_file;
  FILE* debug_label_imbe_file;
#endif

} dsd_state;

/*
 * Frame sync patterns
 */

#define FUSION_SYNC     "31111311313113131131" //HA!
#define INV_FUSION_SYNC "13333133131331313313" //HA!

#define INV_P25P1_SYNC "333331331133111131311111"
#define P25P1_SYNC     "111113113311333313133333"

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

//inversion testing on MS
//#define DMR_MS_DATA_SYNC  "133313311131311113313331"
//#define DMR_MS_VOICE_SYNC "311131133313133331131113"

#define DMR_RC_DATA_SYNC  "131331111133133133311313"

#define DMR_DIRECT_MODE_TS1_DATA_SYNC  "331333313111313133311111"
#define DMR_DIRECT_MODE_TS1_VOICE_SYNC "113111131333131311133333"
#define DMR_DIRECT_MODE_TS2_DATA_SYNC  "311311111333113333133311"
#define DMR_DIRECT_MODE_TS2_VOICE_SYNC "133133333111331111311133"

#define INV_PROVOICE_SYNC    "31313111333133133311331133113311"
#define PROVOICE_SYNC        "13131333111311311133113311331133"
#define INV_PROVOICE_EA_SYNC "13313133113113333311313133133311"
#define PROVOICE_EA_SYNC     "31131311331331111133131311311133"

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
void processDMRdata (dsd_opts * opts, dsd_state * state);
void processDMRvoice (dsd_opts * opts, dsd_state * state);
void dmrBSBootstrap (dsd_opts * opts, dsd_state * state);
void dmrBS (dsd_opts * opts, dsd_state * state);
void dmrMS (dsd_opts * opts, dsd_state * state);
void dmrMSData (dsd_opts * opts, dsd_state * state);
void dmrMSBootstrap (dsd_opts * opts, dsd_state * state);
void processdPMRvoice (dsd_opts * opts, dsd_state * state);
void processAudio (dsd_opts * opts, dsd_state * state);
void processAudioR (dsd_opts * opts, dsd_state * state);
void openPulseInput (dsd_opts * opts);  //not sure if we need to just pass opts, or opts and state yet
void openPulseOutput (dsd_opts * opts);  //not sure if we need to just pass opts, or opts and state yet
void writeSynthesizedVoice (dsd_opts * opts, dsd_state * state);
void playSynthesizedVoice (dsd_opts * opts, dsd_state * state);
void playSynthesizedVoiceR (dsd_opts * opts, dsd_state * state);
void openAudioOutDevice (dsd_opts * opts, int speed);
void openAudioInDevice (dsd_opts * opts);

int getDibit (dsd_opts * opts, dsd_state * state);
int get_dibit_and_analog_signal (dsd_opts * opts, dsd_state * state, int * out_analog_signal);

void skipDibit (dsd_opts * opts, dsd_state * state, int count);
void saveImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d);
void saveAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d);
void PrintAMBEData (dsd_opts * opts, dsd_state * state, char *ambe_d);
void PrintIMBEData (dsd_opts * opts, dsd_state * state, char *imbe_d);
int readImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d);
int readAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d);
void openMbeInFile (dsd_opts * opts, dsd_state * state);
void closeMbeOutFile (dsd_opts * opts, dsd_state * state);
void openMbeOutFile (dsd_opts * opts, dsd_state * state);
void openWavOutFile (dsd_opts * opts, dsd_state * state);
void closeWavOutFile (dsd_opts * opts, dsd_state * state);
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
void sigfun (int sig);
int main (int argc, char **argv);
void playMbeFiles (dsd_opts * opts, dsd_state * state, int argc, char **argv);
void processMbeFrame (dsd_opts * opts, dsd_state * state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24]);
void openSerial (dsd_opts * opts, dsd_state * state);
void resumeScan (dsd_opts * opts, dsd_state * state);
int getSymbol (dsd_opts * opts, dsd_state * state, int have_sync);
void upsample (dsd_state * state, float invalue);
void processDSTAR (dsd_opts * opts, dsd_state * state);
void processNXDNVoice (dsd_opts * opts, dsd_state * state);
void processNXDNData (dsd_opts * opts, dsd_state * state);
void processP25lcw (dsd_opts * opts, dsd_state * state, char *lcformat, char *mfid, char *lcinfo);
void processHDU (dsd_opts * opts, dsd_state * state);
void processLDU1 (dsd_opts * opts, dsd_state * state);
void processLDU2 (dsd_opts * opts, dsd_state * state);
void processTDU (dsd_opts * opts, dsd_state * state);
void processTDULC (dsd_opts * opts, dsd_state * state);
void processProVoice (dsd_opts * opts, dsd_state * state);
void processX2TDMAdata (dsd_opts * opts, dsd_state * state);
void processX2TDMAvoice (dsd_opts * opts, dsd_state * state);
void processDSTAR_HD (dsd_opts * opts, dsd_state * state);
void processYSF(dsd_opts * opts, dsd_state * state); //YSF
short dmr_filter(short sample);
short nxdn_filter(short sample);


int strncmperr(const char *s1, const char *s2, size_t size, int MaxErr);
/* Global functions */ //also borrowed
uint32_t ConvertBitIntoBytes(uint8_t * BufferIn, uint32_t BitLength);
uint32_t ConvertAsciiToByte(uint8_t AsciiMsbByte, uint8_t AsciiLsbByte, uint8_t * OutputByte);
void Convert49BitSampleInto7Bytes(char * InputBit, char * OutputByte);
void Convert7BytesInto49BitSample(char * InputByte, char * OutputBit);
//
//ifdef ncurses
void ncursesOpen ();
void ncursesPrinter (dsd_opts * opts, dsd_state * state);
void ncursesClose ();
//endif ncurses

/* NXDN frame decoding functions */
void ProcessNXDNFrame(dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNxdnRCCHFrame(dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNxdnRTCHFrame(dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNxdnRDCHFrame(dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNxdnRTCH_C_Frame(dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNXDNIdleData (dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNXDNFacch1Data (dsd_opts * opts, dsd_state * state, uint8_t Inverted);
void ProcessNXDNUdchData (dsd_opts * opts, dsd_state * state, uint8_t Inverted);
//end borrow

/* NXDN functions */
void CNXDNConvolution_start(void);
void CNXDNConvolution_decode(uint8_t s0, uint8_t s1);
void CNXDNConvolution_chainback(unsigned char* out, unsigned int nBits);
void CNXDNConvolution_encode(const unsigned char* in, unsigned char* out, unsigned int nBits);
uint8_t NXDN_decode_LICH(uint8_t   InputLich[8],
                         uint8_t * RFChannelType,
                         uint8_t * FunctionnalChannelType,
                         uint8_t * Option,
                         uint8_t * Direction,
                         uint8_t * CompleteLichBinaryFormat,
                         uint8_t   Inverted);
uint8_t NXDN_SACCH_raw_part_decode(uint8_t * Input, uint8_t * Output);
void NXDN_SACCH_Full_decode(dsd_opts * opts, dsd_state * state);
uint8_t NXDN_FACCH1_decode(uint8_t * Input, uint8_t * Output);
uint8_t NXDN_UDCH_decode(uint8_t * Input, uint8_t * Output);
void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state,
                                  uint8_t CrcCorrect, uint8_t * ElementsContent);
void NXDN_decode_VCALL(dsd_opts * opts, dsd_state * state, uint8_t * Message);
void NXDN_decode_VCALL_IV(dsd_opts * opts, dsd_state * state, uint8_t * Message);
char * NXDN_Call_Type_To_Str(uint8_t CallType);
void NXDN_Voice_Call_Option_To_Str(uint8_t VoiceCallOption, uint8_t * Duplex, uint8_t * TransmissionMode);
char * NXDN_Cipher_Type_To_Str(uint8_t CipherType);
uint16_t CRC15BitNXDN(uint8_t * BufferIn, uint32_t BitLength);
uint16_t CRC12BitNXDN(uint8_t * BufferIn, uint32_t BitLength);
uint8_t CRC6BitNXDN(uint8_t * BufferIn, uint32_t BitLength);
void ScrambledNXDNVoiceBit(int * LfsrValue, char * BufferIn, char * BufferOut, int NbOfBitToScramble);


void dPMRVoiceFrameProcess(dsd_opts * opts, dsd_state * state);
void printdPMRAmbeVoiceSample(dsd_opts * opts, dsd_state * state);
void printdPMRRawVoiceFrame (dsd_opts * opts, dsd_state * state);
//dPMR functions
void ScrambledPMRBit(uint32_t * LfsrValue, uint8_t * BufferIn, uint8_t * BufferOut, uint32_t NbOfBitToScramble);
void DeInterleave6x12DPmrBit(uint8_t * BufferIn, uint8_t * BufferOut);
uint8_t CRC7BitdPMR(uint8_t * BufferIn, uint32_t BitLength);
uint8_t CRC8BitdPMR(uint8_t * BufferIn, uint32_t BitLength);
void ConvertAirInterfaceID(uint32_t AI_ID, uint8_t ID[8]);
int32_t GetdPmrColorCode(uint8_t ChannelCodeBit[24]);

void ProcessDMR (dsd_opts * opts, dsd_state * state);
void DMRDataFrameProcess(dsd_opts * opts, dsd_state * state);
void DMRVoiceFrameProcess(dsd_opts * opts, dsd_state * state);
//BPTC (Block Product Turbo Code) functions
void BPTCDeInterleaveDMRData(uint8_t * Input, uint8_t * Output);
uint32_t BPTC_196x96_Extract_Data(uint8_t InputDeInteleavedData[196], uint8_t DMRDataExtracted[96], uint8_t R[3]);
uint32_t BPTC_128x77_Extract_Data(uint8_t InputDataMatrix[8][16], uint8_t DMRDataExtracted[77]);
uint32_t BPTC_16x2_Extract_Data(uint8_t InputInterleavedData[32], uint8_t DMRDataExtracted[32]);

//Read Solomon (12,9) functions
void rs_12_9_calc_syndrome(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome);
uint8_t rs_12_9_check_syndrome(rs_12_9_poly_t *syndrome);
rs_12_9_correct_errors_result_t rs_12_9_correct_errors(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome, uint8_t *errors_found);
rs_12_9_checksum_t *rs_12_9_calc_checksum(rs_12_9_codeword_t *codeword);

//SYNC DMR data extraction functions
void ProcessDmrVoiceLcHeader(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessDmrTerminaisonLC(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessVoiceBurstSync(dsd_opts * opts, dsd_state * state);
uint16_t ComputeCrcCCITT(uint8_t * DMRData);
uint32_t ComputeAndCorrectFullLinkControlCrc(uint8_t * FullLinkControlDataBytes, uint32_t * CRCComputed, uint32_t CRCMask);
uint8_t ComputeCrc5Bit(uint8_t * DMRData);
uint8_t * DmrAlgIdToStr(uint8_t AlgID);
uint8_t * DmrAlgPrivacyModeToStr(uint32_t PrivacyMode);

void ProcessDmrPIHeader(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessCSBK(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessDataData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void Process1Data(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void Process12Data(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void Process34Data(dsd_opts * opts, dsd_state * state, unsigned char tdibits[98], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessMBCData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessMBChData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessWTFData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
void ProcessUnifiedData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20]);
//LFSR code courtesy of https://github.com/mattames/LFSR/
int LFSR(dsd_state * state);
int LFSRP(dsd_state * state);

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
void dstar_header_decode(dsd_state * state, int radioheaderbuffer[660]);

#ifdef __cplusplus
extern "C" {
#endif
void open_rtlsdr_stream(dsd_opts *opts);
void cleanup_rtlsdr_stream();
void get_rtlsdr_sample(int16_t *sample);
void rtlsdr_sighandler();

//TRELLIS
void CDMRTrellisTribitsToBits(const unsigned char* tribits, unsigned char* payload);
unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* tribits);
//unsigned int CDMRTrellisCheckCode(const unsigned char* points, unsigned char* state, unsigned char* tribits);
bool CDMRTrellisFixCode(unsigned char* points, unsigned int failPos, unsigned char* payload);
void CDMRTrellisDeinterleave(const unsigned char* data, signed char* dibits);
void CDMRTrellisDibitsToPoints(const signed char* dibits, unsigned char* points);
void CDMRTrellisPointsToDibits(const unsigned char* points, signed char* dibits);
void CDMRTrellisBitsToTribits(const unsigned char* payload, unsigned char* tribits);
bool CDMRTrellisDecode(const unsigned char* data, unsigned char* payload);



#ifdef __cplusplus
}
#endif

#endif // DSD_H
