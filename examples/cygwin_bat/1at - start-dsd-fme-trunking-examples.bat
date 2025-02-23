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
@REM Consult examples\trunking.sh for more trunking examples, notes, and use case on tcp and rtl input switches
@REM Current Supported Trunking Types Include: 
@REM P25:   Phase 1 or Phase 2 (MAC_SIGNAL/TDMA) Trunking.
@REM DMR:   Cap+, XPT, Con+, T3 (with channel map import)
@REM NXDN:  Type C v1 with channel map import, v2 DFA, "Icom IDAS" Type D Trunking
@REM EDACS: Standard Network, and Extended Addressing (with and without ESK)
@REM 
@REM Note: Simulcast P25 systems may perform poorly or not at all on DSD-FME, even with QPSK enabled.

@REM P25 (Phase 1 Control) Trunking with SDR++ Network Sink input, RIGCTL, /dev/dsp output,
@REM per call wav files, ncurses terminal
@REM dsd-fme.exe -ft -i tcp:127.0.0.1:7355 -U 4532 -T -P -N 2> log.ans

@REM P25 (Phase 2 Control) Trunking with SDR++ Network Sink input, RIGCTL, /dev/dsp output,
@REM per call wav files, ncurses terminal
@REM dsd-fme.exe -f2 -i tcp:127.0.0.1:7355 -U 4532 -T -P -N 2> log.ans

@REM DMR Trunking with RTL input, /dev/dsp output, channel map import,
@REM per call wav files, ncurses terminal, payload dump to console
@REM dsd-fme.exe -fs -i rtl:0:450M:0:-2:12 -T -C examples\dmr_t3_chan.csv -P -N -Z 2> log.ans

@REM DMR Trunking with SDR++ input, RIGCTL, /dev/dsp output, RAS enabled CSBK (-F),
@REM channel map import, per call wav files, ncurses terminal, payload dump to console
@REM dsd-fme.exe -fs -F -i tcp -U 4532 -T -C examples\capacity_plus_chan.csv -P -N -Z 2> log.ans

@REM NXDN48 v2.0 DFA (Direct Frequency Assignment) Trunking with SDR++ input, RIGCTL, /dev/dsp output,
@REM scrambler key import (decimal), per call wav files, ncurses terminal, payload dump to console
@REM dsd-fme.exe -fi -i tcp -U 4532 -T -P -N -Z -k examples\nxdn_sc_key.csv 2> log.ans

@REM NXDN96 Trunking with SDR++ input, RIGCTL, /dev/dsp output,
@REM channel map import, per call wav files, ncurses terminal, payload dump to console
@REM dsd-fme.exe -fn -i tcp -U 4532 -T -C examples\nxdn_chan_map.csv -P -N -Z 2> log.ans

@REM EDACS Standard Network Analog and Digital
@REM dsd-fme.exe -fh -i tcp -U 4532 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans
@REM dsd-fme.exe -fh -i rtl:0:850M:44:-2:24 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans

@REM EDACS Extended Address Analog and Digital (with ESK)
@REM dsd-fme -fE -i tcp:192.168.7.5:7355 -U 4532 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans
@REM dsd-fme -fE -i rtl:0:850M:44:-2:24 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans


@REM PAUSE
