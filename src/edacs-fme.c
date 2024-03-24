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
 * 2024-03 added EA i-call and login message decoding
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

char * getDateE(void) {
  #ifdef AERO_BUILD
  char datename[80];
  #else
  char datename[99];
  #endif
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y%m%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

//fix from YorgosTheodorakis fork -- https://github.com/YorgosTheodorakis/dsd-fme/commit/7884ee555521a887d388152b3b1f11f20433a94b
char * getTimeE(void) //get pretty hhmmss timestamp
{
  char * curr = (char *) malloc(9);
  time_t t = time(NULL);
  struct tm * ptm = localtime(& t);
  sprintf(
    curr,
    "%02d%02d%02d",
    ptm->tm_hour,
    ptm->tm_min,
    ptm->tm_sec
  );
  return curr;
}

char* get_lcn_status_string(int lcn)
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

//generic rms type function
long int gen_rms(short *samples, int len, int step)
{
  
  int i;
  long int rms;
  long p, t, s;
  double dc, err;

  p = t = 0L;
  for (i=0; i<len; i+=step) {
    s = (long)samples[i];
    t += s;
    p += s * s;
  }
  /* correct for dc offset in squares */
  dc = (double)(t*step) / (double)len;
  err = t * 2 * dc - dc * dc * len;

  rms = (long int)sqrt((p-err) / len);
  //make sure it doesnt' randomly feed us a large negative value (overflow?)
  if (rms < 0) rms = 150; //could also consider returning 0 and making it return the last known good value instead
  return rms;
}

//super generic pre-emphasis filter with alpha value of 1 for short sample input / output
static short pstate = 1;
void analog_preemph_filter(short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
  {
    //debug
    // fprintf (stderr, " I:%d;", input[i]);

    input[i] = input[i] - 1 * pstate;
    pstate = input[i];

    //debug
    // fprintf (stderr, " O:%d;;", input[i]);

    //increase gain value slightly
    input[i] *= 1.5;

    //debug
    // fprintf (stderr, " O2:%d;;;", input[i]);
  }
}

//super generic short sample clipping filter, prevent short values from exceeding tolerable clip value
void analog_clipping_filter(short * input, int len)
{
  int i;
  for (i = 0; i < len; i++)
  {
    if (input[i] > 32760)
      input[i] = 32760;
    else if (input[i] < -32760)
      input[i] = -32760;
  }
}

//modified from version found in rtl_fm
static short avg = 0;
void analog_deemph_filter(short * input, int len)
{
  // static short avg;  // cheating...
  int i, d;

  // int a = (int)round(1.0/((1.0-exp(-1.0/(96000 * 75e-6))))); //48000 is rate out, but doubling it sounds 'beefier'
  int a = 8; //result of calc above is 8, so save a cycle on the low end CPUs

  // de-emph IIR
  // avg = avg * (1 - alpha) + sample * alpha;

  for (i = 0; i < len; i++)
  {
    d = input[i] - avg;
    if (d > 0)
      avg += (d + a/2) / a;
    else
      avg += (d - a/2) / a;

    input[i] = (short)avg;
  }
}

//modified from version found in rtl_fm
static int dc_avg = 0;
void analog_dc_block_filter(short * input, int len)
{
  int i, avg;
  int64_t sum = 0;
  for (i=0; i < len; i++)
    sum += input[i];

  avg = sum / len;
  avg = (avg + dc_avg * 9) / 10;
  for (i=0; i < len; i++)
    input[i] -= (short)avg; 

  dc_avg = avg;
}

//listening to and playing back analog audio
void edacs_analog(dsd_opts * opts, dsd_state * state, int afs, unsigned char lcn)
{
  //reset static states for new channel
  pstate = 1;
  avg = 0;
  dc_avg = 0;

  int i, result;
  int count = 5; //RMS has a 5 count (5 * 180ms) now before cutting off;
  short analog1[960];
  short analog2[960];
  short analog3[960];
  short sample = 0; 

  state->last_cc_sync_time = time(NULL);
  state->last_vc_sync_time = time(NULL);

  memset (analog1, 0, sizeof(analog1));
  memset (analog2, 0, sizeof(analog2));
  memset (analog3, 0, sizeof(analog3));

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
      rms = gen_rms(analog3, 960, 1);
    }

    //NOTE: The core dumps observed previously were due to SDR++ Remote Server connection dropping due to Internet/Other issues
    //and unlike in the main livescanner loop where it just hangs, this loop will cause a core dump. The observed issue 
    //has not occurred when using SDR++ on local hardware, just the remote server software over the Internet.

    //NOTE: The fix below does not apply to above observed issue, as the TCP connection will not drop, there will just
    //not be a sample to read in and it hangs on sf_short read until it crashes out, the fix below will prevent issues
    //when SDR++ is closed locally, or the TCP connection closes suddenly.

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
      rms = gen_rms(analog3, 960, 1);
    }

    //RTL Input
    #ifdef USE_RTLSDR
    if (opts->audio_in_type == 3)
    {
      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
        analog1[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
        analog2[i] = sample;
      }

      for (i = 0; i < 960; i++)
      {
        get_rtlsdr_sample(&sample, opts, state);
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

    //test running analog audio through a de-emphasis filter
    analog_deemph_filter(analog1, 960);
    analog_deemph_filter(analog2, 960);
    analog_deemph_filter(analog3, 960);
    //and dc_block filter analog_dc_block_filter
    analog_dc_block_filter(analog1, 960);
    analog_dc_block_filter(analog2, 960);
    analog_dc_block_filter(analog3, 960);
    //test running analog audio through a pre-emphasis filter
    analog_preemph_filter(analog1, 960);
    analog_preemph_filter(analog2, 960);
    analog_preemph_filter(analog3, 960);
    //test running analog short sample clipping filter
    analog_clipping_filter(analog1, 960);
    analog_clipping_filter(analog2, 960);
    analog_clipping_filter(analog3, 960);

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
      fprintf (stderr, " AFS [%03d] [%02d-%03d] LCN [%02d]", afs, afs >> 7, afs & 0x7F, lcn);
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

    if (count > 0) fprintf (stderr, "\n");
    
  }
}

void edacs(dsd_opts * opts, dsd_state * state)
{
  //changed to ulli here for 32-bit cygwin (not sure if was an issue, but playing it safe)
  unsigned long long int fr_1 = 0xFFFFFFFFFF; //40-bit frames
  unsigned long long int fr_2 = 0; //each is a 40 bit frame that repeats 3 times
  unsigned long long int fr_3 = 0; //two messages per frame
  unsigned long long int fr_4 = 0xFFFFFFFFFF; 
  unsigned long long int fr_5 = 0;
  unsigned long long int fr_6 = 0;

  //BCH stuff
  unsigned long long int fr_1m = 0xFFFFFFF; //28-bit 7X message portion to pass to bch handler
  unsigned long long int fr_1t = 0xFFFFFFFFFF; //40 bit return from BCH with poly attached
  unsigned long long int fr_4m = 0xFFFFFFF; //28-bit 7X message portion to pass to bch handler
  unsigned long long int fr_4t = 0xFFFFFFFFFF; //40 bit return from BCH with poly attached

  //commands; may not use these anymore
  unsigned int vcmd = 0xEE; //voice command variable
  unsigned int idcmd = 0xFD;
  unsigned int peercmd = 0xF88; //using for EA detection test
  unsigned int netcmd = 0xF3; //using for Networked Test
  UNUSED2(vcmd, idcmd);
  UNUSED2(peercmd, netcmd);

  char * timestr; //add timestr here, so we can assign it and also free it to prevent memory leak
  timestr = getTimeE();

  state->edacs_vc_lcn = -1; //init on negative for ncurses and tuning

  int i;
  int edacs_bit[241] = {0}; //zero out bit array and collect bits into it.

  for (i = 0; i < 240; i++) //288 bits every transmission minus 48 bit (24 dibit) sync pattern
  {
    edacs_bit[i] = getDibit (opts, state); //getDibit returns binary 0 or 1 on GFSK signal (Edacs and PV)
  }

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

  fr_1 = fr_1 & 0xFFFFFFFFFF;
  fr_4 = fr_4 & 0xFFFFFFFFFF;

  fr_1m = (fr_1 & 0xFFFFFFF000) >> 12;
  fr_4m = (fr_4 & 0xFFFFFFF000) >> 12;

  //take our message and create a new crc for it, if it matches the old crc, then we have a good frame
  fr_1t = edacs_bch (fr_1m);
  fr_4t = edacs_bch (fr_4m);

  fr_1t = fr_1t & 0xFFFFFFFFFF;
  fr_4t = fr_4t & 0xFFFFFFFFFF;

  if (fr_1 != fr_1t || fr_4 != fr_4t)
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
    // - KMAG - voice other calls (interconnect, all-call, etc)
    // - KBLU - subscriber data
    // - KWHT - unknown/reserved

    //Account for ESK, if any
    unsigned long long int fr_esk_mask = ((unsigned long long int)state->esk_mask) << 32;
    fr_1t = fr_1t ^ fr_esk_mask;
    fr_4t = fr_4t ^ fr_esk_mask;

    //Start Extended Addressing Mode 
    if (state->ea_mode == 1)
    {
      unsigned char mt1 = (fr_1t & 0xF800000000) >> 35;
      unsigned char mt2 = (fr_1t & 0x780000000) >> 31;

      //TODO: initialize where they are actually used
      unsigned long long int site_id = 0; //we probably could just make this an int as well as the state variables
      unsigned char lcn = 0;

      //Add raw payloads and MT1/MT2 for easy debug
      if (opts->payload == 1)
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
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

        //Test Call (not seen in the wild, see US patent US7546135B2, Figure 2b)
        if (mt2 == 0x0)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Initiate Test Call");
          fprintf (stderr, "%s", KNRM);
        }
        //Adjacent Sites
        else if (mt2 == 0x1)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Adjacent Site");
          if ( ((fr_1t & 0xFF000) >> 12) > 0 )
          {
            int adj = (fr_1t & 0xFF000) >> 12;
            int adj_l = (fr_1t & 0x1F000000) >> 24;
            fprintf (stderr, " :: Site ID [%02X][%03d] on CC LCN [%02d]%s", adj, adj, adj_l, get_lcn_status_string(lcn));
          }
          fprintf (stderr, "%s", KNRM);
        }
        //Status/Message
        else if (mt2 == 0x4)
        {
          int status = (fr_1t & 0xFF000) >> 12;
          int source = (fr_4t & 0xFFFFF000) >> 12;
          fprintf (stderr, "%s", KBLU);
          if (status == 248) fprintf (stderr, " Status Request :: Target [%08d]", source);
          else               fprintf (stderr, " Message Acknowledgement :: Status [%03d] Source [%08d]", status, source);
          fprintf (stderr, "%s", KNRM);
        }
        //Control Channel LCN
        else if (mt2 == 0x8)
        {
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Control Channel");
          if (((fr_4t >> 12) & 0x1F) != 0)
          {
            state->edacs_cc_lcn = ((fr_4t >> 12) & 0x1F);
            if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) //26, or 27. shouldn't matter don't think cc lcn will give a status lcn val 
            {
              state->edacs_lcn_count = state->edacs_cc_lcn;
            }
            fprintf (stderr, " :: LCN [%d]%s", state->edacs_cc_lcn, get_lcn_status_string(lcn));

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
          site_id = ((fr_1 & 0x1F000) >> 12) | ((fr_1 & 0x1F000000) >> 19);
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Extended Addressing :: Site ID [%02llX][%03lld]", site_id, site_id);
          fprintf (stderr, "%s", KNRM);
          state->edacs_site_id = site_id;
        }
        //disabling kick command, data looks like its just FFFF, no actual values, can't verify accuracy
        // else if (mt2 == 0xB) //KICK LISTING for EA?? Unverified, but probably observed in Unitrunker way back when.
        // {
        //   int kicked = (fr_4t & 0xFFFFF000) >> 12;
        //   fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        //   fprintf (stderr, " FR_3 [%010llX]", fr_3);
        //   fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        //   fprintf (stderr, " FR_6 [%010llX]", fr_6);
        //   fprintf (stderr, " %08d REG/DEREG/AUTH?", kicked);
        // }
        //Patch Groups
        else if (mt2 == 0xC)
        {
          int patch_site = ((fr_4t & 0xFF00000000) >> 32); //is site info valid, 0 for all sites? else patch only good on site listed?
          int sourcep = ((fr_1t & 0xFFFF000) >> 12);
          int targetp = ((fr_4t & 0xFFFF000) >> 12);
          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Patch :: Site [%d] Source [%d] Target [%d] ", patch_site, sourcep, targetp);
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
            fprintf (stderr, " FR_1 [%010llX]", fr_1t);
            fprintf (stderr, " FR_4 [%010llX]", fr_4t);
          }
        }
        
      }
      //TDMA Group Grant Update (never observed, unknown if ever used on any EDACS system)
      else if (mt1 == 0x1)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;
        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Data Group Call :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //Data Group Grant Update
      else if (mt1 == 0x2)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;
        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " TDMA Group Call :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //Voice Call Grant Update
      // MT1 value determines the type of group call:
      // - 0x03 digital group voice (ProVoice, standard on SLERS EA)
      // - 0x06 analog group voice
      else if (mt1 == 0x3 || mt1 == 0x6)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital = (mt1 == 0x3) ? 1 : 0;
        int is_update = (fr_1t & 0x10000000) >> 28;
        int is_emergency = (fr_4t & 0x100000000) >> 32;
        int is_tx_trunking = (fr_4t & 0x200000000) >> 33;
        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
                         state->lasttg = group; // 0 is a valid TG, it's the all-call for agency 0
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;

        fprintf (stderr, "%s", KGRN);
        if (is_digital == 0) fprintf (stderr, " Analog Group Call");
        else                 fprintf (stderr, " Digital Group Call");
        if (is_update == 0) fprintf (stderr, " Assignment");
        else                fprintf (stderr, " Update");
        fprintf (stderr, " :: Group [%05d] Source [%08d] LCN [%02d]%s", group, source, lcn, get_lcn_status_string(lcn));

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
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %d SRC %d.wav", getDateE(), timestr, state->edacs_site_id, group, source);
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
      //I-Call Grant Update
      else if (mt1 == 0x10)
      {
        lcn = (fr_4t & 0x1F00000000) >> 32;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital = (fr_1t & 0x200000000) >> 33;
        int is_update = (fr_1t & 0x100000000) >> 32;
        int target = (fr_1t & 0xFFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        if (target != 0) state->lasttg = target + 100000; //Use IDs > 100000 to represent i-call targets to differentiate from TGs
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;

        fprintf (stderr, "%s", KCYN);
        if (is_digital == 0) fprintf (stderr, " Analog I-Call");
        else                 fprintf (stderr, " Digital I-Call");
        if (is_update == 0) fprintf (stderr, " Assignment");
        else                fprintf (stderr, " Update");

        fprintf (stderr, " :: Target [%08d] Source [%08d] LCN [%02d]%s", target, source, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block - no allow/whitelist support for i-calls
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        //this is working now with the new import setup
        if (opts->trunk_tune_private_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block 
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %d SRC %d I-Call.wav", getDateE(), timestr, state->edacs_site_id, target, source);
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
        lcn  = (fr_4t & 0x1F00000000) >> 32;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Channel Assignment (Unknown Data) :: Source [%08d] LCN [%02d]%s", source, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //System All-Call Grant Update
      else if (mt1 == 0x16)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int is_digital = (fr_1t & 0x10000000) >> 28;
        int is_update = (fr_1t & 0x8000000) >> 27;
        int source = (fr_4t & 0xFFFFF000) >> 12;
                         state->lasttg = -1; // represent system all-call as TG -1 to differentiate from real TGs
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;

        fprintf (stderr, "%s", KMAG);
        if (is_digital == 0) fprintf (stderr, " Analog System All-Call");
        else                 fprintf (stderr, " Digital System All-Call");
        if (is_update == 0) fprintf (stderr, " Assignment");
        else                fprintf (stderr, " Update");
        
        fprintf (stderr, " :: Source [%08d] LCN [%02d]%s", source, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'A' to mode for allow - always allow all-calls by default
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "A");

        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block 
        {
          if (lcn > 0 && lcn < 26 && state->edacs_cc_lcn != 0 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld SRC %d All-Call.wav", getDateE(), timestr, state->edacs_site_id, source);
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
        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
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
          fprintf (stderr, " FR_1 [%010llX]", fr_1t);
          fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        }
      }

    }
    //Start Standard or Networked Mode
    else if (state->ea_mode == 0)
    {
      unsigned char mt_a = (fr_1t & 0xE000000000) >> 37;
      unsigned char mt_b = (fr_1t & 0x1C00000000) >> 34;
      unsigned char mt_d = (fr_1t & 0x3E0000000) >> 29;

      //Add raw payloads and MT-A/MT-B/MT-D for easy debug
      if (opts->payload == 1)
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
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

      //Reverse engineered from Quebec STM system; occurs immediately prior to Voice Group Channel Update
      if (mt_a == 0x0 || mt_a == 0x1)
      {
        //Emergency, LID, and transmission trunking values are not confirmed, need validation
        int is_emergency = (mt_a == 0x1) ? 1 : 0;
        int lid = ((fr_1t & 0x1FC0000000) >> 23) | ((fr_4t & 0xFE0000000) >> 29);
        int lcn = (fr_1t & 0x1F000000) >> 24;
        int is_tx_trunk = (fr_1t & 0x800000) >> 23;
        int group = (fr_1t & 0x7FF000) >> 12;

        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " Voice Group Channel Assignment :: Analog Group [%04d] LID [%05d] LCN [%02d]%s", group, lid, lcn, get_lcn_status_string(lcn));
        if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
        if (is_emergency == 1)
        {
          fprintf (stderr, "%s", KRED);
          fprintf (stderr, " [EMERGENCY]");
        }
        fprintf (stderr, "%s", KNRM);

        // TODO: Actually process the call
      }
      //Voice Group Channel Assignment (6.2.4.1)
      //Emergency Voice Group Channel Assignment (6.2.4.2)
      //Reverse engineered as being for digital calls from San Antonio/Bexar Co system
      else if (mt_a == 0x2 || mt_a == 0x3)
      {
        int is_emergency = (mt_a == 0x3) ? 1 : 0;
        int lid = ((fr_1t & 0x1FC0000000) >> 23) | ((fr_4t & 0xFE0000000) >> 29);
        int lcn = (fr_1t & 0x1F000000) >> 24;
        int is_tx_trunk = (fr_1t & 0x800000) >> 23;
        int group = (fr_1t & 0x7FF000) >> 12;

        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " Voice Group Channel Assignment :: Digital Group [%04d] LID [%05d] LCN [%02d]%s", group, lid, lcn, get_lcn_status_string(lcn));
        if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
        if (is_emergency == 1)
        {
          fprintf (stderr, "%s", KRED);
          fprintf (stderr, " [EMERGENCY]");
        }
        fprintf (stderr, "%s", KNRM);

        // TODO: Actually process the call
      }
      //Data Call Channel Assignment (6.2.4.3)
      else if (mt_a == 0x5)
      {
        int is_individual_call = (fr_1t & 0x1000000000) >> 36;
        int is_from_lid = (fr_1t & 0x800000000) >> 35;
        int port = ((fr_1t & 0x700000000) >> 29) | ((fr_4t & 0x700000000) >> 32);
        int lcn = (fr_1t & 0xF8000000) >> 27;
        int is_individual_id = (fr_1t & 0x4000000) >> 26;
        int lid = (fr_1t & 0x3FFF000) >> 12;
        int group = (fr_1t & 0x7FF000) >> 12;

        fprintf (stderr, "%s", KBLU);
        fprintf (stderr, " Data Call Channel Assignment :: Type");
        if (is_individual_call == 1) fprintf (stderr, " [Individual]");
        else                         fprintf (stderr, " [Group]");
        if (is_individual_id == 1) fprintf (stderr, " LID [%05d]", lid);
        else                       fprintf (stderr, " Group [%04d]", group);
        if (is_from_lid == 1) fprintf (stderr, " -->");
        else                  fprintf (stderr, " <--");
        fprintf (stderr, " Port [%02d] LCN [%02d]%s", port, lcn, get_lcn_status_string(lcn));
        fprintf (stderr, "%s", KNRM);
      }
      //Login Acknowledge (6.2.4.4)
      else if (mt_a == 0x6)
      {
        int group = (fr_1t & 0x1FFC000000) >> 26;
        int lid = (fr_1t & 0x3FFF000) >> 12;

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
          int status = (fr_1t & 0x3FC000000) >> 26;
          int lid = (fr_1t & 0x3FFF000) >> 12;

          fprintf (stderr, "%s", KBLU);
          if (status == 248) fprintf (stderr, " Status Request :: LID [%05d]", lid);
          else               fprintf (stderr, " Message Acknowledgement :: Status [%03d] LID [%05d]", status, lid);
          fprintf (stderr, "%s", KNRM);
        }
        //Interconnect Channel Assignment (6.2.4.6)
        else if (mt_b == 0x1)
        {
          int mt_c = (fr_1t & 0x300000000) >> 32;
          int lcn = (fr_1t & 0xF8000000) >> 27;
          int is_individual_id = (fr_1t & 0x4000000) >> 26;
          int lid = (fr_1t & 0x3FFF000) >> 12;
          int group = (fr_1t & 0x7FF000) >> 12;

          //Abstract away to a target, and be sure to check whether it's an individual call later
          int target = (is_individual_id == 0) ? group : lid;
          
          fprintf (stderr, "%s", KMAG);
          fprintf (stderr, " Interconnect Channel Assignment :: Type");
          if (mt_c == 0x2) fprintf (stderr, " [Voice]");
          else             fprintf (stderr, " [Reserved]");
          if (is_individual_id == 1) fprintf (stderr, " LID [%05d]", target);
          else                       fprintf (stderr, " Group [%04d]", target);
          fprintf (stderr, " LCN [%02d]%s", lcn, get_lcn_status_string(lcn));
          fprintf (stderr, "%s", KNRM);

          // TODO: Actually process the call
        }
        //Channel Updates (6.2.4.7)
        else if (mt_b == 0x3)
        {
          int mt_c = (fr_1t & 0x300000000) >> 32;
          int lcn = (fr_1t & 0xF8000000) >> 27;
          int is_individual = (fr_1t & 0x4000000) >> 26;
          int is_emergency = (fr_1t & 0x2000000) >> 25;
          int group = (fr_1t & 0x7FF000) >> 12;
          int lid = ((fr_1t & 0x1FC0000) >> 11) | ((fr_4t & 0x7F000) >> 12);

          //Abstract away to a target, and be sure to check whether it's an individual call later
          int target = (is_individual == 0) ? group : lid;
          
          //Technically only MT-C 0x1 and 0x3 are defined in TSB 69.3 - using and extrapolating on legacy code
          int is_tx_trunk = (mt_c == 2 || mt_c == 3) ? 1 : 0;
          int is_digital = (mt_c == 1 || mt_c == 3) ? 1 : 0;

          if (is_individual == 0)
          {
            fprintf (stderr, "%s", KGRN);
            fprintf (stderr, " Voice Group Channel Update ::");
          }
          else
          {
            fprintf (stderr, "%s", KCYN);
            fprintf (stderr, " Voice Individual Channel Update ::");
          }
          if (is_digital == 0) fprintf (stderr, " Analog");
          else                 fprintf (stderr, " Digital");
          if (is_individual == 0) fprintf (stderr, " Group [%04d]", target);
          else                    fprintf (stderr, " LID [%05d]", target);
          fprintf (stderr, " LCN [%02d]%s", lcn, get_lcn_status_string(lcn));
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
          state->edacs_vc_lcn = lcn;

          //If we have the same target already in progress, we can keep the source ID since we know it (and don't get
          //it in this CC message). But if we have a different target, we should clear out to a source ID of 0.
          if (is_individual == 0)
          {
            if (state->lasttg != target) {
              state->lasttg = target;
              state->lastsrc = 0;
            }
          }
          else
          {
            //Use IDs > 10000 to represent i-call targets to differentiate from TGs
            if (state->lasttg != target + 10000) {
              state->lasttg = target + 10000;
              state->lastsrc = 0;
            }
          }

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
            //TG hold on EDACS Standard/Net -- block non-matching target, allow matching group
            if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
            if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");
          }

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
                if (is_individual == 0) sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %04d SRC %05d.wav", getDateE(), timestr, state->edacs_site_id, target, state->lastsrc);
                else                    sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %05d SRC %05d I-Call.wav", getDateE(), timestr, state->edacs_site_id, target, state->lastsrc);
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
          int sgid = (fr_1t & 0x3FF800000) >> 23;
          int group = (fr_1t & 0x7FF000) >> 12;

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " System Assigned ID :: SGID [%04d] Group [%04d]", sgid, group);
          fprintf (stderr, "%s", KNRM);
        }
        //Individual Call Channel Assignment (6.2.4.9)
        else if (mt_b == 0x5)
        {
          int is_tx_trunk = (fr_1t & 0x200000000) >> 33;
          int lcn = (fr_1t & 0xF8000000) >> 27;
          int call_type = (fr_1t & 0x4000000) >> 26;
          int target = (fr_1t & 0x3FFF000) >> 12;
          int source = (fr_4t & 0x3FFF000) >> 12;

          fprintf (stderr, "%s", KCYN);
          fprintf (stderr, " Individual Call Channel Assignment :: Type");
          if (call_type == 1) fprintf (stderr, " [Voice]");
          else                fprintf (stderr, " [Reserved]");
          fprintf (stderr, " Caller [%05d] Callee [%05d] LCN [%02d]%s", source, target, lcn, get_lcn_status_string(lcn));
          if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
          fprintf (stderr, "%s", KNRM);

          // TODO: Actually process the call
        }
        //Console Unkey / Drop (6.2.4.10)
        else if (mt_b == 0x6)
        {
          int is_drop = (fr_1t & 0x80000000) >> 31;
          int lcn = (fr_1t & 0x7C000000) >> 26;
          int lid = (fr_1t & 0x3FFF000) >> 12;

          fprintf (stderr, "%s", KYEL);
          fprintf (stderr, " Console ");
          if (is_drop == 0) fprintf (stderr, " Unkey");
          else              fprintf (stderr, " Drop");
          fprintf (stderr, " :: LID [%05d] LCN [%02d]%s", lid, lcn, get_lcn_status_string(lcn));
          fprintf (stderr, "%s", KNRM);
        }
        //Use MT-D
        else if (mt_b == 0x7)
        {
          //Cancel Dynamic Regroup (6.2.4.11)
          if (mt_d == 0x00)
          {
            int knob = (fr_1t & 0x1C000000) >> 26;
            int lid = (fr_1t & 0x3FFF000) >> 12;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Cancel Dynamic Regroup :: LID [%05d] Knob position [%1d]", lid, knob + 1);
            fprintf (stderr, "%s", KNRM);
          }
          //Adjacent Site Control Channel (6.2.4.12)
          else if (mt_d == 0x01)
          {
            int adj_cc_lcn = (fr_1t & 0x1F000000) >> 24;
            int adj_site_index = (fr_1t & 0xE00000) >> 21;
            int adj_site_id = (fr_1t & 0x1F0000) >> 16;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Adjacent Site Control Channel :: Site ID [%02X][%03d] Index [%1d] LCN [%02d]%s", adj_site_id, adj_site_id, adj_site_index, adj_cc_lcn, get_lcn_status_string(adj_cc_lcn));
            if (adj_site_id == 0 && adj_site_index == 0)      fprintf (stderr, " [Adjacency Table Reset]");
            else if (adj_site_id != 0 && adj_site_index == 0) fprintf (stderr, " [Priority System Definition]");
            else if (adj_site_id == 0 && adj_site_index != 0) fprintf (stderr, " [Adjacencies Table Length Definition]");
            else                                              fprintf (stderr, " [Adjacent System Definition]");
            fprintf (stderr, "%s", KNRM);
          }
          //Extended Site Options (6.2.4.13)
          else if (mt_d == 0x02)
          {
            int msg_num = (fr_1t & 0xE000000) >> 25;
            int data = (fr_1t & 0x1FFF000) >> 12;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Extended Site Options :: Message Num [%1d] Data [%04X]", msg_num, data);
            fprintf (stderr, "%s", KNRM);
          }
          //System Dynamic Regroup Plan Bitmap (6.2.4.14)
          else if (mt_d == 0x04)
          {
            int bank = (fr_1t & 0x10000000) >> 28;
            int resident = (fr_1t & 0xFF00000) >> 20;
            int active = (fr_1t & 0xFF000) >> 12;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " System Dynamic Regroup Plan Bitmap :: Plan Bank [%1d] Resident [", bank);

            int plan = bank * 8;
            int first = 1;
            while (resident != 0) {
              if (resident & 0x1 == 1) {
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
              if (active & 0x1 == 1) {
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
            fprintf (stderr, "%s", KNRM);
          }
          //Assignment to Auxiliary Control Channel (6.2.4.15)
          else if (mt_d == 0x05)
          {
            int aux_cc_lcn = (fr_1t & 0x1F000000) >> 24;
            int group = (fr_1t & 0x7FF000) >> 12;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Assignment to Auxiliary CC :: Group [%04d] Aux CC LCN [%02d]%s", group, aux_cc_lcn, get_lcn_status_string(aux_cc_lcn));
            fprintf (stderr, "%s", KNRM);
          }
          //Initiate Test Call Command (6.2.4.16)
          else if (mt_d == 0x06)
          {
            int cc_lcn = (fr_1t & 0x1F000000) >> 24;
            int wc_lcn = (fr_1t & 0xF80000) >> 19;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Initiate Test Call Command :: CC LCN [%02d] WC LCN [%02d]", cc_lcn, wc_lcn);
            fprintf (stderr, "%s", KNRM);
          }
          //Unit Enable / Disable (6.2.4.17)
          else if (mt_d == 0x07)
          {
            int qualifier = (fr_1t & 0xC000000) >> 26;
            int target = (fr_1t & 0x3FFF000) >> 12;

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
            int cc_lcn = (fr_1t & 0x1F000000) >> 24;
            int priority = (fr_1t & 0xE00000) >> 21;
            int is_scat = (fr_1t & 0x80000) >> 19;
            int is_failsoft = (fr_1t & 0x40000) >> 18;
            int is_auxiliary = (fr_1t & 0x20000) >> 17;
            int site_id = (fr_1t & 0x1F000) >> 12;

            fprintf (stderr, "%s", KYEL);
            fprintf (stderr, " Standard/Networked :: Site ID [%02X][%03d] Priority [%1d] CC LCN [%02d]%s", site_id, site_id, priority, cc_lcn, get_lcn_status_string(cc_lcn));
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
          else if (mt_d == 0x0F)
          {
            int lcn = (fr_1t & 0x1F000000) >> 24;
            int qualifier = (fr_1t & 0x800000) >> 23;
            int is_update = (fr_1t & 0x400000) >> 22;
            int is_tx_trunk = (fr_1t & 0x200000) >> 21;
            int lid = ((fr_1t & 0x7F000) >> 5) | ((fr_4t & 0x7F000) >> 12);

            fprintf (stderr, "%s", KMAG);
            fprintf (stderr, " System All-Call Channel");
            if (is_update == 0) fprintf (stderr, " Assignment");
            else                fprintf (stderr, " Update");
            fprintf (stderr, " ::");
            if (qualifier == 1) fprintf (stderr, " [Voice]");
            else                fprintf (stderr, " [Reserved]");
            fprintf (stderr, " LID [%05d] LCN [%02d]%s", lid, lcn, get_lcn_status_string(lcn));
            if (is_tx_trunk == 0) fprintf (stderr, " [Message Trunking]");
            fprintf (stderr, "%s", KNRM);

            // TODO: Actually process the call
          }
          //Dynamic Regrouping (6.2.4.20)
          else if (mt_d == 0x10)
          {
            int fleet_bits = (fr_1t & 0x1C000000) >> 26;
            int lid = (fr_1t & 0x3FFF000) >> 12;
            int plan = (fr_4t & 0x1E0000000) >> 29;
            int type = (fr_4t & 0x18000000) >> 27;
            int knob = (fr_4t & 0x7000000) >> 24;
            int group = (fr_4t & 0x7FF000) >> 12;

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
              fprintf (stderr, " FR_1 [%010llX]", fr_1t);
              fprintf (stderr, " FR_4 [%010llX]", fr_4t);
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
            fprintf (stderr, " FR_1 [%010llX]", fr_1t);
            fprintf (stderr, " FR_4 [%010llX]", fr_4t);
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
          fprintf (stderr, " FR_1 [%010llX]", fr_1t);
          fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        }
      }

    } //end Standard or Networked

    //let users know they need to select an operational mode with the switches below
    else 
    {
      fprintf (stderr, " Detected: Use -fh, -fH, -fe, or -fE for std, esk, ea, or ea-esk;");
      fprintf (stderr, "\n");
      fprintf (stderr, " FR_1 [%010llX]", fr_1t);
      fprintf (stderr, " FR_4 [%010llX]", fr_4t);
    }

  }


  free (timestr); //free allocated memory to prevent memory leak
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

 
