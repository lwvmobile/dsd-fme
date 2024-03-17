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
    if (afs != 0)
    {
      if (state->ea_mode == 0) fprintf (stderr, " AFS [%03d] [%02d-%03d] LCN [%02d]", afs, afs >> 7, afs & 0x7F, lcn);
      else fprintf (stderr, " TG/TGT: [%d] LCN [%02d]", afs, lcn);
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

  unsigned char command = 0xFF;
  unsigned char mt1 = 0x1F;
  unsigned char mt2 = 0xF;
  unsigned char lcn = 0;

  //commands; may not use these anymore
  unsigned int vcmd = 0xEE; //voice command variable
  unsigned int idcmd = 0xFD;
  unsigned int peercmd = 0xF88; //using for EA detection test
  unsigned int netcmd = 0xF3; //using for Networked Test
  UNUSED2(vcmd, idcmd);

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

    //ESK on/off detection, I honestly don't remember the logic for this anymore, but it works fine
    if ( (((fr_1t & 0xF000000000) >> 36) != 0xB)  && (((fr_1t & 0xF000000000) >> 36) != 0x1) && (((fr_1t & 0xFF00000000) >> 32) != 0xF3) )
    {
      //experimenting with values here, not too high, and not too low
      if ( (((fr_1t & 0xF000000000) >> 36) <= 0x8 ))
      { 
        state->esk_mask = 0xA0;
      }
      //ideal value would be 5, but some other values exist that don't allow it
      if ( (((fr_1t & 0xF000000000) >> 36) > 0x8 ) )
      { 
        state->esk_mask = 0x0;
      }
    }

    //Standard/Networked Auto Detection
    //if (command == netcmd) //netcmd is F3 Standard/Networked (I think)
    if ( (fr_1t >> 32 == netcmd) || (fr_1t >> 32 == (netcmd ^ 0xA0)) ) 
    { 
      state->ea_mode = 0; //disable extended addressing mode
    }

    //EA Auto detection //peercmd is 0xF88 peer site relay 0xFF80000000 >> 28
    if (fr_1t >> 28 == peercmd || fr_1t >> 28 == (peercmd ^ 0xA00) )
    {
      state->ea_mode = 1; //enable extended addressing mode
    }

    //Start Extended Addressing Mode 
    if (state->ea_mode == 1)
    {
      command = ((fr_1t & 0xFF00000000) >> 32) ^ state->esk_mask;
      mt1 = (command & 0xF8) >> 3;
      mt2 = (fr_1t & 0x780000000) >> 31;

      //Site ID
      unsigned long long int site_id = 0; //we probably could just make this an int as well as the state variables
      if (mt1 == 0x1F && mt2 == 0xA)
      {
        site_id = ((fr_1 & 0x1F000) >> 12) | ((fr_1 & 0x1F000000) >> 19);
        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Site ID [%02llX][%03lld] Extended Addressing", site_id, site_id);
        fprintf (stderr, "%s", KNRM);
        state->edacs_site_id = site_id;
      }
      //Patch Groups
      else if (mt1 == 0x1F && mt2 == 0xC)
      {
        int patch_site = ((fr_4t & 0xFF00000000) >> 32); //is site info valid, 0 for all sites? else patch only good on site listed?
        int sourcep = ((fr_1t & 0xFFFF000) >> 12);
        int targetp = ((fr_4t & 0xFFFF000) >> 12);
        fprintf (stderr, " Patch -- Site [%d] Source [%d] Target [%d] ", patch_site, sourcep, targetp);
      }
      //Adjacent Sites
      else if (mt1 == 0x1F && mt2 == 0x1)
      {
        fprintf (stderr, " Adjacent Site");
        if ( ((fr_1t & 0xFF000) >> 12) > 0 )
        {
          int adj = (fr_1t & 0xFF000) >> 12;
          int adj_l = (fr_1t & 0x1F000000) >> 24;
          fprintf (stderr, " [%02X][%03d] on CC LCN [%02d]", adj, adj, adj_l);
        }
      }
      //Control Channel LCN
      else if (mt1 == 0x1F && mt2 == 0x8)
      {
        fprintf (stderr, " Control Channel LCN");
        if (((fr_4t >> 12) & 0x1F) != 0)
        {
          state->edacs_cc_lcn = ((fr_4t >> 12) & 0x1F);
          if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) //26, or 27. shouldn't matter don't think cc lcn will give a status lcn val 
          {
            state->edacs_lcn_count = state->edacs_cc_lcn;
          }
          fprintf (stderr, " [%d]", state->edacs_cc_lcn);

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
      }
      //disabling kick command, data looks like its just FFFF, no actual values, can't verify accuracy
      // else if (mt1 == 0x1F && mt2 == 0xB) //KICK LISTING for EA?? Unverified, but probably observed in Unitrunker way back when.
      // {
      //   int kicked = (fr_4t & 0xFFFFF000) >> 12;
      //   fprintf (stderr, " FR_1 [%010llX]", fr_1t);
      //   fprintf (stderr, " FR_3 [%010llX]", fr_3);
      //   fprintf (stderr, " FR_4 [%010llX]", fr_4t);
      //   fprintf (stderr, " FR_6 [%010llX]", fr_6);
      //   fprintf (stderr, " Kick Command?");
      // }
      //Voice Call Grant Update
      // mt1 0x12 is analog group voice call, 0x3 is Digital group voice call, 0x2 Group Data Channel, 0x1 TDMA call
      else if ((mt1 >= 0x1 && mt1 <= 0x3) || mt1 == 0x12)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int group  = (fr_1t & 0xFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        if (group != 0)  state->lasttg = group;
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " Group [%05d] Source [%08d] LCN[%02d]", group, source, lcn);

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

        if (mt1 == 0x1) fprintf (stderr, " TDMA Call"); //never observed, wonder if any EDACS systems ever carried a TDMA signal (X2-TDMA?)
        if (mt1 == 0x2) fprintf (stderr, " Group Data Call"); //Never Seen this one before
        if (mt1 == 0x3) fprintf (stderr, " Digital Call"); //ProVoice, this is what we always get on SLERS EA
        if (mt1 == 0x12) fprintf (stderr, " Analog Call"); //analog, to at least log that we recognize it
        fprintf (stderr, "%s", KNRM);

        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block 
        {
          if (lcn < 26 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TG %d SRC %d.wav", getDateE(), timestr, state->edacs_site_id, group, source);
              if (mt1 != 0x12) //digital
                openWavOutFile (opts, state);
              else //analog
                openWavOutFile48k (opts, state); //
            }
            
            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (mt1 == 0x12) //analog
                edacs_analog(opts, state, group, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (mt1 == 0x6) //analog
                edacs_analog(opts, state, group, lcn);
              #endif
            }

          }
          
        }
      }
      //I-Call Grant Update
      else if (mt1 == 0x4)
      {
        lcn = (fr_4t & 0x1F00000000) >> 32;

        //LCNs greater than 26 are considered status values, "Busy, Queue, Deny, etc"
        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }

        int target = (fr_1t & 0xFFFFF000) >> 12;
        int source = (fr_4t & 0xFFFFF000) >> 12;
        if (target != 0) state->lasttg = target + 100000; //Use IDs > 100000 to represent i-call targets to differentiate from TGs
        if (source != 0) state->lastsrc = source;
        if (lcn != 0)    state->edacs_vc_lcn = lcn;
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " I-Call Target [%08d] Source [%08d] LCN[%02d]", target, source, lcn);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block - no allow/whitelist support for i-calls
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        if (mt2 == 0xA) fprintf (stderr, " Analog Call");
        if (mt2 == 0xE) fprintf (stderr, " Digital Call");
        fprintf (stderr, "%s", KNRM);

        //this is working now with the new import setup
        if (opts->trunk_tune_private_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block 
        {
          if (lcn < 26 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here, should probably check as well to make sure we have a valid trunking method active (rigctl, rtl)
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld TGT %d SRC %d.wav", getDateE(), timestr, state->edacs_site_id, target, source);
              if (mt2 == 0xE) //digital
                openWavOutFile (opts, state);
              else //analog
                openWavOutFile48k (opts, state); //
            }
            
            //do condition here, in future, will allow us to use tuning methods as well, or rtl_udp as well
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw); 
              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because the lcn index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              //
              if (mt2 == 0xA) //analog
                edacs_analog(opts, state, target, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              //
              if (mt2 == 0x10)
                edacs_analog(opts, state, target, lcn);
              #endif
            }

          }
        }
      }
      else //print frames for debug/analysis
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Unknown Command");
      }

    }
    //Start Standard or Networked Mode
    else if (state->ea_mode == 0)
    {
      //standard or networked
      command = ((fr_1t & 0xFF00000000) >> 32) ^ state->esk_mask;
      lcn     = (fr_1t & 0xF8000000) >> 27;

      //site ID and CC LCN
      if (command == 0xFD)
      {
        int site_id = (fr_1t & 0xFF000) >> 12;
        int cc_lcn = (fr_1t & 0x1F000000) >> 24;

        fprintf (stderr, "%s", KYEL);
        fprintf (stderr, " Site ID [%02X][%03d] on CC LCN [%02d] Standard/Networked", site_id, site_id, cc_lcn);
        fprintf (stderr, "%s", KNRM);
        state->edacs_site_id = site_id;
        state->edacs_cc_lcn = cc_lcn;

        if (state->edacs_cc_lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = state->edacs_cc_lcn;
        }

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

      //voice call assignment
      else if (command == 0xEE || command == 0xEF)
      {
        lcn = (fr_1t & 0x3E0000000) >> 29;

        if (lcn > state->edacs_lcn_count && lcn < 26) 
        {
          state->edacs_lcn_count = lcn;
        }
        state->edacs_vc_lcn = lcn;
        //just going to use the default 4-4-3 A-FS scheme, making it user configurable would be a pain
        int afs = (fr_1t & 0x7FF000) >> 12;
        int a = afs >> 7; 
        int fs = afs & 0x7F;
        int status  = (fr_1t & 0xF00000000) >> 32;
        UNUSED(status);
        if (afs > 0) state->lastsrc = afs; 
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, " AFS [%03d] [%02d-%03d] LCN [%02d]", afs, a, fs, lcn);

        char mode[8]; //allow, block, digital enc
        sprintf (mode, "%s", "");

        //if we are using allow/whitelist mode, then write 'B' to mode for block
        //comparison below will look for an 'A' to write to mode if it is allowed
        if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == afs)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on EDACS Standard/Net -- block non-matching target, allow matching afs (decimal)
        if (state->tg_hold != 0 && state->tg_hold != afs) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == afs) sprintf (mode, "%s", "A");

        //NOTE: Restructured below so that analog and digital are handled the same, just that when
        //its analog, it will now start edacs_analog which will while loop analog samples until
        //signal level drops (RMS, or a dotting sequence is detected)

        //analog working now with edacs_analog
        if (command == 0xEE)
          fprintf (stderr, " Analog"); 

        //Digital Call (ProVoice, hopefully, and not Aegis in 2022)
        else
          fprintf (stderr, " Digital");

        //skip analog calls on RTL Input on ARM devices due to High CPU usage from RTL RMS function
        // #ifdef __arm__
        // if (command == 0xEE && opts->audio_in_type == 3)
        //   goto ENDPV;
        // #endif





        //this is working now with the new import setup
        if (opts->trunk_tune_group_calls == 1 && opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0) ) //DE is digital encrypted, B is block
        {
          if (lcn < 26 && state->trunk_lcn_freq[lcn-1] != 0) //don't tune if zero (not loaded or otherwise)
          {
            //openwav file and do per call right here
            if (opts->dmr_stereo_wav == 1 && (opts->use_rigctl == 1 || opts->audio_in_type == 3))
            {
              sprintf (opts->wav_out_file, "./WAV/%s %s EDACS Site %lld AFS %02d-%03d - %03d.wav", getDateE(), timestr, state->edacs_site_id, a, fs, afs);
              if (command == 0xEF) openWavOutFile (opts, state); //digital
              if (command == 0xEE) openWavOutFile48k (opts, state); //analog at 48k
            }

            if (opts->use_rigctl == 1)
            {
              //only set bandwidth IF we have an original one to fall back to (experimental, but requires user to set the -B 12000 or -B 24000 value manually)
              if (opts->setmod_bw != 0 && command == 0xEE) SetModulation(opts->rigctl_sockfd, 7000); //narrower bandwidth, but has issues with dotting sequence
              else if (opts->setmod_bw != 0) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              
              // if (opts->setmod_bw != 0) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);

              SetFreq(opts->rigctl_sockfd, state->trunk_lcn_freq[lcn-1]); //minus one because our index starts at zero
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (command == 0xEE) edacs_analog(opts, state, afs, lcn);
            }

            if (opts->audio_in_type == 3) //rtl dongle
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, state->trunk_lcn_freq[lcn-1]);
              state->edacs_tuned_lcn = lcn;
              opts->p25_is_tuned = 1;
              if (command == 0xEE) edacs_analog(opts, state, afs, lcn);
              #endif
            }
          }
        
        }
        fprintf (stderr, "%s", KNRM);
      }
      //Network Command, Not sure of how to handle, but just show frames
      else if (command == 0xF3)
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Network Command");
      }

      else //print frames for debug/analysis
      {
        fprintf (stderr, " FR_1 [%010llX]", fr_1t);
        fprintf (stderr, " FR_4 [%010llX]", fr_4t);
        fprintf (stderr, " Unknown Command");
      }

    } //end Standard or Networked

    //supply user warning to use -9 switch if decoding doesn't start shortly
    else fprintf (stderr, " Net/EA Auto Detect; Use -9 CLI Switch For Standard;");

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

 