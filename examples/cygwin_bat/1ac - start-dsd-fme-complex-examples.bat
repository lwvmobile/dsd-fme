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
@REM Important! Make sure to route audio in with volume mixer after opening!
@REM You may need to restart the software after setting it up to monitor input
@REM of VB-Cables, or allowing the network traffic for TCP or RIGCTL
@REM See the 'source code\examples\Audio_Plumbing.md' file for more information
@REM
@REM Notice: DSD-FME will not auto-detect NXDN48/96, Provoice, M17, or dPMR signals, 
@REM you will need to manually switch them on with the correct command.
@REM
@REM Notice: This .bat file contains numerous examples, it is recommended to find an
@REM an example that matches your use case, and use the @REM to disable the default
@REM or make your own .bat files with the examples provided here
@REM feel free to experiment and mix and match options, see what works best

@REM DMR with default /dev/dsp input and output, capture source to .wav file,
@REM per call wav files, ncurses terminal, payload dump to console
dsd-fme.exe -fs -6 dmrsource.wav -P -N -Z 2> log.ans

@REM NXDN48 with default /dev/dsp input and output, analog source audio monitor,
@REM per call wav files, ncurses terminal, payload dump to console
@REM dsd-fme.exe -fi -P -N -Z -8 2> log.ans

@REM P25 with pulse audio input and output, capture to .bin file,
@REM per call wav files, ncurses terminal, discard console log
@REM dsd-fme.exe -i pulse:input -o pulse:output -ft -P -N -c p25capture.bin 2> NUL

@REM P25 Phase 2 Traffic Channel Parking with SDR++ Network Sink input, pulse output,
@REM console payload dump, ncurses terminal, manually specify WACN/SYS/CC value
@REM dsd-fme.exe -f2 -i tcp:127.0.0.1:7355 -o pulse:output -X BEE00123ABC -Z -N 2> log.ans

@REM Auto Detect and Use 48k/1 .wav file for input (SDR++, DSD-FME)
@REM dsd-fme.exe -fa -i filename.wav -s 48000

@REM Auto Detect and Use 96k/1 .wav file for input (DSDPlus, GQRX)
@REM dsd-fme.exe -fa -i filename.wav -s 96000

@REM DMR/P25 TDMA and capture.bin format input (DSD-FME, OP25), payload to console
@REM dsd-fme.exe -ft -i capture.bin -Z

@REM -- Lima Zulu Specific Tweaks for NXDN48
@REM -- Run in With SDR++, TCP Network Audio Sink, RIGCTL on 4532, Scanner Mode, 
@REM    load keys from frequency during voice, force scrambler cipher
@REM dsd-fme-lz.exe -i tcp -U 4532 -Y -4 -t 1 -fi -Z -k examples\nxdn_sc_key.csv -C nxdn_chan_map.csv -N 2> log.ans

@REM -- Lima Zulu Specific Tweaks for NXDN96
@REM -- Run in With SDR++, TCP Network Audio Sink, RIGCTL on 4532, Scanner Mode, 
@REM    load keys from frequency during voice, force scrambler cipher
@REM dsd-fme-lz.exe -i tcp -U 4532 -Y -4 -t 1 -fn -Z -k examples\nxdn_sc_key.csv -C nxdn_chan_map.csv -N 2> log.ans

@REM ProVoice Conventional / Simplex from Disc Tap Input
@REM dsd-fme-pvc.exe -fp -Z


@REM PAUSE
