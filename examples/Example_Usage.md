### Example Usage and Notes!

`dsd-fme` is all you need to run for pulse input, pulse output, and auto detect for DMR BS/MS, and P25 (1 and 2) . To use other decoding methods which cannot be auto detected, please use the following command line switches. Make sure to route audio into and out of DSD-FME using pavucontrol and virtual sinks as needed.

```
-ft XDMA decoder class (P25 1 and 2, and DMR Stereo)
-fs DMR Stereo, also may need to use -xr if inverted DMR
-fa Legacy Auto (not recommended)
-fi NXDN48
-fn NXDN96
-fp ProVoice
-fm dPMR, also may need to use -xd if inverted dPMR.
-fr DMR Mono, also may need to use -xr if inverted DMR.
-f1 P25P1
-f2 P25P2 (will need to specify wacn/sys/nac manually)
-fx X2-TDMA
```

## Trunking Examples
Please see trunking.sh for sample start up scripts or scroll down to the bottom for more explanation.

## Example Usage - Default Decoder, Ncurses Terminal, Pulse Input/Output, and Log Console to file
`dsd-fme -N 2> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

Then you can open up your pavucontrol "Pulse Audio Volume Control" or "Volume Control" application and route input into DSD-FME from any application and DSD-FME output to the left and right speakers respectively. (unlock the channel in the application stream and adjust left and right independently)

### Input ###

--TCP Direct Audio Link with SDR++

`dsd-fme -i tcp` Currently defaults to localhost:7355 (SDR++ defaults)

`dsd-fme -i tcp:192.168.7.5:7356` (remote host and custom port)

--48000/9600 Mono Wav File Input

`dsd-fme -i filename.wav` 48k/1 Audio (SDR++, GQRX wav recordings)

`dsd-fme -i filename.wav -s 96000` 96k/1 (DSDPlus Raw Signal Wav Files)

Wav File Input Note: Due to 96000 rate audio requiring me to double the symbol rate and center, be sure to use the -s 96000 at the very end of the startup command. Also, some NXDN48/96 may have difficulties decoding properly with wav file input. 

## Example Usage - RTL2832 Dongle Input
`dsd-fme -ft -i rtl:0:154.9875M:26:-2:8:0:6020 `

```
RTL-SDR options:
 WARNING! Old CLI Switch Handling has been depreciated in favor of rtl:<parms>
 Usage: rtl:dev:freq:gain:ppm:bw:sq:udp
  dev  <num>    RTL-SDR Device Index Number
  freq <num>    RTL-SDR Frequency (851800000 or 851.8M)
  gain <num>    RTL-SDR Device Gain (0-49) (default = 26)(0 = Hardware AGC, not recommended)
  ppm  <num>    RTL-SDR PPM Error (default = 0)
  bw   <num>    RTL-SDR VFO Bandwidth kHz (default = 12)(6, 8, 12, 24) 
  sq   <num>    RTL-SDR Squelch Level (0 - Open, 25 - Little, 50 - Higher)
  udp  <num>    RTL-SDR UDP Remote Port (default = 6020)
 Example: dsd-fme -fp -i rtl:0:851.375M:22:-2:12:0:6021

```

## Example Usage - Extra Payload/PDU Logging

`dsd-fme -Z -N 2>> log.ans`

and in a second terminal tab, same folder, run

`tail -n 40 -f log.ans`

### EDACS/P25/NXDN/DMR Simple/Single VFO Trunking ###

EDACS Trunking (w/ channel map import)

--EDACS/PV Trunking using RIGCTL and TCP Direct Link Audio inside of SDR++ (Tested and Working on EDACS/EDACS-EA with Provoice only, no analog voice monitoring)

`dsd-fme -i tcp -fp -C channel_map.csv -G group.csv -T -U 4532 -N 2> log.ans`

--NXDN48 Trunking (standard band plan) with SDR++ (untested for frequency accuracy)

`dsd-fme -fi -i tcp -T -U 4532 -N 2> log.ans`

--NXDN48 Trunking (w/ channel map import) with SDR++ (put control channel frequency at channel 0 in channel_map.csv)

`dsd-fme -fi -i tcp -T -U 4532 -C channel_map.csv -N 2> log.ans`

--DMR Trunking (w/ channel map import) with SDR++ (put control channel frequency at channel 0 (TIII and Con+) in channel_map.csv)

`dsd-fme -fs -i tcp -T -U 4532 -C channel_map.csv -N 2> log.ans`

--P25 Trunking P1 and P2 (C4FM) with SDR++ 

`dsd-fme -i tcp -T -U 4532 -N 2> log.ans`

--P25 Trunking (CQPSK) with P1 Control Channel (Should switch symbol rate and center on Phase 2 audio channels)

`dsd-fme -i tcp -T -U 4532 -N -mq 2> log.ans`

--P25 Trunking Phase 2 TDMA Control Channel systems with CQPSK (non Phase 1 systems)

`dsd-fme -i tcp -T -U 4532 -N -f2 -m2 2> log.ans`

Trunking Note1: All samples above can also be run with the RTL input method and setting of RTL UDP remote port. In terms of performance, however, SDR++ will do a much better job on weaker/marginal signal compared to the RTL input. RTL input should only be used on strong signal.

`dsd-fme -fp -i rtl:0:851.8M:22:-2:24:0:6020 -T -C channel_map.csv -G group.csv -N 2> log.ans`

Trunking Note2: CQPSK Phase 1 and Phase 2 Systems are subceptible to LSM distortion issues, but seem to do okay, but require really good signal. Some CRC issues still occur with Phase 2 TDMA LCCH Mac Signal that can affect reliability, I believe this issue is ultimately caused by the PSK demodulation inside of FME. I also don't believe this will work on 8-level PSK, but I cannot determine that at the moment. Update: I have improved the LCCH Mac Signal decoding my increasing the QPSK decision point buffers to their maximum values. 

Trunking Note3: DMR Trunking has been coded, and some testing and tweaks have been carried out. Cap+, Con+, and TIII systems seem to do well with trunking now. Placing the frequency for the control channel at channel map 0 in your channel_map.csv file is not required now if using RIGCTL or the RTL Input, both can poll the VFO for the current frequency if it believes its on a control channel, but using channel 0 as the control channel will hardset that frequency to the control channel. If you need to map out your channels for TIII, you can observe the console output and look for channel numbers. For conveniece I have included the DSDPlus channel numbering (as best as I can figure it) into the console print so it will make it easier for users from DSDPlus to map frequencies into the channel_map.csv file. Make sure your channel numbers are the Cd (channel decimal) values from the log, and not the C+ (dsdplus) values. Notice: TIII Site ID value needs work to determine proper DMRLA values for system area and sub area.

```
 Talkgroup Voice Channel Grant (TV_GRANT) - Logical
  Ch [036] Cd [0054] C+ [0110] - TS [1] - Target [01900500] - Source [01900505]
```

Use channel 54 in your import file, which would correspond to dsdplus channels 109 and 110 (109 = TS0 / 110 = TS1).

For Connect Plus, enumerate your list from 1 to the last channel and add the frequency. For Capacity Plus, Rest Channels 1 and 2 will share the same frequency, 3 and 4 will share, 5 and 6 will share, and 7-8 will share, as Capacity Plus counts each RF frequency as two seperate channels, one for each time slot. Capacity Plus Quirk: DSD-FME makes its best effort to follow the rest channel in the event that the sync is lost for longer than the hangtime, but occassionally, DSD-FME will lose the rest channel and will have to hunt through all frequencies to find it again.

Currently uncoded/unknown DMR trunking systems include Hytera XPT.

Trunking Note4: NXDN Trunking may also require a channel map file, depending on the system. If it uses a custom range (above 800), then channels will need to be mapped. If channels do not require mapping (have channels below 800) but you are not tuning properly, then please use a channel_map and please report the issue in the issues along with the channels you are tuning and the actual rf frequency so corrections can be made.

Channel Map and Group CSV Note: Leave the top line of the channel_map.csv and group.csv as the label, do not delete the line, if no line is there, dsd_import skips the first line so it will not import the first channel or first group in those files if there is something there that isn't a label. 

## NCurses Keyboard Shortcuts ##

Some Keyboard Shortcuts have been implemented for testing to see how users like them. Just hope nobody pushes a key on accident and doesn't know which one it was or what it does. The current list of keyboard shortcuts include:

```
esc or arrow keys - ncurses menu
q - quit
c - toggle compact mode
h - toggle call history
z - toggle console payloads
a - toggle call alert beep
4 - force dmr privacy key assertion over fid and svc bits
i - toggle signal inversion on types that can't auto detect (dmr, dpmr)
m - toggle c4fm/cqpsk 10/4 (everything but phase 2 signal)
M - toggle c4fm/cqpsk 8/3 (phase 2 tdma control channel)
t - toggle trunking (needs either rtl input, or rigctl setup at CLI)
R - start capturing symbol capture bin (date/time name file)
r - stop capturing symbol capture bin
spacebar - replay last symbol capture bin (captures must be stopped first)
s - stop playing symbol capture bin or wav input file
P - start per call decoded wav files
p - stop per call decoded wav files
1 - Lockout Tuning/Playback of TG in Slot 1 or Conventional (Current Session Only)
2 - Lockout Tuning/Playback of TG in Slot 2 (Current Session Only)
0 - Toggle Audio Smoothing - May produce crackling if enabled on RTL/TCP or wav/bin files
w - Toggle Trunking/Playback White List (Allow A Groups Only) / Black List (Block B or DE groups only) Mode

```