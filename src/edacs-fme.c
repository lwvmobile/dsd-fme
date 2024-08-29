/*-------------------------------------------------------------------------------
 * EDACS-FME
 * A program for decoding EDACS (ported to DSD-FME)
 * https://github.com/lwvmobile/edacs-fm
 *
 * Portions of this software originally from:
 * https://github.com/sp5wwp/ledacs
 * XTAL Labs
 * 30 IV 2016
 * Many thanks to SP5WWP for permission to use and modify this software
 *
 * Encoder/decoder for binary BCH codes in C (Version 3.1)
 * Robert Morelos-Zaragoza
 * 1994-7
 *
 * LWVMOBILE
 * 2023-11 Version EDACS-FM Florida Man Edition
 *
 * ilyacodes
 * 2024-03 rewrite EDACS standard parsing to spec, add reverse-engineered EA messages
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

char * getLcnStatusString(int lcn)
{
  if (lcn == 26 || lcn == 27)
    return "[Reserved LCN Status]";
  if (lcn == 28)
    return "[Convert To Callee]";
  else if (lcn == 29)
    return "[Call Queued]";
  else if (lcn == 30)
    return "[System Busy]";
  else if (lcn == 31)
    return "[Call Denied]";
  else
    return "";
}

int isAgencyCallGroup(int afs, dsd_state * state)
{
  int fs_mask = state->edacs_s_mask | (state->edacs_f_mask << state->edacs_f_shift);
  return (afs & fs_mask) == 0;
}

int isFleetCallGroup(int afs, dsd_state * state)
{
  if (isAgencyCallGroup(afs, state))
    return 0;

  return (afs & state->edacs_s_mask) == 0;
}

//Bitwise vote-compare the three copies of a message received. Note that fr_2 and fr_5 are transmitted inverted.
unsigned long long int edacsVoteFr(unsigned long long int fr_1_4, unsigned long long int fr_2_5, unsigned long long int fr_3_6)
{
  fr_2_5 = (~fr_2_5) & 0xFFFFFFFFFF;

  unsigned long long int msg_result = 0;
  for (int i = 0; i < 40; i++)
  {
    int bit_1 = (fr_1_4 >> i) & 1;
    int bit_2 = (fr_2_5 >> i) & 1;
    int bit_3 = (fr_3_6 >> i) & 1;

    //Vote: the value of the bit that we see the most is what we assume is correct
    if (bit_1 + bit_2 + bit_3 > 1) {
      // Note that we have to specify long long on the literal 1 to shift it more than 32 bits left
      msg_result |= (1ll << i);
    }
  }

  return msg_result & 0xFFFFFFFFFF;
}

void openWavOutFile48k (dsd_opts * opts, dsd_state * state)
{
  UNUSED(state);
  SF_INFO info;
  info.samplerate = 48000; //48k for analog output (has to match input)
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_RDWR, &info);

  if (opts->wav_out_f == NULL)
  {
    fprintf (stderr,"Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }
}

//listening to and playing back analog audio
void edacs_analog(dsd_opts * opts, dsd_state * state, int afs, unsigned char lcn)
{
  int i, result;
  int count = 5; //RMS has a 5 count (5 * 180ms) now before cutting off;
  short analog1[960];
  short analog2[960];
  short analog3[960];
  short sample = 0;

  // #define DEBUG_ANALOG //enable to digitize analog if 'data' bursts heard

  uint8_t d1[192];
  uint8_t d2[192];
  uint8_t d3[192];

  state->last_cc_sync_time = time(NULL);
  state->last_vc_sync_time = time(NULL);

  memset (analog1, 0, sizeof(analog1));
  memset (analog2, 0, sizeof(analog2));
  memset (analog3, 0, sizeof(analog3));

  memset (d1, 0, sizeof(d1));
  memset (d2, 0, sizeof(d2));
  memset (d3, 0, sizeof(d3));

  long int rms = opts->rtl_squelch_level + 1; //one more for the initial loop phase
  long int sql = opts->rtl_squelch_level;

  fprintf (stderr, "\n");

  while (!exitflag && count > 0)
  {
    //this will only work on 48k/1 short output
    if (opts->audio_in_type == 0)
    {
      for (i = 0; i < 960; i++)
      {
        pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        analog1[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        analog2[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        analog3[i] = sample;
      }
      //this rms will only work properly (for now) with squelch enabled in SDR++ or other
      rms = raw_rms(analog3, 960, 1);
    }

    //NOTE: The core dumps observed previously were due to SDR++ Remote Server connection dropping due to Internet/Other issues
    //and unlike in the main livescanner loop where it just hangs, this loop will cause a core dump. The observed issue
    //has not occurred when using SDR++ on local hardware, just the remote server software over the Internet.

    //NOTE: The fix below does not apply to above observed issue, as the TCP connection will not drop, there will just
    //not be a sample to read in and it hangs on sf_short read until it crashes out, the fix below will prevent issues
    //when SDR++ is closed locally, or the TCP connection closes suddenly.

    //NOTE: Observed two segfaults on EDACS STM analog when doing radio tests or otherwise holding the radio
    //open for extremely long periods of time, could be an issue in digitize where dibit_buf_p is not
    //reset for an extended period of time and overflows, may need to reset buffer occassionally here

    //TCP Input w/ Simple TCP Error Detection Implemented to prevent hard crash if TCP drops off
    if (opts->audio_in_type == 8)
    {
      for (i = 0; i < 960; i++)
      {
        result = sf_read_short(opts->tcp_file_in, &sample, 1);
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected (EDACS Analog).\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          cleanupAndExit(opts, state);
        }
        analog1[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        result = sf_read_short(opts->tcp_file_in, &sample, 1);
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected (EDACS Analog).\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          cleanupAndExit(opts, state);
        }
        analog2[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        result = sf_read_short(opts->tcp_file_in, &sample, 1);
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected (EDACS Analog).\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          cleanupAndExit(opts, state);
        }
        analog3[i] = sample;
      }

      //this rms will only work properly (for now) with squelch enabled in SDR++
      rms = raw_rms(analog3, 960, 1);
    }

    //RTL Input
    #ifdef USE_RTLSDR
    if (opts->audio_in_type == 3)
    {
      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
        sample *= opts->rtl_volume_multiplier;
        analog1[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
        sample *= opts->rtl_volume_multiplier;
        analog2[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
        sample *= opts->rtl_volume_multiplier;
        analog3[i] = sample;
      }
      //the rtl rms value works properly without needing a 'hard' squelch value
      rms = rtl_return_rms();
    }
    #endif

    //digitize analog samples for a dotting sequence check -- moved here before filtering is applied
    unsigned long long int sr = 0;
    for (i = 0; i < 960; i+=5) //Samples Per Symbol is 5, so incrememnt every 5
    {
      sr = sr << 1;
      sr += digitize (opts, state, (int)analog1[i]);
    }

    #ifdef DEBUG_ANALOG
    //save digitized samples to array for looking into those 'data' sounding bursts,
    //this format assumes the same sample per symbol rateused in EDACS/PV
    for (i = 0; i < 192; i++) //Samples Per Symbol is 5, so incrememnt every 5
    {
      d1[i] = digitize (opts, state, (int)analog1[i*5]);
      d2[i] = digitize (opts, state, (int)analog2[i*5]);
      d3[i] = digitize (opts, state, (int)analog3[i*5]);
    }
    #endif

    //Bugfix for buffer overflow from using digitize function, reset buffers
    if (state->dibit_buf_p > state->dibit_buf + 900000)
      state->dibit_buf_p = state->dibit_buf + 200;

    //dmr buffer
    if (state->dmr_payload_p > state->dmr_payload_buf + 900000)
      state->dmr_payload_p = state->dmr_payload_buf + 200;

    // low pass filter
    if (opts->use_lpf == 1)
    {
      lpf (state, analog1, 960);
      lpf (state, analog2, 960);
      lpf (state, analog3, 960);
    }

    //high pass filter
    if (opts->use_hpf == 1)
    {
      hpf (state, analog1, 960);
      hpf (state, analog2, 960);
      hpf (state, analog3, 960);
    }

    //pass band filter
    if (opts->use_pbf == 1)
    {
      pbf (state, analog1, 960);
      pbf (state, analog2, 960);
      pbf (state, analog3, 960);
    }

    //manual gain control
    if (opts->audio_gainA > 0.0f)
    {
      analog_gain (opts, state, analog1, 960);
      analog_gain (opts, state, analog2, 960);
      analog_gain (opts, state, analog3, 960);
    }

    //automatic gain control
    else
    {
      agsm (opts, state, analog1, 960);
      agsm (opts, state, analog2, 960);
      agsm (opts, state, analog3, 960);
    }

    //NOTE: Ideally, we would run raw_rms for TCP/VS here, but the analog spike on EDACS (STM)
    //system gets filtered out, and when they hold the radio open and don't talk,
    //it counts against the squelch hit as no audio, so we will just have to use
    //the squelch checkbox in SDR++ and similar when using those input methods
    // if (opts->audio_in_type != 3)
    //   rms = raw_rms(analog3, 960, 1);

    //reconfigured to use seperate audio out stream that is always 48k short
    if (opts->audio_out_type == 0 && opts->slot1_on == 1)
    {
      pa_simple_write(opts->pulse_raw_dev_out, analog1, 960*2, NULL);
      pa_simple_write(opts->pulse_raw_dev_out, analog2, 960*2, NULL);
      pa_simple_write(opts->pulse_raw_dev_out, analog3, 960*2, NULL);
    }

    if (opts->audio_out_type == 8) //UDP Audio
    {
      udp_socket_blasterA (opts, state, 960*2, analog1);
      udp_socket_blasterA (opts, state, 960*2, analog2);
      udp_socket_blasterA (opts, state, 960*2, analog3);
    }

    //added a condition check so that if OSS output and 8K, switches to 48K when opening OSS
    if (opts->audio_out_type == 5 && opts->floating_point == 0 && opts->slot1_on == 1)
    {
      write (opts->audio_out_fd, analog1, 960*2);
      write (opts->audio_out_fd, analog2, 960*2);
      write (opts->audio_out_fd, analog3, 960*2);
    }

    //STDOUT -- I don't see the harm of adding this here, will be fine for analog only or digital only (non-mixed analog and digital)
    if (opts->audio_out_type == 1 && opts->floating_point == 0 && opts->slot1_on == 1)
    {
      write (opts->audio_out_fd, analog1, 960*2);
      write (opts->audio_out_fd, analog2, 960*2);
      write (opts->audio_out_fd, analog3, 960*2);
    }

    opts->rtl_rms = rms;


    printFrameSync (opts, state, " EDACS", 0, "A");

    if (rms < sql) count--;
    else count = 5;

    if (rms > sql) fprintf(stderr, "%s", KGRN);
    else fprintf(stderr, "%s", KRED);

    fprintf (stderr, " Analog RMS: %04ld SQL: %ld", rms, sql);
    if (state->ea_mode == 0)
    {
      int a = (afs >> state->edacs_a_shift) & state->edacs_a_mask;
      int f = (afs >> state->edacs_f_shift) & state->edacs_f_mask;
      int s = afs & state->edacs_s_mask;
      fprintf (stderr, " AFS [%03d] [%02d-%02d%01d] LCN [%02d]", afs, a, f, s, lcn);
    }
    else
    {
      if (afs == -1) fprintf (stderr, " TGT [ SYSTEM ] LCN [%02d] All-Call", lcn);
      else           fprintf (stderr, " TGT [%08d] LCN [%02d]", afs, lcn);
    }

    //debug, view hit counter
    // fprintf (stderr, " CNT: %d; ", count);

    if (opts->floating_point == 1)
      fprintf (stderr, "Analog Floating Point Output Not Supported");

    //Update Ncurses Terminal
    if (opts->use_ncurses_terminal == 1)
      ncursesPrinter(opts, state);

    //write to wav file if opened
    if (opts->wav_out_f != NULL)
    {
      sf_write_short(opts->wav_out_f, analog1, 960);
      sf_write_short(opts->wav_out_f, analog2, 960);
      sf_write_short(opts->wav_out_f, analog3, 960);
    }

    //debug
    // fprintf (stderr, " SR: %016llX", sr);

    if (sr == 0xAAAAAAAAAAAAAAAA || sr == 0x5555555555555555)
      count = 0; //break while loop, sr will not equal these if just random noise

    fprintf (stderr, "%s", KNRM);

    #ifdef PRETTY_COLORS
    {} //do nothing
    #else
    fprintf (stderr, "SQL HIT: %d; ", 5-count); //add count since user can't see red or green
    #endif

    #ifdef DEBUG_ANALOG
    //debug digitized version of analog out when data bursts may be present
    //NOTE: Without a 'framesync' these could be shifted into odd positions
    if (opts->payload == 1)
    {
      fprintf (stderr, "\n A_DUMP: ");
      for (i = 0; i < 24; i++)
        fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&d1[i*8], 8));
      fprintf (stderr, "\n         ");
      for (i = 0; i < 24; i++)
        fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&d2[i*8], 8));
      fprintf (stderr, "\n         ");
      for (i = 0; i < 24; i++)
        fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&d3[i*8], 8));
      // fprintf (stderr, "\n");
    }
    #endif

    if (count > 0) fprintf (stderr, "\n");

  }
}

void edacs(dsd_opts * opts, dsd_state * state)
{
  //calculate afs shifts and masks (if user cycles them around)

  //quick sanity check, if bit tallies are not 11, reset to default 4:4:3 configuration
  if ( (state->edacs_a_bits + state->edacs_f_bits + state->edacs_s_bits) != 11)
  {
    state->edacs_a_bits = 4;
    state->edacs_f_bits = 4;
    state->edacs_s_bits = 3;
  }

  //calculate shifts by totalling preceeding bits
  state->edacs_a_shift = state->edacs_f_bits + state->edacs_s_bits;
  state->edacs_f_shift = state->edacs_s_bits;

  //calculate masks using bitwise math
  state->edacs_a_mask = (1 << state->edacs_a_bits) - 1;
  state->edacs_f_mask = (1 << state->edacs_f_bits) - 1;
  state->edacs_s_mask = (1 << state->edacs_s_bits) - 1;

  char * timestr = NULL;
  char * datestr = NULL;
  timestr = getTime();
  datestr = getDate();

  state->edacs_vc_lcn = -1; //init on negative for ncurses and tuning

  int i;
  int edacs_bit[241] = {0}; //zero out bit array and collect bits into it.

  for (i = 0; i < 240; i++) //288 bits every transmission minus 48 bit (24 dibit) sync pattern
  {
    edacs_bit[i] = getDibit (opts, state); //getDibit returns binary 0 or 1 on GFSK signal (Edacs and PV)
  }

  //Each EDACS outbound frame consists of two 40-bit (28-bit data, 12-bit BCH) messages. Each message is sent three
  //times, with the middle message bitwise-inverted. We use unsigned long long int here to be safe in 32-bit cygwin (not
  //sure if this was actually an issue).
  unsigned long long int fr_1 = 0;
  unsigned long long int fr_2 = 0;
  unsigned long long int fr_3 = 0;
  unsigned long long int fr_4 = 0;
  unsigned long long int fr_5 = 0;
  unsigned long long int fr_6 = 0;

  //push the edacs_bit array into fr format from EDACS-FM
  for (i = 0; i < 40; i++)
  {
    //only fr_1 and fr4 are going to matter
    fr_1 = fr_1 << 1;
    fr_1 = fr_1 | edacs_bit[i];
    fr_2 = fr_2 << 1;
    fr_2 = fr_2 | edacs_bit[i+40];
    fr_3 = fr_3 << 1;
    fr_3 = fr_3 | edacs_bit[i+80];

    fr_4 = fr_4 << 1;
    fr_4 = fr_4 | edacs_bit[i+120];
    fr_5 = fr_5 << 1;
    fr_5 = fr_5 | edacs_bit[i+160];
    fr_6 = fr_6 << 1;
    fr_6 = fr_6 | edacs_bit[i+200];

  }

  //Take our 3 copies of the first and second message and vote them to extract the two "error-corrected" messages
  unsigned long long int msg_1_ec = edacsVoteFr(fr_1, fr_2, fr_3);
  unsigned long long int msg_2_ec = edacsVoteFr(fr_4, fr_5, fr_6);

  //Get just the 28-bit message portion
  unsigned long long int msg_1_ec_m = msg_1_ec >> 12;
  unsigned long long int msg_2_ec_m = msg_2_ec >> 12;

  //Take the message and create a new crc for it. If the newly crc-ed message matches the old one, we have a good frame.
  unsigned long long int msg_1_ec_new_bch = edacs_bch(msg_1_ec_m) & 0xFFFFFFFFFF;
  unsigned long long int msg_2_ec_new_bch = edacs_bch(msg_2_ec_m) & 0xFFFFFFFFFF;

  //Rename the message variables (sans BCH) for cleaner code below
  unsigned long long int msg_1 = msg_1_ec >> 12;
  unsigned long long int msg_2 = msg_2_ec >> 12;

  if (msg_1_ec != msg_1_ec_new_bch || msg_2_ec != msg_2_ec_new_bch)
  {
    fprintf (stderr, " BCH FAIL ");
  }
  else //BCH Pass, continue from here.
  {

    //Auto Detection Modes Have Been Removed due to reliability issues,
    //users will now need to manually specify these options:
    /*
      -fh             Decode only EDACS Standard/ProVoice*\n");
      -fH             Decode only EDACS Standard/ProVoice with ESK 0xA0*\n");
      -fe             Decode only EDACS EA/ProVoice*\n");
      -fE             Decode only EDACS EA/ProVoice with ESK 0xA0*\n");

      (A) key toggles mode; (S) key toggles mask value in ncurses
    */

    //TODO: Consider re-adding the auto code to make a suggestion to users
    //as to which mode to proceed in?

    //Color scheme:
    // - KRED - critical information (emergency, failsoft, etc)
    // - KYEL - system data
    // - KGRN - voice group calls
    // - KCYN - voice individual calls
    // - KMAG - voice other calls (interconnect, all-call, test call, etc)
    // - KBLU - subscriber data
    // - KWHT - unknown/reserved/special

    //Account for ESK, if any
    unsigned long long int fr_esk_mask = ((unsigned long long int)state->esk_mask) << 20;
    msg_1 = msg_1 ^ fr_esk_mask;
    msg_2 = msg_2 ^ fr_esk_mask;

    //Start Extended Addressing Mode
    if (state->ea_mode == 1)
    {
      unsigned char mt1 = (msg_1 & 0xF800000) >> 23;
      unsigned char mt2 = (msg_1 & 0x780000) >> 19;

      state->edacs_vc_call_type = 0;

      //TODO: initialize where they are actually used
      unsigned long long int site_id = 0; //we probably could just make this an int as well as the state variables
      unsigned char lcn = 0;

      //Add raw payloads and MT1/MT2 for easy debug
      if (opts->payload == 1)
      {
        fprintf (stderr, " MSG_1 [%07llX]", msg_1);
        fprintf (stderr, " MSG_2 [%07llX]", msg_2);
        fprintf (stderr, " (MT1: %02X", mt1);
        // MT2 is meaningless if MT1 is not 0x1F
        if (mt1 == 0x1F)
          fprintf (stderr, "; MT2: %X) ", mt2);
        else
          fprintf (stderr, ")         ");
      }

      //MT1 of 0x1F indicates to use MT2 for the opcode. See US patent US7546135B2, Figure 2b.
      if (mt1 == 0x1F)
      {

        //Initiate Test Call (finally captured in the wild on SLERS EA, along with the "I-Call" with target and source of 0)
        if (mt2 == 0x0)
        {
          // MSG_1             [F802180] MSG_2 [0000000] (MT1: 1F; MT2: 0)  Initiate Test Call
          int cc_lcn = (msg_1 & 0x3E000) >> 13; //shifted to allow this example to be CC LCN 1, as was reported at the time of capture
          int wc_lcn =   (msg_1 & 0xF80) >> 7;

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Initiate Test Call :: CC LCN [%02d] WC LCN [%02d]", cc_lcn, wc_lcn);
          fprintf (stderr, "%s", KNRM);

          state->edacs_vc_lcn = wc_lcn;
          //assign bogus values so that this will show up in ncurses terminal
          //and overwrite current values in the matrix
          state->lasttg  = 999999999;
          state->lastsrc = 999999999;
          state->edacs_vc_call_type = EDACS_IS_VOICE | EDACS_IS_TEST_CALL;
        }
        //Adjacent Sites
        else if (mt2 == 0x1)
        {
          int adj_lcn  = (msg_1 & 0x1F000) >> 12;
          int adj_idx  = (msg_1 & 0xF00) >> 8; //site 177 has 8 adj_sites, so this appears to be a 4-bit value
          int adj_site = (msg_1 & 0xFF);

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Adjacent Site");
          if (adj_site > 0)
            fprintf (stderr, " :: Site ID [%02X][%03d] Index [%d] on CC LCN [%02d]%s", adj_site, adj_site, adj_idx, adj_lcn, getLcnStatusString(lcn));
          else fprintf (stderr, " :: Total Indexed [%d]", adj_idx); //if site value is 0, then this tells us the total number of adjacent sites

          if (adj_site == 0 && adj_idx == 0)      fprintf (stderr, " [Adjacency Table Reset]");
          else if (adj_site != 0 && adj_idx == 0) fprintf (stderr, " [Priority System Definition]");
          else if (adj_site == 0 && adj_idx != 0) fprintf (stderr, " [Adjacencies Table Length Definition]");
          else                                    fprintf (stderr, " [Adjacent System Definition]");
          fprintf (stderr, "%s", KNRM);
        }
        //Status/Message
        else if (mt2 == 0x4)
        {
          int status = msg_1 & 0xFF;
          int source = msg_2 & 0xFFFFF;

          fprintf (stderr, "%s", KBLU);
          if (status == 248) fprintf (stderr, " Status Request :: Target [%08d]", source);
          else               fprintf (stderr, " Message Acknowledgement :: Status [%03d] Source [%08d]", status, source);
          fprintf (stderr, "%s", KNRM);
        }
        //Unit Enable / Disable
        else if (mt2 == 0x7)
        {
          int qualifier = (msg_2 & 0xC000000) >> 26;
          int target    = (msg_2 & 0xFFFFF);

          fprintf (stderr, "%s", KBLU);
          fprintf (stderr, " Unit Enable/Disable ::");
          if (qualifier == 0x0)      fprintf (stderr, " [Temporary Disable]");
          else if (qualifier == 0x1) fprintf (stderr, " [Corrupt Personality]");
          else if (qualifier == 0x2) fprintf (stderr, " [Revoke Logical ID]");
          else                       fprintf (stderr, " [Re-enable Unit]");
          fprintf (stderr, " Target [%08d]", target);
          fprintf (stderr, "%s", KNRM);
        }
        //Control Channel LCN
        else if (mt2 == 0x8)
        {
          int system = msg_1 & 0xFFFF;
          int lcn    = msg_2 & 0x1F;

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " System Information");
          if (lcn != 0)
          {
            state->edacs_cc_lcn = lcn;
            if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) //26, or 27. shouldn't matter don't think cc lcn will give a status lcn val
            {
              state->edacs_lcn_count = state->edacs_cc_lcn;
            }
            fprintf (stderr, " :: System ID [%04X] CC LCN [%02d]%s", system, state->edacs_cc_lcn, getLcnStatusString(lcn));

            //check for control channel lcn frequency if not provided in channel map or in the lcn list
            if (state->trunk_lcn_freq[state->edacs_cc_lcn-1] == 0)
            {
              long int lcnfreq = 0;
              //if using rigctl, we can ask for the currrent frequency
              if (opts->use_rigctl == 1)
              {
                lcnfreq = GetCurrentFreq (opts->rigctl_sockfd);
                if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
              }
              //if using rtl input, we can ask for the current frequency tuned
              if (opts->audio_in_type == 3)
              {
                lcnfreq = (long int)opts->rtlsdr_center_freq;
                if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn-1] = lcnfreq;
              }
            }

            //set trunking cc here so we know where to come back to
            if (opts->p25_trunk == 1 && state->trunk_lcn_freq[state->edacs_cc_lcn-1] != 0)
            {
              state->p25_cc_freq = state->trunk_lcn_freq[state->edacs_cc_lcn-1]; //index starts at zero, lcn's locally here start at 1
            }
          }
          fprintf (stderr, "%s", KNRM);
        }
        //Site ID
        else if (mt2 == 0xA)
        {
          site_id  = ((msg_1 & 0x7000) >> 7) | (msg_1 & 0x1F);
          int area = (msg_1 & 0xFE0) >> 5;

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Extended Addressing :: Site ID [%02llX][%03lld] Area [%02X][%03d]", site_id, site_id, area, area);
          fprintf (stderr, "%s", KNRM);
          state->edacs_site_id = site_id;
        }
        //System Dynamic Regroup Plan Bitmap
        else if (mt2 == 0xB)
        {
          int bank_1     = (msg_1 & 0x10000) >> 16;
          int resident_1 = (msg_1 & 0xFF00) >> 8;
          int active_1   = (msg_1 & 0xFF);
          int bank_2     = (msg_2 & 0x10000) >> 16;
          int resident_2 = (msg_2 & 0xFF00) >> 8;
          int active_2   = (msg_2 & 0xFF);

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " System Dynamic Regroup Plan Bitmap");

          //this message gets EXTREMELY LONG, so lets hide it behind the payload verbosity
          if (opts->payload == 1)
          {
            // Deduplicate some code with a for (foreach would have been great here)
            for (int i = 0; i < 2; i++)
            {
              int bank;
              int resident;
              int active;

              switch (i) {
                case 0:
                  bank     = bank_1;
                  resident = resident_1;
                  active   = active_1;
                  break;
                case 1:
                  bank     = bank_2;
                  resident = resident_2;
                  active   = active_2;
                  break;
              }

              fprintf (stderr, " :: Plan Bank [%1d] Resident [", bank);

              int plan = bank * 8;
              int first = 1;
              while (resident != 0) {
                if ((resident & 0x1) == 1) {
                  if (first == 1)
                  {
                    first = 0;
                    fprintf (stderr, "%d", plan);
                  }
                  else
                  {
                    fprintf (stderr, ", %d", plan);
                  }
                }
                resident >>= 1;
                plan++;
              }

              fprintf (stderr, "] Active [");

              plan = bank * 8;
              first = 1;
              while (active != 0) {
                if ((active & 0x1) == 1) {
                  if (first == 1)
                  {
                    first = 0;
                    fprintf (stderr, "%d", plan);
                  }
                  else
                  {
                    fprintf (stderr, ", %d", plan);
                  }
                }
                active >>= 1;
                plan++;
              }

              fprintf (stderr, "]");
            }
          } //end payload

          fprintf (stderr, "%s", KNRM);
        }
        //Patch Groups / Dynamic Regroup -- Current Code
        // else if (mt2 == 0xC)
        // {
        //   //Note: First 9 bits of msg_1 are the mt1 and mt2 bits
        //   int unk1    = (msg_1 & 0x70000) >> 16;   //unknown 3 bit value preceeding the SGID
        //   int sgid    = (msg_1 & 0xFFFF);          //patched supergroup ID

        //   //Updated Observation: The 'SSN' value may not be unique in this instance, so may be an entirely different value
        //   //altogether. Its function is still unknown, but for the sake of displaying patches, is not required.

        //   int ssn     = (msg_2 & 0xFF00000) >> 20; //this value seems to incrememnt based on SGID, so assigning 8-bit as the SSN
        //   int target  = (msg_2 &   0xFFFFF);      //target group or individual ID (20-bit) to include in supergroup

        //   fprintf (stderr, "%s", KWHT);
        //   fprintf (stderr, " System Dynamic Regroup :: SGID [%05d] Target [%07d]", sgid, target);
        //   if (unk1) fprintf (stderr, " UNK1 [%01X]", unk1); //this value seems to always be 7 for an active patch, 0 for a termination of a patch
        //   if (ssn)  fprintf (stderr, " UNK2 [%02X]", ssn); //this may or may not be a unique value to each SGID, is FE for termination of a patch
        //   fprintf (stderr, "%s", KNRM);
        // }

        //Patch Groups / Dynamic Regroup -- Reverse Engineer WIP from observations, guesswork, and documented P25 MFID A4 regroup operations
        else if (mt2 == 0xC)
        {

          //Note: Due to the method used to reverse engineer this patch using conventions from a newer documented P25 source (MFID A4 L3Harris),
          //its possible that the conventions, terminology, and bits signalled in this message are not entirely accurate, but are just speculation

          //Note: First 9 bits of msg_1 are the mt1 and mt2 bits
          //                     FF80000 (visualization aide)
          int tga     = (msg_1 & 0x70000) >> 16;  //unknown 3 bit value preceeding the SGID (TGA?)
          int unk1    = (msg_1 &  0xFF00) >> 8;  //unknown 8 bit value preceeding the TGA/Options
          int sgid    = (msg_1 &    0xFF);       //patched supergroup ID expressed as an 8 bit value

          //Observation: The SSN value appears to be unique, but also different between sites, as in, each site
          //has its own SSN for the same SGID patching, possibly a first come first serve pool value?
          int ssn     = (msg_2 & 0xF800000) >> 23; //this value seems to incrememnt based on SGID, 5-bit value
          int unk2    = (msg_2 &  0x700000) >> 20; //unknown 3-bit value, possibly linked to TGA when patch is deleted
          int target  = (msg_2 &   0xFFFFF);      //target group or individual ID (20-bit) to include in supergroup

          //look at the 'zero' fields when on 'delete'
          //              MSG_1  [FE00045] MSG_2 [FE00045]
          //                      FF80000 (visualization aide)
          int unk3     = (msg_1 & 0x7FF00) >> 8;
          int unk4     = (msg_2 & 0x7FF00) >> 8;

          //Ilya, please don't nit fix my logging format for these, it breaks grep when parsing a bunch of these all at once
          fprintf (stderr, "%s", KWHT);
          fprintf (stderr, " System Dynamic Regroup :: SP-WGID: %03d; Target: %07d;", sgid, target);
          
          if (sgid != target)
          {
            //decode potential TGA values (assumming same as Harris P25)
            //decided to disable the info presented below since I cannot confirm this
            // if (tga & 4) fprintf (stderr, " One-Way"); //Simulselect
            // else         fprintf (stderr, " Two-Way"); //Patch
            // if (tga & 2) fprintf (stderr, " Group");
            // else         fprintf (stderr, " Radio"); //Individual
                         fprintf (stderr, " Patch");
            if (tga & 1) fprintf (stderr, " Active;");
            else         fprintf (stderr, " Delete;");

            //switched from using the TGA nomenclature to a more generic OPTion since I think this value is still a form of option
            fprintf (stderr, " OPT: %01X;", tga); //this value seems to always be 7 for an active patch, 0 for a termination of a patch (6 if assigned the unk2 value)
            if (unk1)
              fprintf (stderr, " UNK1: %01X;", unk1);
            if (unk2)
              fprintf (stderr, " UNK2: %02X;", unk2);
            fprintf (stderr, " SSN: %02X;", ssn); //this may or may not be a unique value to each SGID, is 1F for termination of a patch
          }
          
          //07:25:17 Sync: +EDACS  MSG_1 [FE00045] MSG_2 [FE00045] (MT1: 1F; MT2: C)  System Dynamic Regroup :: SP-WGID: 069; Target: 0000069; One-Way Group Patch Delete TGA: 6;
          //Upon Reflection, thinking the same MT1 and MT2 values are in both messages, so appears to just be the SP-WGID here and a bunch of zeroes leading into it, and not a TGA value

          else
          {
            fprintf (stderr, " Patch Delete;");
            if (unk3)
              fprintf (stderr, " UNK3: %01X;", unk3);
            if (unk4)
              fprintf (stderr, " UNK4: %02X;", unk4);
          }

          fprintf (stderr, "%s", KNRM);
        }
        //Serial Number Request (not seen in the wild, see US patent 20030190923, Figure 2b)
        else if (mt2 == 0xD)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Serial Number Request");
          fprintf (stderr, "%s", KNRM);
        }
        else
        {
          fprintf (stderr, "%s", KWHT);
          fprintf (stderr, " Unknown Command");
          fprintf (stderr, "%s", KNRM);
          // Only print the payload if we haven't already printed it
          if (opts->payload != 1)
          {
            fprintf (stderr, " ::");
            fprintf (stderr, " MSG_1 [%07llX]", msg_1);
            fprintf (stderr, " MSG_2 [%07llX]", msg_2);
          }
        }

      }
      //TDMA Group Grant Update (never observed, unknown if ever used on any EDACS system)
      else if (mt1 == 0x1)
      {
        lcn        = (msg_1 & 0x3E0000) >> 17;
        int group  = (msg_1 & 0xFFFF);
        int source = (msg_2 & 0xFFFFF);

        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " TDMA Group Call :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, getLcnStatusString(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //Data Group Grant Update
      else if (mt1 == 0x2)
      {
        lcn        = (msg_1 & 0x3E0000) >> 17;
        int group  = (msg_1 & 0xFFFF);
        int source = (msg_2 & 0xFFFFF);

        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Data Group Call :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, getLcnStatusString(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //Voice Call Grant Update
      // MT1 value determines the type of group call:
      // - 0x03 digital group voice (ProVoice, standard on SLERS EA)
      // - 0x06 analog group voice
      else if (mt1 == 0x3 || mt1 == 0x6)
      {
        lcn = (msg_1 & 0x3E0000) >> 17;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital     = (mt1 == 0x3) ? 1 : 0;
        int is_update      = (msg_1 & 0x10000) >> 16;
        int group          = (msg_1 & 0xFFFF);
        int is_tx_trunking = (msg_2 & 0x200000) >> 21;
        int is_emergency   = (msg_2 & 0x100000) >> 20;
        int source         = (msg_2 & 0xFFFFF);

        //Call info for state
        if (lcn != 0)  { state->edacs_vc_lcn = lcn; }
                         state->lasttg = group; // 0 is a valid TG, it's the all-call for agency 0
        if (source != 0) state->lastsrc = source;

        //Call type for state
                               state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_GROUP;
        if (is_digital == 1)   state->edacs_vc_call_type |= EDACS_IS_DIGITAL;
        if (is_emergency == 1) state->edacs_vc_call_type |= EDACS_IS_EMERGENCY;

        fprintf (stderr, "%s", KGRN);
        if (is_digital == 0) fprintf (stderr, " Analog Group Call");
        else                 fprintf (stderr, " Digital Group Call");
        if (is_update == 0) fprintf (stderr, " Assignment");
        else                fprintf (stderr, " Update");
        fprintf (stderr, " :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, getLcnStatusString(lcn));

        //Trunking mode is correlated to (but not guaranteed to match) the type of call:
        // - emergency calls - usually message trunking
        // - normal calls - usually transmission trunking
        if (is_tx_trunking == 0) fprintf (stderr, " [Message Trunking]");
        if (is_emergency == 1)
        {
          fprintf (stderr, "%s", KRED);
          fprintf (stderr, " [EMERGENCY]");
        }
        fprintf (stderr, "%s", KNRM);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == group)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on EDACS EA -- block non-matching target, allow matching group
        if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");

        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %d SRC %d.wav", datestr, timestr, state->edacs_site_id, group, source);
              if (is_digital == 1)
                openWavOutFile (opts, state);
              else
                openWavOutFile48k (opts, state);
            }

            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, group, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, group, lcn);
              #endif
            }

          }

        }
      }
      //I-Call/Test Call Assignment/Update
      else if (mt1 == 0x10)
      {
        lcn = (msg_2 & 0x1F00000) >> 20;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital = (msg_1 & 0x200000) >> 21;
        int is_update  = (msg_1 & 0x100000) >> 20;
        int target     = (msg_1 & 0xFFFFF);
        int source     = (msg_2 & 0xFFFFF);

        //Call info for state
        if (lcn != 0)    state->edacs_vc_lcn = lcn;
        if (target != 0) state->lasttg = target;
        if (source != 0) state->lastsrc = source;

        //Call type for state
        if (target == 0 && source == 0) state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_TEST_CALL;
        else                            state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_INDIVIDUAL;
        if (is_digital == 1) state->edacs_vc_call_type |= EDACS_IS_DIGITAL;

        //Test calls are just I-Calls with source and target of 0
        if (target == 0 && source == 0)
        {
          fprintf (stderr, "%s", KMAG);
          fprintf (stderr, " Test Call");
          if (is_update == 0) fprintf (stderr, " Assignment");
          else                fprintf (stderr, " Update");
          fprintf (stderr, " :: LCN [%02d]%s", lcn, getLcnStatusString(lcn));
          state->edacs_vc_lcn = lcn;
          //assign bogus values so that this will show up in ncurses terminal
          //and overwrite current values in the matrix
          state->lasttg  = 999999999;
          state->lastsrc = 999999999;
          lcn = 0; //set to zero here, because this is not an actual call, so don't tune to it
        }
        else
        {
          fprintf (stderr, "%s", KCYN);
          if (is_digital == 0) fprintf (stderr, " Analog I-Call");
          else                 fprintf (stderr, " Digital I-Call");
          if (is_update == 0) fprintf (stderr, " Assignment");
          else                fprintf (stderr, " Update");

          fprintf (stderr, " :: Target [%08d] Source [%08d] LCN [%02d]%s", target, source, lcn, getLcnStatusString(lcn));
        }

        fprintf (stderr, "%s", KNRM);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block - no allow/whitelist support for i-calls
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        //Get target mode for calls that are in the allow/whitelist
        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == target)
          {
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on EDACS EA I-CALL -- block non-matching target
        if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");

        //this is working now with the new import setup
        if (opts->trunk_tune_private_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %d SRC %d I-Call.wav", datestr, timestr, state->edacs_site_id, target, source);
              if (is_digital == 1)
                openWavOutFile (opts, state);
              else
                openWavOutFile48k (opts, state);
            }

            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, target, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, target, lcn);
              #endif
            }

          }
        }
      }
      //Channel assignment (unknown reason, just know it assigns an LCN in the expected order; believed related to data)
      else if (mt1 == 0x12)
      {
        lcn        = (msg_2 & 0x1F00000) >> 20;
        int source = (msg_2 & 0xFFFFF);

        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Channel Assignment (Unknown Data) :: Source [%08d] LCN [%02d]%s", source, lcn, getLcnStatusString(lcn));
        fprintf (stderr, "%s", KNRM);

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        //Call info for state
        if (lcn != 0)    state->edacs_vc_lcn = lcn;
        if (source != 0) state->lastsrc = source;

        //Call type for state
        state->edacs_vc_call_type = EDACS_IS_INDIVIDUAL;
      }
      //System All-Call Grant Update
      else if (mt1 == 0x16)
      {
        lcn = (msg_1 & 0x3E0000) >> 17;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital = (msg_1 & 0x10000) >> 16;
        int is_update  = (msg_1 & 0x8000) >> 15;
        int source     = (msg_2 & 0xFFFFF);

        //Call info for state
        if (lcn != 0)  { state->edacs_vc_lcn = lcn; }
                         state->lasttg = 0;
        if (source != 0) state->lastsrc = source;

        //Call type for state
                               state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_ALL_CALL;
        if (is_digital == 1)   state->edacs_vc_call_type |= EDACS_IS_DIGITAL;

        fprintf (stderr, "%s", KMAG);
        if (is_digital == 0) fprintf (stderr, " Analog System All-Call");
        else                 fprintf (stderr, " Digital System All-Call");
        if (is_update == 0) fprintf (stderr, " Assignment");
        else                fprintf (stderr, " Update");

        fprintf (stderr, " :: Source [%08d] LCN [%02d]%s", source, lcn, getLcnStatusString(lcn));
        fprintf (stderr, "%s", KNRM);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'A' to mode for allow - always allow all-calls by default
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "A");

        //TG hold on EDACS ALL-CALL -- block ALL CALL in favor of hold?
        // if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
        // if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");

        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld SRC %d All-Call.wav", datestr, timestr, state->edacs_site_id, source);
              if (is_digital == 1)
                openWavOutFile (opts, state);
              else
                openWavOutFile48k (opts, state);
            }

            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, -1, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0)
                edacs_analog(opts, state, -1, lcn);
              #endif
            }

          }

        }
      }
      //Login
      else if (mt1 == 0x19)
      {
        int group  = (msg_1 & 0xFFFF);
        int source = (msg_2 & 0xFFFFF);
        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Login :: Group [%05d] Source [%08d]", group, source);
        fprintf (stderr, "%s", KNRM);
      }
      //Unknown command
      else
      {
        fprintf (stderr, "%s", KWHT);
        fprintf (stderr, " Unknown Command");
        fprintf (stderr, "%s", KNRM);
        // Only print the payload if we haven't already printed it
        if (opts->payload != 1)
        {
          fprintf (stderr, " ::");
          fprintf (stderr, " MSG_1 [%07llX]", msg_1);
          fprintf (stderr, " MSG_2 [%07llX]", msg_2);
        }
      }

    }
    //Start Standard or Networked Mode
    else if (state->ea_mode == 0)
    {
      unsigned char mt_a = (msg_1 & 0xE000000) >> 25;
      unsigned char mt_b = (msg_1 & 0x1C00000) >> 22;
      unsigned char mt_d = (msg_1 & 0x3E0000) >> 17;

      state->edacs_vc_call_type = 0;

      //Add raw payloads and MT-A/MT-B/MT-D for easy debug
      if (opts->payload == 1)
      {
        fprintf (stderr, " MSG_1 [%07llX]", msg_1);
        fprintf (stderr, " MSG_2 [%07llX]", msg_2);
        fprintf (stderr, " (MT-A: %X", mt_a);
        // MT-B is meaningless if MT-A is not 0x7
        if (mt_a == 0x7)
        {
          fprintf (stderr, "; MT-B: %X", mt_b);
          // MT-D is meaningless if MT-B is not 0x7
          if (mt_b == 0x7)
          {
            fprintf (stderr, "; MT-D: %02X) ", mt_d);
          }
          else
          {
            fprintf (stderr, ")           ");
          }
        }
        else
        {
          fprintf (stderr, ")                    ");
        }
      }

      //The following is heavily based on TIA/EIA Telecommunications System Bulletin 69.3, "Enhanced Digital Access
      //Communications System (EDACS) Digital Air Interface for Channel Access, Modulation, Messages, and Formats",
      //April 1998. Where real world systems are found to diverge from this bulletin, please note the basis for the
      //deviation.

      //MT-A 0 and 1 as analog/digital mode indicator reverse engineered from Montreal STM and San Antonio/Bexar Co
      //systems; occurs immediately prior to Voice Group Channel Update.
      //
      //Voice Group Channel Assignment (6.2.4.1)
      //Emergency Voice Group Channel Assignment (6.2.4.2)
      if (mt_a == 0x0 || mt_a == 0x1 || mt_a == 0x2 || mt_a == 0x3)
      {
        int is_digital     = (mt_a == 0x2 || mt_a == 0x3) ? 1 : 0;
        int is_emergency   = (mt_a == 0x1 || mt_a == 0x3) ? 1 : 0;
        int lid            = ((msg_1 & 0x1FC0000) >> 11) | ((msg_2 & 0xFE0000) >> 17);
        int lcn            = (msg_1 & 0x1F000) >> 12;
        int is_tx_trunk    = (msg_1 & 0x800) >> 11;
        int group          = (msg_1 & 0x7FF);
        int is_agency_call = isAgencyCallGroup(group, state);
        int is_fleet_call  = isFleetCallGroup(group, state);

        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " Voice Group Channel Assignment ::");
        if (is_digital == 0) fprintf (stderr, " Analog");
        else                 fprintf (stderr, " Digital");
        fprintf (stderr, " Group [%04d] LID [%05d] LCN [%02d]%s", group, lid, lcn, getLcnStatusString(lcn));
        if (is_agency_call == 1)      fprintf (stderr, " [Agency]");
        else if (is_agency_call == 1) fprintf (stderr, " [Fleet]");
        if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
        if (is_emergency == 1)
        {
          fprintf (stderr, "%s", KRED);
          fprintf (stderr, " [EMERGENCY]");
        }
        fprintf (stderr, "%s", KNRM);

        //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        //Call info for state
        if (lcn != 0){state->edacs_vc_lcn = lcn;}
                      state->lasttg = group;
                      state->lastsrc = lid;

        //Call type for state
                                state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_GROUP;
        if (is_digital == 1)    state->edacs_vc_call_type |= EDACS_IS_DIGITAL;
        if (is_emergency == 1)  state->edacs_vc_call_type |= EDACS_IS_EMERGENCY;
        if (is_agency_call)     state->edacs_vc_call_type |= EDACS_IS_AGENCY_CALL;
        else if (is_fleet_call) state->edacs_vc_call_type |= EDACS_IS_FLEET_CALL;

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        //Get group mode for calls that are in the allow/whitelist
        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == group)
          {
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }
        //TG hold on EDACS Standard/Net -- block non-matching target, allow matching group
        if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");

        //NOTE: Restructured below so that analog and digital are handled the same, just that when
        //its analog, it will now start edacs_analog which will while loop analog samples until
        //signal level drops (RMS, or a dotting sequence is detected)

        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %04d SRC %05d.wav", datestr, timestr, state->edacs_site_id, group, lid);
              if (is_digital == 0) openWavOutFile48k (opts, state); //analog at 48k
              else                 openWavOutFile (opts, state); //digital
            }

            if (opts->use_rigctl == 1)
            {
              //only set bandwidth IF we have an original one to fall back to (experimental, but requires user to set the -B 12000 or -B 24000 value manually)
              if (opts->setmod_bw != 0)
              {
                if (is_digital == 0) SetModulation(opts->rigctl_sockfd, 7000); //narrower bandwidth, but has issues with dotting sequence
                else                 SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              }

              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0) edacs_analog(opts, state, group, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (is_digital == 0) edacs_analog(opts, state, group, lcn);
              #endif
            }
          }
        }
      }
      //Data Call Channel Assignment (6.2.4.3)
      else if (mt_a == 0x5)
      {
        int is_individual_call = (msg_1 & 0x1000000) >> 24;
        int is_from_lid        = (msg_1 & 0x800000) >> 23;
        int port               = ((msg_1 & 0x700000) >> 17) | ((msg_2 & 0x700000) >> 20);
        int lcn                = (msg_1 & 0xF8000) >> 15;
        int is_individual_id   = (msg_1 & 0x4000) >> 14;
        int lid                = (msg_1 & 0x3FFF);
        int group              = (msg_1 & 0x7FF);

        //Abstract away to a target, and be sure to check whether it's an individual call later
        int target = (is_individual_id == 0) ? group : lid;

        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Data Call Channel Assignment :: Type");
        if (is_individual_call == 1) fprintf (stderr, " [Individual]");
        else                         fprintf (stderr, " [Group]");
        if (is_individual_id == 1) fprintf (stderr, " LID [%05d]", target);
        else                       fprintf (stderr, " Group [%04d]", target);
        if (is_from_lid == 1) fprintf (stderr, " -->");
        else                  fprintf (stderr, " <--");
        fprintf (stderr, " Port [%02d] LCN [%02d]%s", port, lcn, getLcnStatusString(lcn));
        fprintf (stderr, "%s", KNRM);

        //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
        if (lcn > state->edacs_lcn_count && lcn < 26)
        {
          state->edacs_lcn_count = lcn;
        }

        //Call info for state
        if (lcn != 0){state->edacs_vc_lcn = lcn;}
                      state->lasttg = target;
                      state->lastsrc = 0;

        //Call type for state
        if (is_individual_call == 0) state->edacs_vc_call_type = EDACS_IS_GROUP;
        else                         state->edacs_vc_call_type = EDACS_IS_INDIVIDUAL;
      }
      //Login Acknowledge (6.2.4.4)
      else if (mt_a == 0x6)
      {
        int group = (msg_1 & 0x1FFC000) >> 14;
        int lid   = (msg_1 & 0x3FFF);

        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Login Acknowledgement :: Group [%04d] LID [%05d]", group, lid);
        fprintf (stderr, "%s", KNRM);
      }
      //Use MT-B
      else if (mt_a == 0x7)
      {
        //Status Request / Message Acknowledge (6.2.4.5)
        if (mt_b == 0x0)
        {
          int status = (msg_1 & 0x3FC000) >> 14;
          int lid    = (msg_1 & 0x3FFF);

          fprintf (stderr, "%s", KBLU);
          if (status == 248) fprintf (stderr, " Status Request :: LID [%05d]", lid);
          else               fprintf (stderr, " Message Acknowledgement :: Status [%03d] LID [%05d]", status, lid);
          fprintf (stderr, "%s", KNRM);
        }
        //Interconnect Channel Assignment (6.2.4.6)
        else if (mt_b == 0x1)
        {
          int mt_c             = (msg_1 & 0x300000) >> 20;
          int lcn              = (msg_1 & 0xF8000) >> 15;
          int is_individual_id = (msg_1 & 0x4000) >> 14;
          int lid              = (msg_1 & 0x3FFF);
          int group            = (msg_1 & 0x7FF);

          //Abstract away to a target, and be sure to check whether it's an individual call later
          int target = (is_individual_id == 0) ? group : lid;

          //Technically only MT-C 0x2 is defined in TSB 69.3 - using and extrapolating on legacy code
          int is_digital = (mt_c == 2 || mt_c == 3) ? 1 : 0;

          fprintf (stderr, "%s", KMAG);
          fprintf (stderr, " Interconnect Channel Assignment :: Type");
          if (is_digital == 0) fprintf (stderr, " Analog");
          else                 fprintf (stderr, " Digital");
          if (is_individual_id == 1) fprintf (stderr, " LID [%05d]", target);
          else                       fprintf (stderr, " Group [%04d]", target);
          fprintf (stderr, " LCN [%02d]%s", lcn, getLcnStatusString(lcn));
          fprintf (stderr, "%s", KNRM);

          //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
          if (lcn > state->edacs_lcn_count && lcn < 26)
          {
            state->edacs_lcn_count = lcn;
          }

          //Call info for state
          if (lcn != 0){state->edacs_vc_lcn = lcn;}
                        state->lasttg = 0;
                        state->lastsrc = target;

          //Call type for state
                               state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_INTERCONNECT;
          if (is_digital == 1) state->edacs_vc_call_type |= EDACS_IS_DIGITAL;
        }
        //Channel Updates (6.2.4.7)
        //Source/caller being present in individual call channel updates reverse engineered from Montreal STM system
        //Test calls reverse engineered from HartLink system
        else if (mt_b == 0x3)
        {
          int mt_c           = (msg_1 & 0x300000) >> 20;
          int lcn            = (msg_1 & 0xF8000) >> 15;
          int is_individual  = (msg_1 & 0x4000) >> 14;
          int is_emergency   = (is_individual == 0) ? (msg_1 & 0x2000) >> 13 : 0;
          int group          = (msg_1 & 0x7FF);
          int lid            = (msg_1 & 0x3FFF);
          int source         = (msg_2 & 0x3FFF); //Source only present in individual calls
          int is_agency_call = is_individual == 0 && isAgencyCallGroup(group, state);
          int is_fleet_call  = is_individual == 0 && isFleetCallGroup(group, state);

          //Abstract away to a target, and be sure to check whether it's an individual call later
          int target = (is_individual == 0) ? group : lid;

          //Test calls are just individual calls with source and target of 0
          int is_test_call = (target == 0 && source == 0);

          //Technically only MT-C 0x1 and 0x3 are defined in TSB 69.3 - using and extrapolating on legacy code
          int is_tx_trunk = (mt_c == 2 || mt_c == 3) ? 1 : 0;
          int is_digital = (mt_c == 1 || mt_c == 3) ? 1 : 0;

          if (is_individual == 0)
          {
            fprintf (stderr, "%s", KGRN);
            fprintf (stderr, " Voice Group Channel Update ::");
          }
          else if (is_test_call == 0)
          {
            fprintf (stderr, "%s", KCYN);
            fprintf (stderr, " Voice Individual Channel Update ::");
          }
          else
          {
            fprintf (stderr, "%s", KMAG);
            fprintf (stderr, " Voice Test Channel Update ::");
          }
          if (is_digital == 0) fprintf (stderr, " Analog");
          else                 fprintf (stderr, " Digital");
          if (is_individual == 0)     fprintf (stderr, " Group [%04d]", target);
          else if (is_test_call == 0) fprintf (stderr, " Callee [%05d] Caller [%05d]", target, source);
          fprintf (stderr, " LCN [%02d]%s", lcn, getLcnStatusString(lcn));
          if (is_agency_call == 1)     fprintf (stderr, " [Agency]");
          else if (is_fleet_call == 1) fprintf (stderr, " [Fleet]");
          if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
          if (is_emergency == 1)
          {
            fprintf (stderr, "%s", KRED);
            fprintf (stderr, " [EMERGENCY]");
          }
          fprintf (stderr, "%s", KNRM);

          //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
          if (lcn > state->edacs_lcn_count && lcn < 26)
          {
            state->edacs_lcn_count = lcn;
          }

          //Call info for state
          if (lcn != 0){state->edacs_vc_lcn = lcn;}
                        state->lasttg = target;
                        //Alas, EDACS standard does not provide a source LID on channel updates - try to work around this on the display end instead
                        state->lastsrc = 0;

          //Call type for state
                                      state->edacs_vc_call_type  = EDACS_IS_VOICE;
          if (is_individual == 0)     state->edacs_vc_call_type |= EDACS_IS_GROUP;
          else if (is_test_call == 0) state->edacs_vc_call_type |= EDACS_IS_INDIVIDUAL;
          else                        state->edacs_vc_call_type |= EDACS_IS_TEST_CALL;
          if (is_digital == 1)        state->edacs_vc_call_type |= EDACS_IS_DIGITAL;
          if (is_emergency == 1)      state->edacs_vc_call_type |= EDACS_IS_EMERGENCY;
          if (is_agency_call)         state->edacs_vc_call_type |= EDACS_IS_AGENCY_CALL;
          else if (is_fleet_call)     state->edacs_vc_call_type |= EDACS_IS_FLEET_CALL;

          char mode[8]; //allow, block, digital enc
          sprintf (mode, "%s", "");

          //if we are using allow/whitelist mode, then write 'B' to mode for block
          //comparison below will look for an 'A' to write to mode if it is allowed
          if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

          //Individual calls always remain blocked if in allow/whitelist mode
          if (is_individual == 0)
          {
            //Get group mode for calls that are in the allow/whitelist
            for (int i = 0; i < state->group_tally; i++)
            {
              if (state->group_array[i].groupNumber == target)
              {
                strcpy (mode, state->group_array[i].groupMode);
                break;
              }
            }
            //moved to below, we want the TG HOLD to override either group or individual calls
            //
            //
          }

          //TG hold on EDACS STD/NET -- block non-matching abstract target (moved here to fix tuning that occurs on I-CALL during TG HOLD)
          if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
          if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");

          //NOTE: Restructured below so that analog and digital are handled the same, just that when
          //its analog, it will now start edacs_analog which will while loop analog samples until
          //signal level drops (RMS, or a dotting sequence is detected)

          //this is working now with the new import setup
          if (((is_individual == 0 && opts->trunk_tune_group_calls == 1) || (is_individual == 1 && opts->trunk_tune_private_calls == 1)) &&
              opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
          {
            if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
            {
              //openwav file and do per call right here
              if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
              {
                if (is_individual == 0) sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %04d SRC %05d.wav", datestr, timestr, state->edacs_site_id, target, state->lastsrc);
                else                    sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %05d SRC %05d I-Call.wav", datestr, timestr, state->edacs_site_id, target, state->lastsrc);
                if (is_digital == 0) openWavOutFile48k (opts, state); //analog at 48k
                else                 openWavOutFile (opts, state); //digital
              }

              if (opts->use_rigctl == 1)
              {
                //only set bandwidth IF we have an original one to fall back to (experimental, but requires user to set the -B 12000 or -B 24000 value manually)
                if (opts->setmod_bw != 0)
                {
                  if (is_digital == 0) SetModulation(opts->rigctl_sockfd, 7000); //narrower bandwidth, but has issues with dotting sequence
                  else                 SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                }

                SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1;
                if (is_digital == 0) edacs_analog(opts, state, target, lcn);
              }

              if (opts->audio_in_type == 3) //rtl dongle
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1;
                if (is_digital == 0) edacs_analog(opts, state, target, lcn);
                #endif
              }
            }
          }
        }
        //System Assigned ID (6.2.4.8)
        else if (mt_b == 0x4)
        {
          int sgid  = (msg_1 & 0x3FF800) >> 11;
          int group = (msg_1 & 0x7FF);

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " System Assigned ID :: SGID [%04d] Group [%04d]", sgid, group);
          fprintf (stderr, "%s", KNRM);
        }
        //Individual Call Channel Assignment (6.2.4.9)
        //Analog and digital flag reverse engineered from Montreal STM system
        //Test calls reverse engineered from HartLink system
        else if (mt_b == 0x5)
        {
          int is_tx_trunk = (msg_1 & 0x200000) >> 21;
          int lcn         = (msg_1 & 0xF8000) >> 15;
          int is_digital  = (msg_1 & 0x4000) >> 14;
          int target      = (msg_1 & 0x3FFF);
          int source      = (msg_2 & 0x3FFF);

          if (target == 0 && source == 0)
          {
            fprintf (stderr, "%s", KMAG);
            fprintf (stderr, " Test Call Channel Assignment ::");
            fprintf (stderr, " LCN [%02d]%s", lcn, getLcnStatusString(lcn));

            state->edacs_vc_lcn = lcn;
            //assign bogus values so that this will show up in ncurses terminal and overwrite current values in the matrix
            state->lasttg  = 999999999;
            state->lastsrc = 999999999;
            lcn = 0; //set to zero here, because this is not an actual call, so don't tune to it
          }
          else {
            fprintf (stderr, "%s", KCYN);
            fprintf (stderr, " Voice Individual Channel Assignment ::");
            if (is_digital == 0) fprintf (stderr, " Analog");
            else                 fprintf (stderr, " Digital");
            fprintf (stderr, " Callee [%05d] Caller [%05d] LCN [%02d]%s", target, source, lcn, getLcnStatusString(lcn));
            if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
          }
          fprintf (stderr, "%s", KNRM);

          //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
          if (lcn > state->edacs_lcn_count && lcn < 26)
          {
            state->edacs_lcn_count = lcn;
          }

          //Call info for state
          if (lcn != 0){state->edacs_vc_lcn = lcn;}
                        state->lasttg = target;
                        state->lastsrc = source;

          //Call type for state
          if (target == 0 && source == 0) state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_TEST_CALL;
          else                            state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_INDIVIDUAL;
          if (is_digital == 1)            state->edacs_vc_call_type |= EDACS_IS_DIGITAL;

          char mode[8]; //allow, block, digital enc
          sprintf (mode, "%s", "");

          //if we are using allow/whitelist mode, then write 'B' to mode for block
          //Individual calls always remain blocked if in allow/whitelist mode
          if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

          //NOTE: Restructured below so that analog and digital are handled the same, just that when
          //its analog, it will now start edacs_analog which will while loop analog samples until
          //signal level drops (RMS, or a dotting sequence is detected)

          //this is working now with the new import setup
          if ((opts->trunk_tune_private_calls == 1) && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
          {
            if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
            {
              //openwav file and do per call right here
              if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
              {
                sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %05d SRC %05d I-Call.wav", datestr, timestr, state->edacs_site_id, target, state->lastsrc);
                if (is_digital == 0) openWavOutFile48k (opts, state); //analog at 48k
                else                 openWavOutFile (opts, state); //digital
              }

              if (opts->use_rigctl == 1)
              {
                //only set bandwidth IF we have an original one to fall back to (experimental, but requires user to set the -B 12000 or -B 24000 value manually)
                if (opts->setmod_bw != 0)
                {
                  if (is_digital == 0) SetModulation(opts->rigctl_sockfd, 7000); //narrower bandwidth, but has issues with dotting sequence
                  else                 SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                }

                SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1;
                if (is_digital == 0) edacs_analog(opts, state, target, lcn);
              }

              if (opts->audio_in_type == 3) //rtl dongle
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
                state->edacs_tuned_lcn = lcn;
                opts->p25_is_tuned = 1;
                if (is_digital == 0) edacs_analog(opts, state, target, lcn);
                #endif
              }
            }
          }
        }
        //Console Unkey / Drop (6.2.4.10)
        else if (mt_b == 0x6)
        {
          int is_drop = (msg_1 & 0x80000) >> 19;
          int lcn     = (msg_1 & 0x7C000) >> 14;
          int lid     = (msg_1 & 0x3FFF);

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Console ");
          if (is_drop == 0) fprintf (stderr, " Unkey");
          else              fprintf (stderr, " Drop");
          fprintf (stderr, " :: LID [%05d] LCN [%02d]%s", lid, lcn, getLcnStatusString(lcn));
          fprintf (stderr, "%s", KNRM);
        }
        //Use MT-D
        else if (mt_b == 0x7)
        {
          //Cancel Dynamic Regroup (6.2.4.11)
          if (mt_d == 0x00)
          {
            int knob = (msg_1 & 0x1C000) >> 14;
            int lid  = (msg_1 & 0x3FFF);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Cancel Dynamic Regroup :: LID [%05d] Knob position [%1d]", lid, knob + 1);
            fprintf (stderr, "%s", KNRM);
          }
          //Adjacent Site Control Channel (6.2.4.12)
          else if (mt_d == 0x01)
          {
            int adj_cc_lcn     = (msg_1 & 0x1F000) >> 12;
            int adj_site_index = (msg_1 & 0xE00) >> 9;
            int adj_site_id    = (msg_1 & 0x1F0) >> 4;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Adjacent Site Control Channel :: Site ID [%02X][%03d] Index [%1d] LCN [%02d]%s", adj_site_id, adj_site_id, adj_site_index, adj_cc_lcn, getLcnStatusString(adj_cc_lcn));
            if (adj_site_id == 0 && adj_site_index == 0)      fprintf (stderr, " [Adjacency Table Reset]");
            else if (adj_site_id != 0 && adj_site_index == 0) fprintf (stderr, " [Priority System Definition]");
            else if (adj_site_id == 0 && adj_site_index != 0) fprintf (stderr, " [Adjacencies Table Length Definition]");
            else                                              fprintf (stderr, " [Adjacent System Definition]");
            fprintf (stderr, "%s", KNRM);
          }
          //Extended Site Options (6.2.4.13)
          else if (mt_d == 0x02)
          {
            int msg_num = (msg_1 & 0xE000) >> 13;
            int data    = (msg_1 & 0x1FFF);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Extended Site Options :: Message Num [%1d] Data [%04X]", msg_num, data);
            fprintf (stderr, "%s", KNRM);
          }
          //System Dynamic Regroup Plan Bitmap (6.2.4.14)
          else if (mt_d == 0x04)
          {
            int bank     = (msg_1 & 0x10000) >> 16;
            int resident = (msg_1 & 0xFF00) >> 8;
            int active   = (msg_1 & 0xFF);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " System Dynamic Regroup Plan Bitmap");

            //this message gets EXTREMELY LONG, so lets hide it behind the payload verbosity
            if (opts->payload == 1)
            {
              fprintf (stderr, " :: Plan Bank [%1d] Resident [", bank);
              int plan = bank * 8;
              int first = 1;
              while (resident != 0) {
                if ((resident & 0x1) == 1) {
                  if (first == 1)
                  {
                    first = 0;
                    fprintf (stderr, "%d", plan);
                  }
                  else
                  {
                    fprintf (stderr, ", %d", plan);
                  }
                }
                resident >>= 1;
                plan++;
              }

              fprintf (stderr, "] Active [");

              plan = bank * 8;
              first = 1;
              while (active != 0) {
                if ((active & 0x1) == 1) {
                  if (first == 1)
                  {
                    first = 0;
                    fprintf (stderr, "%d", plan);
                  }
                  else
                  {
                    fprintf (stderr, ", %d", plan);
                  }
                }
                active >>= 1;
                plan++;
              }

              fprintf (stderr, "]");
            } //end payload

            fprintf (stderr, "%s", KNRM);
          }
          //Assignment to Auxiliary Control Channel (6.2.4.15)
          else if (mt_d == 0x05)
          {
            int aux_cc_lcn = (msg_1 & 0x1F000) >> 12;
            int group      = (msg_1 & 0x7FF);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Assignment to Auxiliary CC :: Group [%04d] Aux CC LCN [%02d]%s", group, aux_cc_lcn, getLcnStatusString(aux_cc_lcn));
            fprintf (stderr, "%s", KNRM);
          }
          //Initiate Test Call Command (6.2.4.16)
          else if (mt_d == 0x06)
          {
            int cc_lcn = (msg_1 & 0x1F000) >> 12;
            int wc_lcn = (msg_1 & 0xF80) >> 7;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Initiate Test Call Command :: CC LCN [%02d] WC LCN [%02d]", cc_lcn, wc_lcn);
            fprintf (stderr, "%s", KNRM);
          }
          //Unit Enable / Disable (6.2.4.17)
          else if (mt_d == 0x07)
          {
            int qualifier = (msg_1 & 0xC000) >> 14;
            int target    = (msg_1 & 0x3FFF);

            fprintf (stderr, "%s", KBLU);
            fprintf (stderr, " Unit Enable/Disable ::");
            if (qualifier == 0x0)      fprintf (stderr, " [Temporary Disable]");
            else if (qualifier == 0x1) fprintf (stderr, " [Corrupt Personality]");
            else if (qualifier == 0x2) fprintf (stderr, " [Revoke Logical ID]");
            else                       fprintf (stderr, " [Re-enable Unit]");
            fprintf (stderr, " LID [%05d]", target);
            fprintf (stderr, "%s", KNRM);
          }
          //Site ID (6.2.4.18)
          else if (mt_d == 0x08 || mt_d == 0x09 || mt_d == 0x0A || mt_d == 0x0B)
          {
            int cc_lcn       = (msg_1 & 0x1F000) >> 12;
            int priority     = (msg_1 & 0xE00) >> 9;
            int is_scat      = (msg_1 & 0x80) >> 7;
            int is_failsoft  = (msg_1 & 0x40) >> 6;
            int is_auxiliary = (msg_1 & 0x20) >> 5;
            int site_id      = (msg_1 & 0x1F);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Standard/Networked :: Site ID [%02X][%03d] Priority [%1d] CC LCN [%02d]%s", site_id, site_id, priority, cc_lcn, getLcnStatusString(cc_lcn));
            if (is_failsoft == 1)
            {
              fprintf (stderr, "%s", KRED);
              fprintf (stderr, " [FAILSOFT]");
              fprintf (stderr, "%s", KYEL);
            }
            if (is_scat == 1)      fprintf (stderr, " [SCAT]");
            if (is_auxiliary == 1) fprintf (stderr, " [Auxiliary]");
            fprintf (stderr, "%s", KNRM);

            //Store our site ID
            state->edacs_site_id = site_id;

            //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
            if (state->edacs_cc_lcn > state->edacs_lcn_count && cc_lcn < 26)
            {
              state->edacs_lcn_count = state->edacs_cc_lcn;
            }

            //If this is only an auxiliary CC, keep searching for the primary CC
            if (is_auxiliary == 0)
            {
              //Store our CC LCN
              state->edacs_cc_lcn = cc_lcn;

              //Check for control channel LCN frequency if not provided in channel map or in the LCN list
              if (state->trunk_lcn_freq[state->edacs_cc_lcn - 1] == 0)
              {
                //If using rigctl, we can ask for the currrent frequency
                if (opts->use_rigctl == 1)
                {
                  long int lcnfreq = GetCurrentFreq (opts->rigctl_sockfd);
                  if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn - 1] = lcnfreq;
                }

                //If using rtl input, we can ask for the current frequency tuned
                if (opts->audio_in_type == 3)
                {
                  long int lcnfreq = (long int)opts->rtlsdr_center_freq;
                  if (lcnfreq != 0) state->trunk_lcn_freq[state->edacs_cc_lcn - 1] = lcnfreq;
                }
              }

              //Set trunking CC here so we know where to come back to
              if (opts->p25_trunk == 1 && state->trunk_lcn_freq[state->edacs_cc_lcn - 1] != 0)
              {
                //Index starts at zero, LCNs locally here start at 1
                state->p25_cc_freq = state->trunk_lcn_freq[state->edacs_cc_lcn - 1];
              }
            }
          }
          //System All-Call (6.2.4.19)
          //Analog and digital flag extrapolated from reverse engineering of other messages
          else if (mt_d == 0x0F)
          {
            int lcn         = (msg_1 & 0x1F000) >> 12;
            int is_digital  = (msg_1 & 0x800) >> 11;
            int is_update   = (msg_1 & 0x400) >> 10;
            int is_tx_trunk = (msg_1 & 0x200) >> 9;
            int lid         = (msg_1 & 0x7F) | ((msg_2 & 0xFE) << 6);

            fprintf (stderr, "%s", KMAG);
            fprintf (stderr, " System All-Call Channel");
            if (is_update == 0) fprintf (stderr, " Assignment");
            else                fprintf (stderr, " Update");
            fprintf (stderr, " ::");
            if (is_digital == 0) fprintf (stderr, " Analog");
            else                 fprintf (stderr, " Digital");
            fprintf (stderr, " LID [%05d] LCN [%02d]%s", lid, lcn, getLcnStatusString(lcn));
            if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
            fprintf (stderr, "%s", KNRM);

            //LCNs >= 26 are reserved to indicate status (queued, busy, denied, etc)
            if (lcn > state->edacs_lcn_count && lcn < 26)
            {
              state->edacs_lcn_count = lcn;
            }

            //Call info for state
            if (lcn != 0){state->edacs_vc_lcn = lcn;}
                          state->lasttg = 0;
                          state->lastsrc = lid;

            //Call type for state
                                 state->edacs_vc_call_type  = EDACS_IS_VOICE | EDACS_IS_ALL_CALL;
            if (is_digital == 1) state->edacs_vc_call_type |= EDACS_IS_DIGITAL;

            char mode[8]; //allow, block, digital enc
            sprintf (mode, "%s", "");

            //if we are using allow/whitelist mode, then write 'A' to mode for allow - always allow all-calls by default
            if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "A");

            //NOTE: Restructured below so that analog and digital are handled the same, just that when
            //its analog, it will now start edacs_analog which will while loop analog samples until
            //signal level drops (RMS, or a dotting sequence is detected)

            //this is working now with the new import setup
            if ((opts->trunk_tune_group_calls == 1) && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
            {
              if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
              {
                //openwav file and do per call right here
                if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
                {
                  sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld SRC %05d All-Call.wav", datestr, timestr, state->edacs_site_id, state->lastsrc);
                  if (is_digital == 0) openWavOutFile48k (opts, state); //analog at 48k
                  else                 openWavOutFile (opts, state); //digital
                }

                if (opts->use_rigctl == 1)
                {
                  //only set bandwidth IF we have an original one to fall back to (experimental, but requires user to set the -B 12000 or -B 24000 value manually)
                  if (opts->setmod_bw != 0)
                  {
                    if (is_digital == 0) SetModulation(opts->rigctl_sockfd, 7000); //narrower bandwidth, but has issues with dotting sequence
                    else                 SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                  }

                  SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
                  state->edacs_tuned_lcn = lcn;
                  opts->p25_is_tuned = 1;
                  if (is_digital == 0) edacs_analog(opts, state, 0, lcn);
                }

                if (opts->audio_in_type == 3) //rtl dongle
                {
                  #ifdef USE_RTLSDR
                  rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
                  state->edacs_tuned_lcn = lcn;
                  opts->p25_is_tuned = 1;
                  if (is_digital == 0) edacs_analog(opts, state, 0, lcn);
                  #endif
                }
              }
            }
          }
          //Dynamic Regrouping (6.2.4.20)
          else if (mt_d == 0x10)
          {
            int fleet_bits = (msg_1 & 0x1C000) >> 14;
            int lid        = (msg_1 & 0x3FFF);
            int plan       = (msg_2 & 0x1E0000) >> 17;
            int type       = (msg_2 & 0x18000) >> 15;
            int knob       = (msg_2 & 0x7000) >> 12;
            int group      = (msg_2 & 0x7FF);

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Dynamic Regrouping :: Plan [%02d] Knob position [%1d] LID [%05d] Group [%04d] Fleet bits [%1d]", plan, knob + 1, lid, group, fleet_bits);
            if (type == 0)      fprintf (stderr, " [Forced select, no deselect]");
            else if (type == 1) fprintf (stderr, " [Forced select, optional deselect]");
            else if (type == 2) fprintf (stderr, " [Reserved]");
            else                fprintf (stderr, " [Optional select]");
            fprintf (stderr, "%s", KNRM);
          }
          //Reserved command (MT-D)
          else
          {
            fprintf (stderr, "%s", KWHT);
            fprintf (stderr, " Reserved Command (MT-D)");
            fprintf (stderr, "%s", KNRM);
            // Only print the payload if we haven't already printed it
            if (opts->payload != 1)
            {
              fprintf (stderr, " ::");
              fprintf (stderr, " MSG_1 [%07llX]", msg_1);
              fprintf (stderr, " MSG_2 [%07llX]", msg_2);
            }
          }
        }
        //Reserved command (MT-B)
        else
        {
          fprintf (stderr, "%s", KWHT);
          fprintf (stderr, " Reserved Command (MT-B)");
          fprintf (stderr, "%s", KNRM);
          // Only print the payload if we haven't already printed it
          if (opts->payload != 1)
          {
            fprintf (stderr, " ::");
            fprintf (stderr, " MSG_1 [%07llX]", msg_1);
            fprintf (stderr, " MSG_2 [%07llX]", msg_2);
          }
        }
      }
      //Reserved command (MT-A)
      else
      {
        fprintf (stderr, "%s", KWHT);
        fprintf (stderr, " Reserved Command (MT-A)");
        fprintf (stderr, "%s", KNRM);
        // Only print the payload if we haven't already printed it
        if (opts->payload != 1)
        {
          fprintf (stderr, " ::");
          fprintf (stderr, " MSG_1 [%07llX]", msg_1);
          fprintf (stderr, " MSG_2 [%07llX]", msg_2);
        }
      }

    } //end Standard or Networked

    //let users know they need to select an operational mode with the switches below
    else
    {
      fprintf (stderr, " Detected EDACS: Use -fh, -fH, -fe, or -fE for std, esk, ea, or ea-esk to specify the type");
      fprintf (stderr, "\n");
      fprintf (stderr, " MSG_1 [%07llX]", msg_1);
      fprintf (stderr, " MSG_2 [%07llX]", msg_2);
    }

  }


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
  
  fprintf (stderr, "\n");

}

void eot_cc(dsd_opts * opts, dsd_state * state)
{

  fprintf (stderr, "EOT; \n");

  //set here so that when returning to the CC, it doesn't go into an immediate hunt if not immediately acquired
  state->last_cc_sync_time = time(NULL);
  state->last_vc_sync_time = time(NULL);

  //jump back to CC here
  if (opts->p25_trunk == 1 && state->p25_cc_freq != 0 && opts->p25_is_tuned == 1)
  {

    //rigctl
    if (opts->use_rigctl == 1)
    {
      state->lasttg = 0;
      state->lastsrc = 0;
      state->payload_algid = 0;
      state->payload_keyid = 0;
      state->payload_miP = 0;
      //reset some strings
      sprintf (state->call_string[0], "%s", "                     "); //21 spaces
      sprintf (state->call_string[1], "%s", "                     "); //21 spaces
      sprintf (state->active_channel[0], "%s", "");
      sprintf (state->active_channel[1], "%s", "");
      opts->p25_is_tuned = 0;
      state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
      if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
      SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
    }

    //rtl
    else if (opts->audio_in_type == 3)
    {
      #ifdef USE_RTLSDR
      state->lasttg = 0;
      state->lastsrc = 0;
      state->payload_algid = 0;
      state->payload_keyid = 0;
      state->payload_miP = 0;
      //reset some strings
      sprintf (state->call_string[0], "%s", "                     "); //21 spaces
      sprintf (state->call_string[1], "%s", "                     "); //21 spaces
      sprintf (state->active_channel[0], "%s", "");
      sprintf (state->active_channel[1], "%s", "");
      opts->p25_is_tuned = 0;
      state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;
      rtl_dev_tune (opts, state->p25_cc_freq);
      #endif
    }

  }
}


