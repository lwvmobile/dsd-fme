## WARNING! The DEV branch may have broken or incomplete features, changes in Menu or CLI options, and other quirks. USE AT YOUR OWN RISK!

# Digital Speech Decoder - Florida Man Edition

This version of DSD is a flavor blend of [szechyjs](https://github.com/szechyjs/dsd "szechyjs") RTL branch and some of my own additions, along with portions of DMR and NXDN code from the [LouisErigHerve](https://github.com/LouisErigHerve/dsd "LouisErigHerve") branch. This code also borrows snippets, inspiration, and ideas from other open source works including [Boatbod OP25](https://github.com/boatbod/op25 "Boatbod OP25"), [DSDcc](https://github.com/f4exb/dsdcc "DSDcc"), [SDTRunk](https://github.com/DSheirer/sdrtrunk "SDRTrunk"), [MMDVMHost](https://github.com/g4klx/MMDVMHost "MMDVMHost"), [LFSR](https://github.com/mattames/LFSR "LFSR"), and [EZPWD-Reed-Solomon](https://github.com/pjkundert/ezpwd-reed-solomon "EZPWD"). This project wouldn't be possible without a few good people providing me plenty of sample audio files to run over and over again. Special thanks to jurek1111, KrisMar, noamlivne, racingfan360, iScottyBotty, LimaZulu and hrh17 for the many hours of wav samples submitted by them.

## 2022.10.01 Update ##

Very Experimental (Mostly Just Sampling Speed and using built in QPSK demodulator) for Phase 2 CQPSK 6000 sps input over disc tap inputs (SDR++, GQRX, WAV files). Promising results, but some errors in decoding as expected. I've had some pretty good luck using SDR++ to listen to and decode Duke Energy TDMA Control Channel through the virtual sink. Experimental CQPSK can be toggled in Ncurses Menu and also via command line switch `-f2 -m2` but keep in mind this will only work with Phase 2 RF channels using CQPSK, and not a combined decoding.

Also, added some new iden_up PDUs and other odd and end P25 PDU's (still far from complete). Ncurses Terminal and Console output will now show relevant frequencies for CC and Voice channels if iden_up commands are present.

DSD-FME, and The Florida Man Labs will be taking a hiatus period from straigth development of this project. Currently, I feel we are at a bit of a stand still as far as what can be done with the project without new demodulation, dibit buffer, audio, and input methods. DSD-FME will continue to receive maintenance updates for simple bug fixes and also any major bug issues that cause crashing, etc., but I am reluctant to continue developing with the current underlying issues mentioned above. I am going to go into a period of hiatus with this project and do lots of experimentation, learning, and testing to see what viable options are out there or need to be created from scratch. I feel like I've learned a lot, achieved a lot, made tons of mistakes, learned from a few of them, but ultimately improved on the DSD project. DSD-FME will continue to receive replies and support in any opened issues but will currently only extend to help in setting up and using the software, on top of regular minor maintenance and any crash related issues.

## 2022.09.28 Mini Update ##
Back by popular demand, I've re-implemented DMR Stereo as a standalone decoder. DMR Stereo is also still available in XDMA decoding for P25 1, 2, and DMR BS/MS Simplex as well.

DMR Stereo `dsd-fme -fs` 
XDMA       `dsd-fme -ft` 

It can also be selected in the NCurses Terminal Menu. Sadly I had to remove the X2-TDMA decoder option in the menu, I'm sure nobody will miss it.

## 2022.09.17 Update ##
P25 Phase 2 Audio decoding has been implemented into DSD-FME. Currently, Phase 2 will only work from OP25 symbol capture bin files, or from wav files/SDR applications/RTL input that are sourced from FSK4 sources. CQPSK (LSM/H-D8PSK) will not work from audio sources. With the addition of Phases 2 audio, a new default decoder class has been implemented, XDMA, which is P25 Phase 1, Phase 2, DMR BS and MS (DMR Stereo). Furthermore, very limited Phase 1 TSBK support and Phase 2 FACCH/SACCH/LCCH has been worked in just to get Network Status Broadcasts, which can fill in the required P2 parameters for WACN, SYSID, and CC for Phase 2 frame de-scrambling, and active voice call information.

With the implementation of the XDMA decoder class (default decoder), the CLI switches for DMR Stereo has been repurposed. -T option will now open a per call wav file saving when used with NCurses Terminal, otherwise it will write wav files to slot 1 and slot 2 wav files.

Anybody using `dsd-fme -fr -T` ...... should now just use `dsd-fme -ft` instead!

To see the up to date CLI options, please look at the help options with the -h CLI option.

## 2022.08.12 Update ##
A new menu system has been introduced in the NCURSES Terminal. Also includes support for LRRP to text file type of choice and reading OP25 symbol capture bin files.

[![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/pulseaudio/dsd-fme.png)](https://www.youtube.com/watch?v=TqTfAfaPJ4s "DSD-FME Update 2022.08.12")

To get started with the new menu system, simply launch with:

`dsd-fme -N 2> log.txt`

## 2022.06.20 Update - Current Users Please Read!! ##
The executable for this project has been changed from `dsd` to `dsd-fme` so after pulling or cloning the lastest version, make sure to call the software with `dsd-fme`.

## 2022.05.05 Update ##
DMR Stereo method added for listening to voice audio in both TDMA channels/slots simultaneously.

### Example Usage and Note!
`dsd-fme` or `./dsd-fme` may need to be used in your usage, depending on if you are running it from the build folder, or if you have run make install. You will need to tweak the examples to your particular usage. Users can always check `dsd-fme -h`  for help and all command line switches to use.

`dsd-fme` is all you need to run for pulse input, pulse output, and auto detect for DMR BS/MS, P25 (1 and 2) and X2-TDMA decoding. To use other decoding methods which cannot be auto detected, please use the following command line switches. Make sure to route audio into and out of DSD-FME using pavucontrol and virtual sinks as needed.

```
-ft XDMA decoder class (P25 1 and 2, DMR BS/MS, X2-TDMA)
-fs DMR Stereo BS/MS Simplex
-fa Legacy Auto
-fi NXDN48
-fn NXDN96
-fp ProVoice
-fm dPMR, also may need to use -xd if inverted dPMR.
-fr DMR, also may need to use -xr if inverted DMR.
-f1 P25P1
-fx X2-TDMA
```

## Example Usage - New XDMA Decoder, Ncurses Terminal, Pulse Input/Output, and Log Console to file
`dsd-fme -ft -N 2> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

Then you can open up your pavucontrol "Pulse Audio Volume Control" or "Volume Control" application and route input into DSD-FME from any application and DSD-FME output to the left and right speakers respectively. (unlock the channel in the application stream and adjust left and right independently)

## Example Usage - RTL2832 Dongle Input
`dsd-fme -fi -i rtl -c 154.9875M -P -2 -D 1 -G 36 -L 70 -U 6021 -Y 12`

```
-i rtl to use rtl_fm (default is -i pulse for pulse audio)

-o pulse to set output to pulse audio (excluded, is default anyways)

-c Set frequency

-P set PPM error (default = 0)

-D set device index number (default = 0; first device detected)

-G set device gain (0-49) (default = 0 Auto Gain)

-L set rtl squelch to 70 (default = 0; off)

-U set UDP port for rtl_fm remote control (default = 6020)

-Y 12 set rtl VFO bandwidth in kHz, (default = 12)(6, 8, 12, 16, 24, 48)

```
## Example STDIN UDP from GQRX or SDR++, output to Pulse Audio, and append decoded audio to wav file
Be sure to first start UDP output sink in GQRX or SDR++ and set VFO appropriately

`socat stdio udp-listen:7355 | dsd-fme -fi -i - -w nxdn.wav`

## Example Usage - Extra Information for Academic Study and Logging

`dsd-fme -Z -N 2>> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/pulseaudio/dsd-fme2.png)

## Roadmap
The Current list of objectives include:

1. New demodulators and input method(s) (considering liquid-dsp).

2. Graphical User Interface (may need to migrate to new project).

3. Lots of experimentation.

## How to clone, check out, and build this branch

### Ubuntu 22.04/20.04/LM20/Debian Bullseye or Newer:

Using the included download-and-install.sh should make for a simple and painless clone, build, and install on newer Debian/Ubuntu/Mint/Pi systems. Simply acquire or copy the script, and run it. Update: Ubuntu 22.04 and RPi Bullseye 64-bit has been tested working with the installer script and functions appropriately.

If you need all dependencies build and installed first (only on Debian/Ubuntu/Mint/Pi), run:

```
wget https://raw.githubusercontent.com/lwvmobile/dsd-fme/pulseaudio/download-and-install.sh
chmod +x download-and-install.sh
./download-and-install.sh
```

If you have dependencies already installed (i.e. need a fresh clean install on a system with DSD-FME already or using system other than Debian/Ubuntu, etc), please run this instead:

```
wget https://raw.githubusercontent.com/lwvmobile/dsd-fme/pulseaudio/download-and-install-nodeps.sh
chmod +x download-and-install-nodeps.sh
./download-and-install-nodeps.sh
```

### Ubuntu 18.04/LM19/Buster Note:
The above install.sh should now function on older system types. You can elect to manually follow the steps down below. Do NOT Manually build and install ITPP 4.3.1 on older systems, it is currently not wanting to build on Ubuntu 18.04 and Linux Mint 19. Install it from the repository instead.

## Manual Install

First, install dependency packages. This guide will assume you are using Debian/Ubuntu based distros. Check your package manager for equivalent packages if different.

```
sudo apt update
sudo apt install libpulse-dev pavucontrol libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev libncurses5 libncurses5-dev rtl-sdr librtlsdr-dev libusb-1.0-0-dev cmake git wget make build-essential libitpp-dev libncursesw5-dev
```
## Headless

If running headless, swap out pavucontrol for pulsemixer, and also install pulseaudio as well. Attempting to install pavucontrol in a headless environment may attempt to install a minimal desktop environment. Note: Default behavior of pulseaudio in a headless environment may be to be muted, so check by opening pulsemixer and unmuting and routing audio appropriately.

```
sudo apt install libpulse-dev libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev libncurses5 libncurses5-dev rtl-sdr librtlsdr-dev libusb-1.0-0-dev cmake git wget make build-essential libitpp-dev libncursesw5-dev pulsemixer pulseaudio
```

### Build and Install ITPP - ONLY IF NOT IN REPO!!

```
wget -O itpp-latest.tar.bz2 http://sourceforge.net/projects/itpp/files/latest/download?source=files
tar xjf itpp*
#if you can't cd into this folder, double check folder name first
cd itpp-4.3.1
mkdir build
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
cd ..
cd ..
```

MBELib is considered a requirement in this build. You must read this notice prior to continuing. [MBElib Patent Notice](https://github.com/lwvmobile/mbelib#readme "MBElib Patent Notice") This version of MBELib is 1.3.1 and prints to STDERR, using the stock 1.3.0 MBElib may cause problems with print alignments if paired with this version of DSD-FME, or cause issues with printing over the screen in future builds.

```
git clone https://github.com/lwvmobile/mbelib
cd mbelib
mkdir build
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
cd ..
cd ..
```

Finish by running these steps to clone and build DSD-FME w/ pulseaudio support.

```
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
git branch -a
git checkout remotes/origin/pulseaudio
git checkout -b pulseaudio
git branch -a #double check to see if you are on pulseaudio branch
sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
mkdir build
cd build
cmake ..
make -j `nproc`
##only run make install if you don't have another version already installed##
sudo make install
sudo ldconfig

```
Optional 'Virtual Sinks' for routing audio from SDR++ or GQRX, Media Players, etc. into DSD-FME

You may wish to direct sound into DSD-FME via Virtual Sinks. You may set up a Virtual Sink or two on your machine for routing audio in and out of applications to other applications using the following command, and opening up pavucontrol "PulseAudio Volume Control" in the menu (or `pulsemixer` in headless mode) to change line out of application to virtual sink, and line in of DSD-FME to monitor of virtual sink. This command will not persist past a reboot, so you will need to invoke them each time you reboot, or search for how to add this to your conf files for persistency if desired.

```
pacmd load-module module-null-sink sink_name=virtual_sink  sink_properties=device.description=Virtual_Sink
pacmd load-module module-null-sink sink_name=virtual_sink2  sink_properties=device.description=Virtual_Sink2
```

Already have this branch, and just want to pull the latest build? You can run the rebuild.sh file in the dsd-fme folder, or manually do the pull with the commands:

```
##Open your clone folder##
git pull https://github.com/lwvmobile/dsd-fme pulseaudio
##cd into your build folder##
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
```

If the call alert wav files aren't playing, then make sure to run the following in the dsd-fme folder to copy the wav files to the /usr/share/ folder and give them adequate permission to be accessed.

```
sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
```

# Join the Conversation

Want to help drive the direction of this project and read more about the latest updates and releases to DSD-FME? Then join the conversation on the 'unofficial official' [DSD-FME](https://forums.radioreference.com/threads/dsd-fme.438137/ "DSD-FME") Forum Thread on the Radio Reference Forums.


## License
Copyright (C) 2010 DSD Author
GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
    REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
    LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
    OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
    PERFORMANCE OF THIS SOFTWARE.
