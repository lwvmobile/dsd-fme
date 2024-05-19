### Example Usage and Notes!

`dsd-fme` is all you need to run for pulse input, pulse output, and auto detect for DMR BS/MS, and P25. To use other decoding methods which cannot be auto detected, please use the following command line switches.

```
-ft XDMA decoder class (P25p1 and p2, YSF, and DMR Stereo)
-fs DMR Stereo, also may need to use -xr if inverted DMR
-fa Legacy Auto (not recommended)
-fi NXDN48
-fn NXDN96
-fp Provoice
-fh EDACS Standard / Network
-fH EDACS Standard / Network with ESK
-fe EDACS Extended Addresssing
-fE EDACS Extended Addressing with ESK
-fm dPMR, also may need to use -xd if inverted dPMR.
-f1 P25P1
-f2 P25P2 (may need to specify wacn/sys/nac manually if traffic channel)
-fx X2-TDMA
-fy YSF
-fz M17 Audio
```

## Conventional Frequency Scanner
This feature is used to allow DSD-FME to use RIGCTL or RTL input and cycle through frequencies to attain a frame sync. 

This works almost identically to how the trunkng methods work, but instead of locking onto a control channel and tuning based on its decoding, it tunes through all loaded frequencies in a CSV file and stops when it finds a frame sync. This method is meant for loading a bunch of conventional non-trunking frequencies that signal only when voice or short data bursts are present (DMR T2, Conventional P25 P1, Conventional NXDN, etc) and decode them until the frame sync stops and then resume its scan. This method uses SDR++ RIGCTL, or RTL internal handling to go through signal. -Y is the scan switch. Note: This method will NOT perform at a level similar to a real hardware scanner, SDR tuning is not nearly as fast or reliable at locking onto signal.

To scan through a mixture of DMR and P25 Conventional, use:
`dsd-fme -i tcp -C channel_map.csv -G group.csv -Y -U 4532 -N 2> log.ans` 

To scan through NXDN48 only conventional, use: 
`dsd-fme -fi -i tcp -C channel_map.csv -Y -U 4532 -N 2> log.ans` 

To scan through NXDN96 only conventional, use: 
`dsd-fme -fn -i tcp -C channel_map.csv -Y -U 4532 -N 2> log.ans`

The Channel Map setup will be identical to any of the channel maps found for trunking systems, channel numbers do not matter and can be arbitrarily organized, and frequencies can be duplicated to be 'scanned' more frequently in larger CSV files. Scan speed by default is 1 second, and is most stable at that speed. This can be adjusted by using the `-t 2` option, for example, to specify 2 seconds of hang time per frequency. ~~If activity occurs on a frequency, an additional 2 seconds of 'hangtime' will occur on that frequency for any follow up voice activity.~~ Using `-t 0` will forego the one second wait time, and this can work in some situations, but is not advised as it may skip during the middle of transmissions, or skip over them entirely without a framesync.

Please Note: DMR Simplex may not behave well on scan, I have not been able to test it as such, but given the nature of the on-off timeslot signalling on DMR Simplex, missed frame syncs and skipping/resuming scan may occur.

## Trunking Examples
Please see trunking.sh for sample start up scripts or scroll down to the bottom for more explanation.

## Example Usage - Default Decoder, Ncurses Terminal, Pulse Input/Output, and Log Console to file
`dsd-fme -N 2> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

### Audio Plumbing

Please see [Audio Plumbing](https://github.com/lwvmobile/dsd-fme/blob/aw_dev/examples/Audio_Plumbing.md "Audio Plumbing") for general help with audio plumbing (routing from A to B).

### Input ###

--TCP Direct Audio Link with SDR++

`dsd-fme -i tcp` Currently defaults to localhost:7355 (SDR++ defaults)

`dsd-fme -i tcp:192.168.7.5:7356` (remote host and custom port)

--48000/9600 Mono Wav File Input

`dsd-fme -i filename.wav` 48k/1 Audio (SDR++, GQRX wav recordings)

`dsd-fme -i filename.wav -s 96000` 96k/1 (DSDPlus Raw Signal Wav Files)

Wav File Input Note: Due to 96000 rate audio requiring me to double the symbol rate and center, be sure to use the -s 96000 at the very end of the startup command. Also, some NXDN48/96 may have difficulties decoding properly with wav file input. 

## Example Usage - RTL2832 Dongle Input
Short Setup (all default values):

`dsd-fme -i rtl`

Detailed Setup:

`dsd-fme -ft -i rtl:0:154.9875M:26:-2:8:0:6020 `

```
RTL-SDR options:
 Usage: rtl:dev:freq:gain:ppm:bw:sq:vol
  NOTE: all arguments after rtl are optional now for trunking, but user configuration is recommended
  dev  <num>    RTL-SDR Device Index Number
  freq <num>    RTL-SDR Frequency (851800000 or 851.8M) 
  gain <num>    RTL-SDR Device Gain (0-49)(default = 0; Hardware AGC recommended)
  ppm  <num>    RTL-SDR PPM Error (default = 0)
  bw   <num>    RTL-SDR Bandwidth kHz (default = 12)(8, 12, 16, 24)  
  sq   <num>    RTL-SDR Squelch Level vs RMS Value (Optional)
  vol  <num>    RTL-SDR Sample 'Volume' Multiplier (default = 2)(1,2,3)
 Example: dsd-fme -fs -i rtl -C cap_plus_channel.csv -T
 Example: dsd-fme -fp -i rtl:0:851.375M:22:-2:24:0:2

```

## Example Usage - Extra Payload/PDU Logging

`dsd-fme -Z -N 2> log.ans`

and in a second terminal tab, same folder, run

`tail -n 40 -f log.ans`

### EDACS/P25/NXDN/DMR Simple/Single VFO Trunking ###

EDACS Trunking (w/ channel map import)

--EDACS/PV Trunking using RIGCTL and TCP Direct Link Audio inside of SDR++ (see switches above for STD/NET, EA, and ESK modes)

`dsd-fme -i tcp -fE -C channel_map.csv -G group.csv -T -U 4532 -N 2> log.ans`

--NXDN48 Trunking (Direct Frequency Assignment) with SDR++ (untested for frequency accuracy)

`dsd-fme -fi -i tcp -T -U 4532 -N 2> log.ans`

--NXDN48 Trunking (w/ channel map import) with SDR++ (put control channel frequency at channel 0 in channel_map.csv)

`dsd-fme -fi -i tcp -T -U 4532 -C channel_map.csv -N 2> log.ans`

--DMR Trunking (w/ channel map import) with SDR++ (put control channel frequency at channel 0 (TIII and Con+) in channel_map.csv)

`dsd-fme -fs -i tcp -T -U 4532 -C channel_map.csv -N 2> log.ans`

--P25 Trunking P1 and P2 (C4FM) with SDR++ 

`dsd-fme -i tcp -T -U 4532 -N 2> log.ans`

--P25 Trunking (QPSK) with P1 Control Channel (Should switch symbol rate and center on Phase 2 audio channels)

`dsd-fme -i tcp -T -U 4532 -N -mq 2> log.ans`

--P25 Trunking Phase 2 TDMA Control Channel systems with QPSK (non Phase 1 systems)

`dsd-fme -i tcp -T -U 4532 -N -f2 -m2 2> log.ans`

Trunking Note1: All samples above can also be run with the RTL input method and setting of RTL UDP remote port. In terms of performance, however, SDR++ will do a much better job on weaker/marginal signal compared to the RTL input. RTL input should only be used on strong signal.

`dsd-fme -fp -i rtl:0:851.8M:22:-2:24:0:6020 -T -C channel_map.csv -G group.csv -N 2> log.ans`

Trunking Note2: QPSK Phase 1 and Phase 2 Systems may be subceptible to LSM distortion issues, but seem to do okay, but require really good signal. Some CRC issues still occur with Phase 2 TDMA LCCH Mac Signal that can affect reliability, I believe this issue is ultimately caused by the PSK demodulation inside of FME. I also don't believe this will work on 8-level PSK, but I cannot determine that at the moment. Update: I have improved the LCCH Mac Signal decoding my increasing the QPSK decision point buffers to their maximum values. 

Trunking Note3: DMR Trunking has been coded, and some testing and tweaks have been carried out. Cap+, Con+, and TIII systems seem to do well with trunking now. Placing the frequency for the control channel at channel map 0 in your channel_map.csv file is not required now if using RIGCTL or the RTL Input, both can poll the VFO for the current frequency if it believes its on a control channel, but setting a fake channel number (i.e. 999) first with the CC frequency will result in finding the CC faster on startup if desired. If you need to map out your channels for TIII, you can observe the console output and look for channel numbers. For conveniece I have included the DSDPlus style LSN-esque numbering into the console print so it will make it easier for users from DSDPlus to map frequencies into the channel_map.csv file. Make sure your channel numbers are the lpcn (12-bit logical physical channel number - decimal format) values from the log, and not the LPCN+TS (13-bit DSDPlus LSN) values. Notice: TIII Site ID value needs work to determine proper DMRLA values for system area and sub area.

```
00:43:00 Sync: +DMR  [slot1]  slot2  | Color Code=00 | CSBK
 Talkgroup Voice Channel Grant (TV_GRANT) - Logical
  LPCN: 0075; TS: 2; LPCN+TS: 0152; Target: 16518173 - Source: 16533625

```

Use channel 75 in your import file, which would correspond to dsdplus channels 151 and 152 (151 = TS1 / 152 = TS2).

For Connect Plus, enumerate your list from 1 to the last channel and add the frequency. For Capacity Plus, LSN 1 and 2 will share the same frequency, 3 and 4 will share, 5 and 6 will share, and 7-8 will share, as Capacity Plus counts each 'channel' as two seperate channels (LSN), one for each time slot. Capacity Plus Quirk: DSD-FME makes its best effort to follow the rest channel in the event that the sync is lost for longer than the hangtime, but occassionally, DSD-FME may lose the rest channel and will have to hunt through all frequencies to find it again.

Trunking Note4: NXDN Trunking v1 will require a channel map. Please see the example folder for an appropriate channel map. NOTICE: NXDN trunking has been fixed on the RTL (rtl_fm) input method and works correctly now using a soft squelch value to determine when to look for frame sync. Tests show this works very well, but squelch adjustments may be required to differentiate the noise floor from the signal.

NXDN Trunking Update: NXDN DFA (Direct Frequency Assignment) has been coded from the v2 documents, so using a channel map may not be required, as long as the DFA is configured with standard values, and not 'system definable' values. This will only work on trunking systems that use DFA (newer systems) in accordance to the v2 CAI document NXDN TS 1-A Version 2.0 September 2016. If the system uses 'system definable' values, then make sure to put the channels in a channel map csv file as as the 16-bit OFN values for proper tuning.

NXDN Type-D/IDAS Update: Type-D or IDAS decoding/trunking has been coded now and tested on a small two channel system. Set up a channel map much like Type-C trunking. If preferred, insert channel 31 at the top of your channel map for the 'home repeater' or go to channel of choice during a TX_REL or DISC message. Extensive testing of Type-D has not been conducted. Some cavaets may currently include iffy per call wav file saving and potentially shifting SRC and TGT ID values due to how SCCH messages are handled and how Busy Repeater Messages are handled. Private calls will need to be enabled by enabling 'data calls', if desired. During testing, only data calls came on private calls, so this was set to disabled by default, but can be enabled with data calls enabled. Also, note that some home repeater channels may or may not carry calls from all systems.

Channel Map and Group CSV Note: Leave the top line of the channel_map.csv and group.csv as the label, do not delete the line, if no line is there, dsd_import skips the first line so it will not import the first channel or first group in those files if there is something there that isn't a label. 

Hytera XPT: Code has been added for XPT system slco/flco/csbk decoding and trunking. The setup will be similar to Capacity Plus trunking in the csv file, listing each LSN channel to a frequency (see examples/hytera_xpt_chan.csv). Currently, this set up is working with smaller XPT systems that I have been able to test with.

## NCurses Keyboard Shortcuts ##

Some Keyboard Shortcuts have been implemented for testing to see how users like them. Just hope nobody pushes a key on accident and doesn't know which one it was or what it does. The current list of keyboard shortcuts include:

```
esc or arrow keys - ncurses menu
q - quit
c - toggle compact mode
h - toggle call history
z - toggle console payloads
a - toggle call alert beep
4 - force privacy/scrambler key assertion over enc identifiers (dmr and nxdn)
6 - force rc4 key assertion over missing pi header/le enc identifiers (dmr)
i - toggle signal inversion on types that can't auto detect (dmr, dpmr)
m - toggle c4fm/qpsk 10/4 (everything but phase 2 signal)
M - toggle c4fm/qpsk 8/3 (phase 2 tdma control channel)
R - start capturing symbol capture bin (date/time name file)
r - stop capturing symbol capture bin
spacebar - replay last symbol capture bin (captures must be stopped first)
s - stop playing symbol capture bin or wav input file (lower s)
P - start per call decoded wav files (Capital P)
p - stop per call decoded wav files (lower p)
t - toggle trunking (needs either rtl input, or rigctl connection)
y - toggle scanner (needs either rtl input, or rigctl connection)
1 - Toggle Voice Synthesis in TDMA Slot 1 or FDMA Conventional Voice
2 - Toggle Voice Synthesis in TDMA Slot 2
w - Toggle Trunking/Playback White List (Allow A Groups Only) / Black List (Block B or DE groups only) Mode
g - Toggle Trunking Tuning to Group Calls (DMR T3, Con+, Cap+, P25, NXDN)
u - Toggle Trunking Tuning to Private Calls (DMR T3, Cap+, P25)
d - Toggle Trunking Tuning to Data Calls (DMR T3, NXDN)
e - Toggle Trunking Tuning to Encrypted Calls (P25)
8 - Connect to SDR++ TCP Audio Sink with Defaults/Retry Sink Connection on Disconnect
9 - Connect to SDR++ RIGCTL Server with Defaults/Retry RIGCTL Connection
D - Reset DMR Site Parms/Call Strings, etc.
Z - Simulate NoCarrier/No VC/CC sync (capital Z)

! - Lockout Tuning/Playback of TG in Slot 1 or Conventional -- Current Session Only if no group.csv file specified
@ - Lockout Tuning/Playback of TG in Slot 2 -- Current Session Only if no group.csv file specified

k - Hold TG in Slot 1 or Conventional, or clear current hold
l - Hold TG in Slot 2 on TDMA Systems, or clear current hold

C - Drop Call and Return to CC during trunking operation
L - Manual Cycle Forward Channel Tuned when RIGCTL or using RTL input and channel csv loaded

S - Toggle between EDACS Standard/Network and Extended Addressing mode (Capital S)
A - Toggle EDACS ESK Mask (none vs A0)

H - Toggle High Pass Filter on Digital Sythesized Voice
V - Toggle Low  Pass Filter on Analog Voice or Raw Signal Monitoring
B - Toggle High Pass Filter on Analog Voice or Raw Signal Monitoring
N - Toggle Passband  Filter on Analog Voice or Raw Signal Monitoring

v - Cycle RTL Input Volume Multiplier 1X, 2X, 3X (lower v)

```

## Sending Audio to a Icecast 2 Server via FFmpeg (Windows)

Make sure to enable "Stereo Mix" from Volume Control Panel and then disable Windows Sounds.

```
ffmpeg -f dshow -i audio="Stereo Mix (Realtek High Definition Audio)" -c:a aac -b:a 64k -content_type 'audio/aac' -vn -f adts icecast://source:password@192.168.5.251:8844/DSDFME
```

## Sending Audio to a Icecast 2 Server via FFmpeg (Linux w/ Pulse Audio)

`pactl list short sources`

Choose Desired Device to Cast:

```
0 auto_null.monitor       module-null-sink.c      s16le 2ch 48000Hz       IDLE
1	alsa_output.pci-0000_0d_00.3.analog-stereo.monitor	module-alsa-card.c	s16le 2ch 44100Hz	RUNNING
2	alsa_input.pci-0000_0d_00.3.analog-stereo	module-alsa-card.c	s16le 2ch 44100Hz	RUNNING
3	virtual_sink.monitor	module-null-sink.c	s16le 2ch 44100Hz	RUNNING
4	virtual_sink2.monitor	module-null-sink.c	s16le 2ch 44100Hz	RUNNING
7	alsa_output.pci-0000_0b_00.1.hdmi-stereo-extra1.monitor	module-alsa-card.c	s16le 2ch 44100Hz	RUNNING
```

Example: 

```
francis@12cores:~$ pactl list short sources
0       auto_null.monitor       module-null-sink.c      s16le 2ch 48000Hz       IDLE
francis@12coresx:~$ ffmpeg -f pulse -i 0 -c:a libmp3lame -ab 64k -f mp3 icecast://source:password@XXXXXXXXX:8000


```

Note: Do not run this command for ffmpeg/icecast piped to the main dsd-fme command, make sure these are running in seperate terminals.

## Sending Audio to UDP Target/Port

Audio can be sent to a target address on your local area network with the UDP blaster using the `-o udp:targetaddress:port` option.
Note: Any analog audio monitoring will not function properly over UDP port (rate difference issues)

Example (Short 8k/2):
`dsd-fme -fs -o udp:192.168.7.8:23470`

Receiving End (Short 8k/2):
`socat stdio udp-listen:23470 | play --buffer 640 -q -b 16 -r 8000 -c2 -t s16 -`

Example (Short 8k/1):
`dsd-fme -f1 -o udp:192.168.7.8:23469`

Receiving End (Short 8k/1):
`socat stdio udp-listen:23469 | play --buffer 320 -q -b 16 -r 8000 -c1 -t s16 -`

Example (Float 8k/2):
`dsd-fme -ft -y -o udp:192.168.7.8:23468`

Receiving End (Float 8k/2):
`socat stdio udp-listen:23468 | play --buffer 1280 -q -b 16 -r 8000 -c2 -t f32 -`

Example (Float 8k/1):
`dsd-fme -fi -y -o udp:192.168.7.8:23467`

Receiving End (Float 8k/1):
`socat stdio udp-listen:23467 | play --buffer 640 -q -b 16 -r 8000 -c1 -t f32 -`

Note: --buffer option in play is calculated by (channels * samples * sizeof(samples)), 
where samples is 160 for mono, 320 for stereo, and short samples are 2 and float samples are 4.
This is vital to prevent 'lag' or cut-off audio when the UDP audio doesn't fill the default buffer size.

Note2: Be sure to start the receiving end AFTER starting DSD-FME. If DSD-FME is restarted, make sure to restart the receiving end as well, as socat/UDP closes the UDP port listening when DSD-FME closes the UDP socket.

Note3: socat version 1.8 may have a `E xioopen_ipdgram_listen(): unknown address family 0` problem, downgrading to 1.7.4 is recommended if this occurs. `sudo downgrade socat`.

## Record 48k/1 audio with rtl_fm

You can record suitable audio to feed as input to dsd-fme with `-i` using rtl_fm:

`rtl_fm -M nfm -f [FREQ] -s 48k -g 42 -l 20 > input.wav`

You can also combine that with `socat` to send the audio to a remote dsd-fme instance in real time while keeping a local copy on the host where the RTL-SDR dongle is:

`rtl_fm -M fm -f [FREQ] -s 48k -g 42 -l 20 | tee input.wav | socat -u - TCP-LISTEN:1234,forever,reuseaddr`

On the remote side where dsd-fme is, use the TCP input:

`dsd-fme -i tcp:192.168.7.8:1234 -N`

Note: As always, when using rtl_fm, play with `-l` and `-g` to find a good squelch and gain values for your environment - you will know if the squelch is the right value when `input.wav` does not increase in size in the absence of useful signal on the monitored frequency.

## Using the Experimental M17 Encoder (and Decoder)

You can use the experimental M17 Voice Stream and Packet encoder with the following examples.

Encode/Decode Local Feedback Voice Stream (Single Session):

`dsd-fme -fZ -i pulse -o pulse -M M17:3:SP5WWP:BROADCAST:48000`

Using the `-8` option (raw audio monitor) will instead send encoded raw audio to output instead of decoded audio. For example,

To send a voice stream out of stdout into SOCAT TCP to a second/remote DSD-FME session to decode:

Encode (Upper Z):

`dsd-fme -fZ -8 -i pulse -M M17:9:LWVMOBILE:DSD-FME:48000 -o - | socat stdio tcp-listen:7355,forever,reuseaddr`

Decode (Lower z): 

`dsd-fme -fz -i tcp:192.168.7.8:7355`

NOTE: When using tcp input for M17 decoding, DSD-FME will keep the TCP connection alive with constant retries, so no need to restart the decoding session on connection failure.

SMS Packet Encoder:

`dsd-fme -fP -S "This is a sample message sent over M17 Packet Data using DSD-FME to encode it."`

SMS Packet Encoder (over SOCAT TCP):

`dsd-fme -fP -8 -S "This is a sample message sent over M17 Packet Data using DSD-FME to encode it and SOCAT to send it over TCP." -o - | socat stdio tcp-listen:7355,forever,reuseaddr`

Many combinations of input and output can be used as well, and using features like raw audio saving with `-6 m17str.wav` will save a wav file of your encoded audio to be replayed later with `-i m17str.wav`
Also note, features function similarly, like -Z for payload, and input options include pulse, oss, stdin, rtl, tcp, etc, using those as the mic in for voice encoding. With the right setup of input/output/input using SOCAT TCP pipes, it is conceivable to use DSD-FME to convert analog audio from NFM into M17 audio, or even change another digital format supported by DSD-FME into M17 audio, if desired (purely for research and fun purposes). Ncurses terminal will function with internal encoder/decoder and wav file creation, with toggle encode/tx with `\` key, but will not function with stdout properly.

The `-M` switch is used to specify user M17 variables, the string is in the format of M17:CAN:SRC:DST:INPUT_RATE.
CAN 1-15; SRC and DST have to be no more than 9 UPPER base40 characters.
BASE40: '  ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.'
Input Rate Default is 48000; Use Multiples of 8000 up to 48000.
Values not entered into the M17: string are set to default values. M17:7:DSD-FME:DSD-FME:48000

Fun Example Transmuting OTA Analog Things into M17:

Start up in sequence.

Terminal #1 (RTL Input):
`dsd-fme -fZ -8 -M M17:1:ANALOG:M17:48000 -i rtl:0:162.425M -o - | socat stdio tcp-listen:7310,forever,reuseaddr`

Terminal #2:
`dsd-fme -fz -i tcp:127.0.0.1:7310`

Fun Example Transmuting OTA Digital Things into M17:

Start up all three of these in sequence.

Terminal #1 (connecting to SDR++):
`dsd-fme -fi -i tcp:127.0.0.1:7355 -o - | socat stdio tcp-listen:7300,forever,reuseaddr`

Terminal #2:
`dsd-fme -fZ -8 -M M17:2:NXDN:M17:8000 -i tcp:127.0.0.1:7300 -o - | socat stdio tcp-listen:7301,forever,reuseaddr`

Terminal #3:
`dsd-fme -fz -i tcp:127.0.0.1:7301`

Note: Transmuting only carries over voice when audio is output, and will not carry any ID values, or any other data payloads.
This mode is only meant for educational or fun purposes, please do not retransmit stuff that doesn't belong to you.

Information on this section will be updated as features are added or modified, etc. If you need help with a particular function and can't find a suitable answer, always open a new issue here, or make contact on the radio reference forum for additional examples, guidance, ideas, help, etc.