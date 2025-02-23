@REM Set the console / terminal to 90 columns, 50 lines
@REM mode con: cols=90 lines=50

@REM change directory into the dsd-fme folder
cd dsd-fme

@REM Digital Speech Decoder: Florida Man Edition
@REM DSD-FME Cygwin Windows Portable Builds
@REM
@REM The source code of this software can be donwloaded here:
@REM https://github.com/lwvmobile/dsd-fme
@REM
@REM If you paid for, or were otherwise fleeced for this software, DEMAND YOUR MONEY BACK!
@REM
@REM This software is designed to be used with an SDR receiver (like SDR++ or SDR#) with TCP input,
@REM RTL Input, media file playback over virtual cables or null-sinks, or a Discriminator Tap input
@REM
@REM The complete option list can be seen by entering the following command:
@REM dsd-fme.exe -h
@REM
@REM Enjoy !
@REM
@REM Notice: This .bat file contains numerous examples, it is recommended to find an
@REM an example that matches your use case, and use the @REM to disable the default
@REM or make your own .bat files with the examples provided here
@REM feel free to experiment and mix and match options, see what works best
@REM
@REM Scanning will not perform quickly like a real scanner, but can be used with any
@REM supported decoding type and analog as well, and will require a channel_map to scan frequencies
@REM channel numbers do not matter, but use unique ones for each frequeny
@REM -t 0 will scan as fast as possible, but may miss frame sync, -t 1 is stable, but needs 1 second between tuning

@REM P25/DMR/Analog Scanning with RTL input, /dev/dsp output, channel map import,
@REM per call wav files, ncurses terminal, payload dump to console, raw audio signal monitoring
@REM dsd-fme.exe -ft -i rtl:0:450M:0:-2:12 -Y -t 1 -C examples\dmr_t3_chan.csv -P -8 -N -Z 2> log.ans

@REM P25/DMR/Analog Scanning with SDR++ input (use squelch), RIGCTL, /dev/dsp output, channel map import,
@REM per call wav files, ncurses terminal, payload dump to console, raw audio signal monitoring
@REM dsd-fme.exe -ft -i tcp:127.0.0.1:7355 -U 4532 -Y -t 1 -C examples\dmr_t3_chan.csv -P -8 -N -Z 2> log.ans

@REM NXDN48/Analog Scanning with SDR++ input (use squelch), RIGCTL, /dev/dsp output, channel map import,
@REM per call wav files, ncurses terminal, payload dump to console, raw audio signal monitoring
@REM dsd-fme.exe -fi -i tcp:127.0.0.1:7355 -U 4532 -Y -t 1-C examples\nxdn_chan_map.csv -P -8 -N -Z 2> log.ans

@REM Analog Scanning with SDR++ input (use squelch), RIGCTL, /dev/dsp output, channel map import
@REM dsd-fme.exe -fA -i tcp:127.0.0.1:7355 -U 4532 -Y -t 0 -C examples\nxdn_chan_map.csv -8


@REM PAUSE
