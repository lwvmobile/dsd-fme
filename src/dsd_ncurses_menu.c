/*-------------------------------------------------------------------------------
* dsd_ncurses_menu.c
* DSD-FME ncurses terminal menu system
*
* LWVMOBILE
* 2025-05 DSD-FME Florida Man Edition
*-----------------------------------------------------------------------------*/

#include "dsd.h"

#ifdef USE_RTLSDR
#include <rtl-sdr.h>
//use to list out all detected RTL dongles
char vendor[256], product[256], serial[256], userdev[256];
int device_count = 0;
#endif

uint32_t temp_freq = -1;

//struct for checking existence of directory to write to
struct stat st_wav = {0};
int reset = 0;

//testing a few things, going to put this into ncursesMenu
#define WIDTH 36
#define HEIGHT 25

int startx = 0;
int starty = 0;

char *choicesc[] = {
  "Return",
  "      ", //Save Decoded Audio WAV (Legacy Mode) //Disabled
  "Save Signal to Symbol Capture Bin",
  "Toggle Muting Encrypted Traffic    ",
  "Save Per Call Decoded WAV",
  "Setup and Start RTL Input ",
  "Retune RTL Dongle         ",
  "Toggle C4FM/QPSK (P2 TDMA CC)",
  "Toggle C4FM/QPSK (P1 FDMA CC)",
  "Start TCP Direct Link Audio",
  "Configure RIGCTL",
  "Stop All Decoded WAV Saving",
  "Read OP25/FME Symbol Capture Bin",
  "Replay Last Symbol Capture Bin",
  "Stop & Close Symbol Capture Bin Playback",
  "Stop & Close Symbol Capture Bin Saving",
  "Toggle Call Alert Beep     ",
  "Resume Decoding"
  };
//make tweaks for TDMA and AUTO?
char *choices[] = {
  "Resume Decoding",
  "Decode AUTO",
  "Decode TDMA",
  "Decode DSTAR",
  "Decode M17",
  "Decode EDACS/PV",
  "Decode P25p2",
  "Decode dPMR",
  "Decode NXDN48",
  "Decode NXDN96",
  "Decode DMR",
  "Decode YSF",
  "Toggle Signal Inversion",
  "Key Entry",
  "Reset Event History",
  "Toggle Payloads to Console",
  "Manually Set p2 Parameters", //16
  "Input & Output Options",
  "LRRP Data to File",
  "Exit DSD-FME",
  };


int n_choices = sizeof(choices) / sizeof(char *);
// int n_choicesb = sizeof(choicesb) / sizeof(char *);
int n_choicesc = sizeof(choicesc) / sizeof(char *);

void print_menu(WINDOW *menu_win, int highlight)
{
	int x, y, i;

	x = 2;
	y = 2;
	box(menu_win, 0, 0);
	for(i = 0; i < n_choices; ++i)
	{	if(highlight == i + 1) /* High light the present choice */
		{	wattron(menu_win, A_REVERSE);
			mvwprintw(menu_win, y, x, "%s", choices[i]);
			wattroff(menu_win, A_REVERSE);
		}
		else
			mvwprintw(menu_win, y, x, "%s", choices[i]);
		++y;
	}
	wrefresh(menu_win);
}

void print_menuc(WINDOW *menu_win, int highlight)
{
	int x, y, i;

	x = 2;
	y = 2;
	box(menu_win, 0, 0);
	for(i = 0; i < n_choicesc; ++i)
	{	if(highlight == i + 1) /* High light the present choice */
		{	wattron(menu_win, A_REVERSE);
			mvwprintw(menu_win, y, x, "%s", choicesc[i]);
			wattroff(menu_win, A_REVERSE);
		}
		else
			mvwprintw(menu_win, y, x, "%s", choicesc[i]);
		++y;
	}
	wrefresh(menu_win);
}

//ncursesMenu
void ncursesMenu (dsd_opts * opts, dsd_state * state)
{

  //update sync time on cc sync so we don't immediately go CC hunting when exiting the menu
  state->last_cc_sync_time = time(NULL);

  //close pulse output if not null output
  if (opts->audio_out == 1 && opts->audio_out_type == 0)
  {
    closePulseOutput (opts);
  }

  //close OSS output
  if (opts->audio_out_type == 2 || opts->audio_out_type == 5)
  {
    close (opts->audio_out_fd);
  }

  if (opts->audio_in_type == 0) //close pulse input if it is the specified input method
  {
    closePulseInput(opts);
  }

  if (opts->audio_in_type == 3)
  {
    #ifdef USE_RTLSDR
    rtl_clean_queue();
    #endif
  }

  if (opts->audio_in_type == 8) //close TCP input SF file so we don't buffer audio while not decoding
  {
    sf_close(opts->tcp_file_in);
  }


  state->payload_keyid = 0;
  state->payload_keyidR = 0;

  //zero out
  state->nxdn_last_tg = 0;
  state->nxdn_last_ran = -1; //0
  state->nxdn_last_rid = 0; //0

  WINDOW *menu_win;
  WINDOW *test_win;
  WINDOW *entry_win;
  WINDOW *info_win;
	int highlight  = 1;
  int highlightc = 1;
	int choice  = 0;
  int choicec = 0;
	int c;
  int e;

  startx = 2;
  starty = 1;

	menu_win = newwin(HEIGHT, WIDTH, starty, startx);
	keypad(menu_win, TRUE);
	mvprintw(0, 0, "  Use arrow keys to go up and down, Press ENTER to select a choice.");
	refresh();
	print_menu(menu_win, highlight);
  while(1)
	{	c = wgetch(menu_win);
		switch(c)
		{	case KEY_UP:
				if(highlight == 1)
					highlight = n_choices;
				else
					--highlight;
				break;
			case KEY_DOWN:
				if(highlight == n_choices)
					highlight = 1;
				else
					++highlight;
				break;
			case 10:
				choice = highlight;
				break;
			default:
				//mvprintw(24, 0, "Character pressed is = %3d Hopefully it can be printed as '%c'", c, c);
				refresh();
				break;
		}

		print_menu(menu_win, highlight);


    if (highlight == 2)
    {
      info_win = newwin(7, WIDTH+18, starty, startx+20);
      box (info_win, 0, 0);
      mvwprintw(info_win, 2, 2, " AUTO Decoding Class Supports the following:");
      mvwprintw(info_win, 3, 2, " P25p1, YSF, DSTAR, X2-TDMA and DMR");
      mvwprintw(info_win, 4, 2, " C4FM or QPSK @ 4800bps (no H8D-QPSK)");
      wrefresh(info_win);
    }

    if (highlight == 3)
    {
      info_win = newwin(7, WIDTH+18, starty, startx+20);
      box (info_win, 0, 0);
      mvwprintw(info_win, 2, 2, " TDMA Trunking Class Supports the following:");
      mvwprintw(info_win, 3, 2, " P25p1 Voice/Control, P25p2 Traffic, and DMR");
      mvwprintw(info_win, 4, 2, " C4FM or QPSK @ 4800 or 6000 (no H8D-QPSK)");
      wrefresh(info_win);
    }

    if (highlight == 7)
    {
      info_win = newwin(7, WIDTH+18, starty, startx+20);
      box (info_win, 0, 0);
      mvwprintw(info_win, 2, 2, " P25p2 Control (MAC_SIGNAL) or Single Voice Freq.");
      mvwprintw(info_win, 3, 2, " NOTE: Manually set WACN/SYSID/CC on Voice Only");
      mvwprintw(info_win, 4, 2, " C4FM or QPSK @ 6000 (no H8D-QPSK)");
      wrefresh(info_win);
    }

    //Input Output Options
    if (choice == 18)
    {
      //test making a new window while other window is open
      test_win = newwin(HEIGHT-1, WIDTH+9, starty+5, startx+5);
      keypad(menu_win, FALSE);
      keypad(test_win, TRUE);
      mvprintw(0, 0, "Input & Output Options                            ");
      refresh();
      print_menuc(test_win, highlightc);
      while(1)
    	{	e = wgetch(test_win);
    		switch(e)
    		{	case KEY_UP:
    				if(highlightc == 1)
    					highlightc = n_choicesc;
    				else
    					--highlightc;
    				break;
    			case KEY_DOWN:
    				if(highlightc == n_choicesc)
    					highlightc = 1;
    				else
    					++highlightc;
    				break;
    			case 10:
    				choicec = highlightc;
    				break;
    			default:
    				//mvprintw(24, 0, "Character pressed is = %3d Hopefully it can be printed as '%c'", c, c);
    				refresh();
    				break;
    		}
        print_menuc(test_win, highlightc);
        // if (choicec == 2) //Legacy Decode to single wav file is disabled
        // {
        //   char * timestr = getTime();
        //   char * datestr = getDate();
        //   sprintf (opts->wav_out_file, "%s %s DSD-FME-DECODED.wav", datestr, timestr);
        //   if (timestr != NULL)
        //   {
        //     free (timestr);
        //     timestr = NULL;
        //   }
        //   if (datestr != NULL)
        //   {
        //     free (datestr);
        //     datestr = NULL;
        //   }
        //   openWavOutFile (opts, state);
        // }
        if (choicec == 3)
        {
          //read in filename for symbol capture bin
          entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter FME Symbol Capture Bin Filename");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%s", opts->symbol_out_file); //&opts->symbol_out_file
          noecho();

          openSymbolOutFile (opts, state);
        }
        if (choicec == 4)
        {
          //toggle all mutes
          if (opts->unmute_encrypted_p25 == 0)
          {
            opts->unmute_encrypted_p25 = 1;
          }
          else opts->unmute_encrypted_p25 = 0;

          if (opts->dmr_mute_encL == 0)
          {
            opts->dmr_mute_encL = 1;
          }
          else opts->dmr_mute_encL = 0;

          if (opts->dmr_mute_encR == 0)
          {
            opts->dmr_mute_encR = 1;
          }
          else opts->dmr_mute_encR = 0;

        }
        if (choicec == 5)
        {
          char wav_file_directory[1024];
          sprintf (wav_file_directory, "%s", opts->wav_out_dir);
          wav_file_directory[1023] = '\0';
          if (stat(wav_file_directory, &st_wav) == -1)
          {
            fprintf (stderr, "%s wav file directory does not exist\n", wav_file_directory);
            fprintf (stderr, "Creating directory %s to save decoded wav files\n", wav_file_directory);
            mkdir(wav_file_directory, 0700);
          }
          fprintf (stderr,"\n Per Call Wav File Enabled to Directory: %s;.\n", opts->wav_out_dir);
          srand(time(NULL)); //seed random for filenames (so two filenames aren't the exact same datetime string on initailization)
          opts->wav_out_f  = open_wav_file(opts->wav_out_dir, opts->wav_out_file, 8000, 0);
          opts->wav_out_fR = open_wav_file(opts->wav_out_dir, opts->wav_out_fileR, 8000, 0);
          opts->dmr_stereo_wav = 1;

        }

        if (choicec == 6)
        {
          int confirm = 0;

          #ifdef USE_RTLSDR

          entry_win = newwin(6, WIDTH+6, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL Device Index or Serial Number:");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_dev_index);
          noecho();

          entry_win = newwin(6, WIDTH+6, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL Device PPM Error:");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtlsdr_ppm_error);
          noecho();

          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Frequency in Hz (851.8 MHz is 851800000 Hz): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtlsdr_center_freq);
          noecho();

          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " BW (8, 12, 24, 48)(12 Recommended): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_bandwidth);
          noecho();

          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL Gain Value (0-49) (0 = AGC): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_gain_value);
          noecho();

          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL UDP Port - Legacy/Optional (Default = 0): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_udp_port);
          noecho();

          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL Vol Multiplier (1,2,3) (Default = 1): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_volume_multiplier);
          if (opts->rtl_volume_multiplier > 3 || opts->rtl_volume_multiplier < 0)
            opts->rtl_volume_multiplier = 1;
          noecho();

          entry_win = newwin(8, WIDTH+22, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " RTL RMS Squelch Level (NXDN/dPMR/Analog/Raw only): ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtl_squelch_level);
          noecho();

          //use to list out all detected RTL dongles
          device_count = rtlsdr_get_device_count();
          if (!device_count)
          {
            fprintf(stderr, "No supported devices found.\n");
          }
          else fprintf(stderr, "Found %d device(s):\n", device_count);
          for (int i = 0; i < device_count; i++)
          {
            rtlsdr_get_device_usb_strings(i, vendor, product, serial);
            fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);

            sprintf (userdev, "%08d", opts->rtl_dev_index);

            //check by index first, then by serial
            if (opts->rtl_dev_index == i)
            {
              fprintf (stderr, "Selected Device #%d with Serial Number: %s \n", i, serial);
              break;
            }
            else if (strcmp (userdev, serial) == 0)
            {
              fprintf (stderr, "Selected Device #%d with Serial Number: %s \n", i, serial);
              opts->rtl_dev_index = i;
              break;
            }

          }

          entry_win = newwin(18, WIDTH+20, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Starting RTL Input. Cannot Release/Stop Until Exit.");
          mvwprintw(entry_win, 4, 2, " RTL Frequency: %d Hz", opts->rtlsdr_center_freq);
          mvwprintw(entry_win, 5, 2, " RTL Device Index Number: %d; SN:%s", opts->rtl_dev_index, serial);
          mvwprintw(entry_win, 6, 2, " RTL Device Bandwidth: %d kHz", opts->rtl_bandwidth);
          mvwprintw(entry_win, 7, 2, " RTL Device Gain: %d", opts->rtl_gain_value);
          mvwprintw(entry_win, 8, 2, " RTL Volume Multiplier: %d", opts->rtl_volume_multiplier);
          mvwprintw(entry_win, 9, 2, " RTL Device UDP Port: %d", opts->rtl_udp_port);
          mvwprintw(entry_win, 10, 2, " RTL Device PPM: %d", opts->rtlsdr_ppm_error);
          mvwprintw(entry_win, 11, 2, " RTL RMS Squelch: %d", opts->rtl_squelch_level);
          mvwprintw(entry_win, 13, 2, " Are You Sure?");
          mvwprintw(entry_win, 14, 2, " 1 = Yes, 2 = No ");
          mvwprintw(entry_win, 15, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &confirm);
          noecho();

          refresh();

          //works well, but can't release dongle later, so its a one way trip until exit
          //TODO: Write function to release the dongle -- call cleanup_rtlsdr_stream()
          if (confirm == 1)
          {
            opts->audio_in_type = 3; //RTL input, only set this on confirm
            choicec = 18; //exit to decoder screen only if confirmed, otherwise, just go back
          }

          #else
          UNUSED(confirm);

          #endif

        }

        if (choicec == 7) //RTL DEV Retune
        {
          //read in new rtl frequency
          #ifdef USE_RTLSDR
          entry_win = newwin(6, WIDTH+18, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter Frequency in Hz (851.8 MHz is 851800000 Hz) ");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rtlsdr_center_freq);
          noecho();

          //retune dongle if frequency is not zero
          if (opts->rtlsdr_center_freq != 0)
            rtl_dev_tune (opts, opts->rtlsdr_center_freq);
          #endif
          choicec = 18;
        }

        if (choicec == 8)
        {
          if (state->rf_mod == 0)
          {
            state->rf_mod = 1;
            state->samplesPerSymbol = 8;
            state->symbolCenter = 3;
            opts->mod_c4fm = 0;
            opts->mod_qpsk = 1;
          }
          else
          {
            state->rf_mod = 0;
            state->samplesPerSymbol = 10;
            state->symbolCenter = 4;
            opts->mod_c4fm = 1;
            opts->mod_qpsk = 0;
          }
        }

        if (choicec == 9)
        {
          if (state->rf_mod == 0)
          {
            state->rf_mod = 1;
            state->samplesPerSymbol = 10;
            state->symbolCenter = 4;
            opts->mod_c4fm = 0;
            opts->mod_qpsk = 1;
          }
          else
          {
            state->rf_mod = 0;
            state->samplesPerSymbol = 10;
            state->symbolCenter = 4;
            opts->mod_c4fm = 1;
            opts->mod_qpsk = 0;
          }
        }

        // if (choicec == 9)
        // {
        //   if (opts->audio_out == 0)
        //   {
        //     opts->audio_out = 1;
        //     opts->audio_out_type = 0;
        //     // state->audio_out_buf_p = 0;
        //     // state->audio_out_buf_pR = 0;
        //     state->audio_out_idx = 0;
        //     state->audio_out_idx2 = 0;
        //     state->audio_out_idxR = 0;
        //     state->audio_out_idx2R = 0;
        //   }
        //   else
        //   {
        //     opts->audio_out = 0;
        //     opts->audio_out_type = 9;
        //     // state->audio_out_buf_p = 0;
        //     // state->audio_out_buf_pR = 0;
        //     state->audio_out_idx = 0;
        //     state->audio_out_idx2 = 0;
        //     state->audio_out_idxR = 0;
        //     state->audio_out_idx2R = 0;
        //   }
        // }

        //TCP Audio Input
        if (choicec == 10)
        {
          //read in tcp hostname
          sprintf (opts->tcp_hostname, "%s", "localhost");
          opts->tcp_portno = 7355;

          entry_win = newwin(8, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter TCP Direct Link Hostname:");
          mvwprintw(entry_win, 3, 2, "  (default is localhost)");
          mvwprintw(entry_win, 4, 2, "                        ");
          mvwprintw(entry_win, 5, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%s", opts->tcp_hostname);
          noecho();

          //read in tcp port number
          entry_win = newwin(8, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter TCP Direct Link Port Number:");
          mvwprintw(entry_win, 3, 2, "  (default is 7355)");
          mvwprintw(entry_win, 4, 2, "                        ");
          mvwprintw(entry_win, 5, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->tcp_portno);
          noecho();

          opts->tcp_sockfd = Connect(opts->tcp_hostname, opts->tcp_portno);
          if (opts->tcp_sockfd == 0) goto TCP_END;
          //successful connection, continue;

          opts->audio_in_type = 8;
          opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
          opts->audio_in_file_info->samplerate=opts->wav_sample_rate;
          opts->audio_in_file_info->channels=1;
          opts->audio_in_file_info->seekable=0;
          opts->audio_in_file_info->format=SF_FORMAT_RAW|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;
          opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);

          if(opts->tcp_file_in == NULL)
          {
            fprintf(stderr, "Error, couldn't open TCP with libsndfile: %s\n", sf_strerror(NULL));
            if (opts->audio_out_type == 0)
            {
              sprintf (opts->audio_in_dev, "%s", "pulse");
              opts->audio_in_type = 0;
            }
            else opts->audio_in_type = 5;

          }



          TCP_END: ; //do nothing

        }

        //RIGCTL
        if (choicec == 11)
        {
          //read in tcp hostname
          sprintf (opts->rigctlhostname, "%s", "localhost");
          opts->rigctlportno = 4532;

          entry_win = newwin(8, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter RIGCTL Hostname:");
          mvwprintw(entry_win, 3, 2, "  (default is localhost)");
          mvwprintw(entry_win, 4, 2, "                        ");
          mvwprintw(entry_win, 5, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%s", opts->rigctlhostname);
          noecho();

          //read in tcp port number
          entry_win = newwin(8, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter RIGCTL Port Number:");
          mvwprintw(entry_win, 3, 2, "  (default is 4532)");
          mvwprintw(entry_win, 4, 2, "                        ");
          mvwprintw(entry_win, 5, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%d", &opts->rigctlportno);
          noecho();

          opts->rigctl_sockfd = Connect(opts->rigctlhostname, opts->rigctlportno);
          if (opts->rigctl_sockfd != 0) opts->use_rigctl = 1;
          else opts->use_rigctl = 0;
        }

        // if (choicec == 10)
        // {
        //   if (opts->ncurses_compact == 0)
        //   {
        //     opts->ncurses_compact = 1;
        //   }
        //   else opts->ncurses_compact = 0;

        // }

        // if (choicec == 11)
        // {
        //   if (opts->ncurses_history == 0)
        //   {
        //     opts->ncurses_history = 1;
        //   }
        //   else opts->ncurses_history = 0;
        // }

        if (choicec == 12)
        {
          //TODO: Add Closing of RAW files as well?
          opts->wav_out_f = close_and_rename_wav_file(opts->wav_out_f, opts->wav_out_file, opts->wav_out_dir, &state->event_history_s[0]);
          opts->wav_out_fR = close_and_rename_wav_file(opts->wav_out_fR, opts->wav_out_fileR, opts->wav_out_dir, &state->event_history_s[1]);
          opts->wav_out_file[0] = 0; //Bugfix for decoded wav file display after disabling
          opts->wav_out_fileR[0] = 0;
          opts->dmr_stereo_wav = 0;
        }

        if (choicec == 13) //lucky number 13 for OP25 Symbol Capture Bin Files
        {
          //read in filename for symbol capture bin
          entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
          box (entry_win, 0, 0);
          mvwprintw(entry_win, 2, 2, " Enter OP25 Symbol Capture Bin Filename");
          mvwprintw(entry_win, 3, 3, " ");
          echo();
          refresh();
          wscanw(entry_win, "%s", opts->audio_in_dev); //&opts->audio_in_dev
          noecho();
          //do the thing with the thing
          struct stat stat_buf;
          if (stat(opts->audio_in_dev, &stat_buf) != 0)
          {
            fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
            goto SKIP;
          }
          if (S_ISREG(stat_buf.st_mode))
          {
            opts->symbolfile = fopen(opts->audio_in_dev, "r");
            opts->audio_in_type = 4; //symbol capture bin files
          }
          SKIP:
          choicec = 18;
        }

        if (choicec == 14) //replay last file
        {
          struct stat stat_buf;
          if (stat(opts->audio_in_dev, &stat_buf) != 0)
          {
            fprintf (stderr,"Error, couldn't open %s\n", opts->audio_in_dev);
            goto SKIPR;
          }
          if (S_ISREG(stat_buf.st_mode))
          {
            opts->symbolfile = fopen(opts->audio_in_dev, "r");
            opts->audio_in_type = 4; //symbol capture bin files
          }
          SKIPR:
          choicec = 18; //exit
        }

        if (choicec == 15) //stop/close last file PLAYBACK
        {
          if (opts->symbolfile != NULL)
          {
            if (opts->audio_in_type == 4) //check first, or issuing a second fclose will crash the SOFTWARE
            {
              fclose(opts->symbolfile); //free(): double free detected in tcache 2 (this is a new one) happens when closing more than once

            }

          }
          if (opts->audio_out_type == 0) opts->audio_in_type = 0; //set after closing to prevent above crash condition
          else (opts->audio_in_type = 5);

          choicec = 18; //exit
        }

        if (choicec == 16) //stop/close last file RECORDING
        {
          if (opts->symbol_out_f)
          {
            closeSymbolOutFile (opts, state);
            sprintf (opts->audio_in_dev, "%s", opts->symbol_out_file); //swap output bin filename to input for quick replay
          }
          choicec = 18; //exit
        }

        //toggle call alert beep
        if (choicec == 17)
        {
          if (opts->call_alert == 0)
          {
            opts->call_alert = 1;
          }
          else opts->call_alert = 0;
        }

        if (choicec != 0 && choicec != 18 ) //return to last menu
        {
          //return
          choice = 0;
          choicec = 0;

          keypad(test_win, FALSE);
          keypad(menu_win, TRUE);
          delwin(test_win);
          print_menu(menu_win, highlight);
          //wrefresh(menu_win);
          break;
        }

        if (choicec == 18) //exit both menus
        {
          //exit
          choice = 1;
          choicec = 0;

          keypad(test_win, FALSE);
          keypad(menu_win, TRUE);
          delwin(test_win);
          print_menu(menu_win, highlight);
          break;
        }
      }
      clrtoeol(); //clear to end of line?
      refresh();
    }

    //Key Entry
    if (choice == 14)
    {
      state->payload_keyid = 0;
      state->payload_keyidR = 0;
      short int option = 0;
      entry_win = newwin(16, WIDTH+6, starty+10, startx+10);
      box (entry_win, 0, 0);
      mvwprintw(entry_win, 2, 2, "Key Type Selection");
      mvwprintw(entry_win, 3, 2, " ");
      mvwprintw(entry_win, 4, 2, "1 -  Basic Privacy ");
      mvwprintw(entry_win, 5, 2, "2 - Hytera Privacy ");
      mvwprintw(entry_win, 6, 2, "3 - NXDN/dPMR Scrambler ");
      mvwprintw(entry_win, 7, 2, "4 - Force BP/Scr Key Priority ");
      mvwprintw(entry_win, 8, 3, "-------------------------------- ");
      mvwprintw(entry_win, 9, 2, "5 - RC4 or DES Key ");
      // mvwprintw(entry_win, 10, 2, "6 - Force DMR RC4 Priority/LE ");
      mvwprintw(entry_win, 10, 2, "6 - AES-128 or AES-256 Key ");
      mvwprintw(entry_win, 11, 3, "-------------------------------- ");
      mvwprintw(entry_win, 14, 3, " ");
      echo();
      refresh();
      wscanw(entry_win, "%hd", &option); //%d
      noecho();
      opts->dmr_mute_encL = 0;
      opts->dmr_mute_encR = 0;
      if (option == 1)
      {
        state->K = 0;
        entry_win = newwin(6, WIDTH+6, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, "Basic Privacy Key Number (DEC):");
        mvwprintw(entry_win, 3, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%lld", &state->K);
        noecho();
        if (state->K > 255) state->K = 255;

        state->keyloader = 0; //turn off keyloader
      }
      if (option == 2)
      {
        state->K1 = 0;
        state->K2 = 0;
        state->K3 = 0;
        state->K4 = 0;
        state->H = 0;

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value (HEX) ");
        mvwprintw(entry_win, 3, 2, " 10 Char or First 16 for 32/64");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->H);
        noecho();
        state->K1 = state->H;

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 2 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Second 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K2);
        noecho();

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 3 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Third 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K3);
        noecho();

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 4 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Fourth 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K4);
        noecho();

        state->keyloader = 0; //turn off keyloader
      }
      if (option == 3)
      {
        state->R = 0;
        entry_win = newwin(6, WIDTH+6, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, "NXDN/dPMR Scrambler Key Value (DEC):");
        mvwprintw(entry_win, 3, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%lld", &state->R);
        noecho();
        if (state->R > 0x7FFF) state->R = 0x7FFF;

        state->keyloader = 0; //turn off keyloader
      }
      //toggle enforcement of basic privacy key over enc bit set on traffic
      if (option == 4)
      {
        if (state->M == 1 || state->M == 0x21)
        {
          state->M = 0;
        }
        else state->M = 1;
      }

      if (option == 5)
      {
        state->R = 0;
        entry_win = newwin(6, WIDTH+6, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, "RC4/DES Key Value (HEX):");
        mvwprintw(entry_win, 3, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->R);
        noecho();
        //disable size check for DES keys
        // if (state->R > 0xFFFFFFFFFF)
        // {
        //   state->R = 0xFFFFFFFFFF;
        // }
        state->RR = state->R; //assign for both slots
        state->keyloader = 0; //turn off keyloader
      }

      //toggle enforcement of rc4 key over missing pi header/le on DMR
      // if (option == 6)
      // {
      //   if (state->M == 1 || state->M == 0x21)
      //   {
      //     state->M = 0;
      //   }
      //   else state->M = 0x21;
      // }

      //load AES keys
      if (option == 6)
      {
        state->K1 = 0;
        state->K2 = 0;
        state->K3 = 0;
        state->K4 = 0;
        state->H = 0;

        memset (state->A1, 0, sizeof(state->A1));
        memset (state->A2, 0, sizeof(state->A2));
        memset (state->A3, 0, sizeof(state->A3));
        memset (state->A4, 0, sizeof(state->A4));

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter AES128/256 Key Value (HEX) ");
        mvwprintw(entry_win, 3, 2, " First 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K1);
        noecho();

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        // mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 2 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Second 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K2);
        noecho();

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        // mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 3 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Third 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K3);
        noecho();

        entry_win = newwin(7, WIDTH+8, starty+10, startx+10);
        box (entry_win, 0, 0);
        // mvwprintw(entry_win, 2, 2, " Enter **tera Privacy Key Value 4 (HEX) ");
        mvwprintw(entry_win, 3, 2, " Fourth 16 Chars or Zero");
        mvwprintw(entry_win, 4, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%llX", &state->K4);
        noecho();

        //load the AES keys into a seperate handler
        state->A1[0] = state->A1[1] = state->K1;
        state->A2[0] = state->A2[1] = state->K2;
        state->A3[0] = state->A3[1] = state->K3;
        state->A4[0] = state->A4[1] = state->K4;

        //zero out the K1-K4
        state->K1 = 0;
        state->K2 = 0;
        state->K3 = 0;
        state->K4 = 0;

        state->keyloader = 0; //turn off keyloader

        //check to see if there is a value loaded or not
        if (state->A1[0] == 0 && state->A2[0] == 0 && state->A3[0] == 0 && state->A4[0] == 0)
          state->aes_key_loaded[0] = 0;
        else state->aes_key_loaded[0] = 1;

        //mirror
        state->aes_key_loaded[1] = state->aes_key_loaded[0];
      }

      if (state->K == 0 && state->K1 == 0 && state->K2 == 0 && state->K3 == 0 && state->K4 == 0)
      {
        opts->dmr_mute_encL = 1;
        opts->dmr_mute_encR = 1;
      }

      break;
    }

    if (choice == 99) //UNUSED
    {
      //setup Auto parameters--default ones
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      sprintf (opts->output_name, "Legacy Auto");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 1;
      opts->frame_x2tdma = 1;
      opts->frame_p25p1 = 1;
      opts->frame_p25p2 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 1;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 1;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      opts->unmute_encrypted_p25 = 0;
      break;

    }
    if (choice == 6)
    {
      //ProVoice Specifics
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 5;
      state->symbolCenter = 2;
      sprintf (opts->output_name, "EDACS/PV");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_p25p2 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 1;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 0;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 1;
      state->rf_mod = 2;
      break;
    }
    if (choice == 4)
    {
      //DSTAR
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      sprintf (opts->output_name, "DSTAR");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 1;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_p25p2 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      // state->rf_mod = 2;
      opts->unmute_encrypted_p25 = 0;
      break;
    }
    if (choice == 3) //was 5, changed to 3 and also made it the TDMA class
    {
      //TDMA -- was P25p1 only
      // resetState (state); //use sparingly, may cause memory leak
      // opts->use_heuristics = 1; //Causes issues with Voice Wide
      if (opts->use_heuristics == 1)
      {
        initialize_p25_heuristics(&state->p25_heuristics);
        initialize_p25_heuristics(&state->inv_p25_heuristics);
      }
      opts->frame_p25p1 = 1;
      opts->frame_p25p2 = 1;
      opts->frame_dmr = 1;
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      state->rf_mod = 0;
      sprintf (opts->output_name, "TDMA");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 1; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 2;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      opts->unmute_encrypted_p25 = 0;
      break;
    }

    if (choice == 2)
    {
      //AUTO Stereo P25 1, 2, and DMR
      // resetState (state); //use sparingly, seems to cause issue when switching back to other formats
      if (opts->use_heuristics == 1)
      {
        initialize_p25_heuristics(&state->p25_heuristics);
        initialize_p25_heuristics(&state->inv_p25_heuristics);
      }
      // opts->use_heuristics = 0;
      opts->frame_dmr = 1;
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      state->rf_mod = 0;
      sprintf (opts->output_name, "AUTO");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 1; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 2;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 1;
      opts->frame_p25p2 = 1;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 1;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      break;
    }
    if (choice == 7)
    {
      // resetState (state); //use sparingly, seems to cause issue when switching back to other formats
      state->samplesPerSymbol = 8;
      state->symbolCenter = 3;
      sprintf (opts->output_name, "P25p2");
      opts->dmr_mono = 0;
      opts->dmr_stereo = 1; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 2;
      opts->frame_dmr = 0;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_p25p2 = 1;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      break;
    }
    if (choice == 8)
    {
      //dPMR
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 20;
      state->symbolCenter = 10;
      sprintf (opts->output_name, "dPMR");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 1;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      //opts->unmute_encrypted_p25 = 0;
      break;
    }
    if (choice == 9)
    {
      //NXDN48
      // resetState (state); //use sparingly, may cause memory leak
      opts->frame_nxdn48 = 1;
      state->samplesPerSymbol = 20;
      state->symbolCenter = 10;
      state->rf_mod = 0;
      sprintf (opts->output_name, "NXDN48");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 1;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      //opts->unmute_encrypted_p25 = 0;
      // opts->mod_qpsk = 0;
      // opts->mod_gfsk = 0;
      // state->rf_mod = 0;
    }
    if (choice == 10)
    {
      //NXDN96
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      sprintf (opts->output_name, "NXDN96");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 1;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      //opts->unmute_encrypted_p25 = 0;
    }
    if (choice == 11)
    {
      //Decode DMR Stereo (was X2-TDMA)
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      // sprintf (opts->output_name, "X2-TDMA");
      sprintf (opts->output_name, "DMR");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 1; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 2;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 1;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
      //opts->unmute_encrypted_p25 = 0;
    }
    if (choice == 12)
    {
      //Decode YSF Fusion
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      sprintf (opts->output_name, "YSF");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 1;
      opts->frame_m17 = 0;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
    }
    if (choice == 5) //was 3
    {
      //Decode M17
      // resetState (state); //use sparingly, may cause memory leak
      state->samplesPerSymbol = 10;
      state->symbolCenter = 4;
      sprintf (opts->output_name, "M17");
      opts->dmr_mono = 0;
      opts->dmr_stereo  = 0; //this value is the end user option
      state->dmr_stereo = 0; //this values toggles on and off depending on voice or data handling
      opts->pulse_digi_rate_out = 8000;
      opts->pulse_digi_out_channels = 1;
      opts->frame_dstar = 0;
      opts->frame_x2tdma = 0;
      opts->frame_p25p1 = 0;
      opts->frame_nxdn48 = 0;
      opts->frame_nxdn96 = 0;
      opts->frame_dmr = 0;
      opts->frame_dpmr = 0;
      opts->frame_provoice = 0;
      opts->frame_ysf = 0;
      opts->frame_m17 = 1;
      opts->mod_c4fm = 1;
      opts->mod_qpsk = 0;
      opts->mod_gfsk = 0;
      state->rf_mod = 0;
    }
    if (choice == 13)
    {
      //Set all signal for inversion or uninversion
      if (opts->inverted_dmr == 0)
      {
        opts->inverted_dmr = 1;
        opts->inverted_dpmr = 1;
        opts->inverted_x2tdma = 1;
        opts->inverted_ysf = 1;
        opts->inverted_m17 = 1;
      }
      else
      {
        opts->inverted_dmr = 0;
        opts->inverted_dpmr = 0;
        opts->inverted_x2tdma = 0;
        opts->inverted_ysf = 0;
        opts->inverted_m17 = 0;
      }

    }
    if (choice == 15) //reset event history
    {
      //initialize event history items (0 to 255)
      for (uint8_t i = 0; i < 2; i++)
        init_event_history(&state->event_history_s[i], 0, 255);
    }

    if (choice == 16) //toggle payload printing
    {
      if (opts->payload == 0)
      {
        opts->payload = 1;
        fprintf(stderr, "Payload on\n");
      }
      else
      {
        opts->payload = 0;
        fprintf(stderr, "Payload Off\n");
      }
    }
    if (choice == 17)
    {
      //hardset P2 WACN, SYSID, and NAC
      entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
      box (entry_win, 0, 0);
      mvwprintw(entry_win, 2, 2, " Enter Phase 2 WACN (HEX)");
      mvwprintw(entry_win, 3, 3, " ");
      echo();
      refresh();
      wscanw(entry_win, "%llX", &state->p2_wacn); //%X
      if (state->p2_wacn > 0xFFFFF)
      {
        state->p2_wacn = 0xFFFFF;
      }
      noecho();

      entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
      box (entry_win, 0, 0);
      mvwprintw(entry_win, 2, 2, " Enter Phase 2 SYSID (HEX)");
      mvwprintw(entry_win, 3, 3, " ");
      echo();
      refresh();
      wscanw(entry_win, "%llX", &state->p2_sysid); //%X
      if (state->p2_sysid > 0xFFF)
      {
        state->p2_sysid = 0xFFF;
      }
      noecho();

      entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
      box (entry_win, 0, 0);
      mvwprintw(entry_win, 2, 2, " Enter Phase 2 NAC/CC (HEX)");
      mvwprintw(entry_win, 3, 3, " ");
      echo();
      refresh();
      wscanw(entry_win, "%llX", &state->p2_cc); //%X
      if (state->p2_cc > 0xFFF)
      {
        state->p2_cc = 0xFFF;
      }
      noecho();

      //need handling to truncate larger than expected values

      //set our hardset flag to 1 if values inserted, else set to 0 so we can attempt to get them from TSBK/LCCH
      if (state->p2_wacn != 0 && state->p2_sysid != 0 && state->p2_cc != 0)
      {
        state->p2_hardset = 1;
      }
      else state->p2_hardset = 0;


    }
    if (choice == 19)
    {
      short int lrrpchoice = 0;
      entry_win = newwin(10, WIDTH+16, starty+10, startx+10);
      box (entry_win, 0, 0);
      mvwprintw(entry_win, 2, 2, " Enable or Disable LRRP Data File");
      mvwprintw(entry_win, 3, 2, " 1 - ~/lrrp.txt (QGis)");
      mvwprintw(entry_win, 4, 2, " 2 - ./DSDPlus.LRRP (LRRP.exe)");
      mvwprintw(entry_win, 5, 2, " 3 - ./Custom Filename");
      mvwprintw(entry_win, 6, 2, " 4 - Cancel/Stop");
      mvwprintw(entry_win, 7, 2, " ");
      mvwprintw(entry_win, 8, 2, " ");
      echo();
      refresh();
      wscanw(entry_win, "%hd", &lrrpchoice); //%d
      noecho();

      if (lrrpchoice == 1)
      {
        //find user home directory and append directory and filename.
        char * filename = "/lrrp.txt";
        char * home_dir = getenv("HOME");
        char * filepath = malloc(strlen(home_dir) + strlen(filename) + 1);
        strncpy (filepath, home_dir, strlen(home_dir) + 1);
        strncat (filepath, filename, strlen(filename) + 1);
        //assign home directory/filename to lrrp_out_file
        sprintf (opts->lrrp_out_file, "%s", filepath); //double check make sure this still works
        opts->lrrp_file_output = 1;
      }

      else if (lrrpchoice == 2)
      {
        sprintf (opts->lrrp_out_file, "DSDPlus.LRRP");
        opts->lrrp_file_output = 1;
      }

      else if (lrrpchoice == 3)
      {
        //read in filename for symbol capture bin
        opts->lrrp_out_file[0] = 0;
        entry_win = newwin(6, WIDTH+16, starty+10, startx+10);
        box (entry_win, 0, 0);
        mvwprintw(entry_win, 2, 2, " Enter LRRP Data Filename");
        mvwprintw(entry_win, 3, 3, " ");
        echo();
        refresh();
        wscanw(entry_win, "%s", opts->lrrp_out_file); //&opts->lrrp_out_file
        noecho();
        if (opts->lrrp_out_file[0] != 0) //NULL
        {
          opts->lrrp_file_output = 1;
        }
        else
        {
          opts->lrrp_file_output = 0;
          sprintf (opts->lrrp_out_file, "%s", "");
          opts->lrrp_out_file[0] = 0;
        }
      }
      else
      {
        opts->lrrp_file_output = 0;
        sprintf (opts->lrrp_out_file, "%s", "");
        opts->lrrp_out_file[0] = 0;
      }
    }
    if (choice == 20)
    {
      exitflag = 1;
      break;
    }

		if(choice != 0 && choice != 20)	/* User did a choice come out of the infinite loop */
			break;

	}

	clrtoeol(); //clear to end of line?
	refresh();
  state->menuopen = 0; //flag the menu is closed, resume processing getFrameSync

  //reopen pulse output with new parameters, if not null output type
  if (opts->audio_out == 1 && opts->audio_out_type == 0)
  {
    openPulseOutput (opts);
  }

  if (opts->audio_out_type == 2 || opts->audio_out_type == 5)
  {
    openOSSOutput (opts);
  }


  if (opts->audio_in_type == 0) //reopen pulse input if it is the specified input method
  {
    openPulseInput(opts);
  }
  //fix location of this statement to inside the if statement and add conditions
  if (opts->audio_in_type == 3) //open rtl input if it is the specified input method
  {
    
    #ifdef USE_RTLSDR
    if (opts->rtl_started == 0)
    {
      opts->rtl_started = 1; //set here so ncurses terminal doesn't attempt to open it again
      open_rtlsdr_stream(opts);
    }
    rtl_clean_queue();
    reset_dibit_buffer(state); //test and observe for any random issues, disable if needed
    #elif AERO_BUILD
    opts->audio_out_type = 5; //hopefully the audio stream is still/already open //shouldn't this be 5? Was set to 3
    #else
    opts->audio_out_type = 0; //need to see if we need to open pulseoutput as well here?
    openPulseOutput (opts);
    #endif
  }

  if (opts->audio_in_type == 8) //re-open TCP input 'file'
  {
    opts->tcp_file_in = sf_open_fd(opts->tcp_sockfd, SFM_READ, opts->audio_in_file_info, 0);
  }


  //update sync time on cc sync so we don't immediately go CC hunting when exiting the menu
  state->last_cc_sync_time = time(NULL);

} //end Ncurses Menu
