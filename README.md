# Digital Speech Decoder - Florida Man Edition

This version of DSD is a flavor blend of [szechyjs](https://github.com/szechyjs/dsd "szechyjs") RTL branch and some of my own additions, along with portions of DMR and NXDN code from the [LouisErigHerve](https://github.com/LouisErigHerve/dsd "LouisErigHerve") branch as well. This code also borrows snippets, inspiration, and ideas from other open source works including [Boatbod OP25](https://github.com/boatbod/op25 "Boatbod OP25"), [DSDcc](https://github.com/f4exb/dsdcc "DSDcc"), [SDTRunk](https://github.com/DSheirer/sdrtrunk "SDRTrunk"), [MMDVMHost](https://github.com/g4klx/MMDVMHost "MMDVMHost"), and [LFSR](https://github.com/mattames/LFSR "LFSR"). This project wouldn't be possible without a few good people providing me plenty of sample audio files to run over and over again. Special thanks to jurek1111, KrisMar, noamlivne, racingfan360, iScottyBotty, LimaZulu and hrh17 for the many hours of wav samples submitted by them.

## 2022.08.12 Update ##
A new menu system has been introduced in the NCURSES Terminal. While running DSD-FME, a user can simply use the ESC or arrow keys to open the menu and change options on the fly. This is extremely beneficial when wanting to change settings on the fly, and also to alleviate the need to use complex start up command line arguments. All of the previous command line arguments still work and will get you up faster, but if you would rather configure after start up, or change settings while running, the menu system is very helpful in that regard.

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/pulseaudio/dsd-fme.png)

To get started with the new menu system, simply launch with:

`dsd-fme -N 2> log.txt`

Virtually all the examples listed below can now be set up on the fly with the menu system, no need to check the help or refer to this page.

This release also has provisional support for playing back OP25 capture bin files, and the creation and playback of its own capture bin files as well (most likely not OP25 compatible). Mileage may vary. Be sure to choose the decoder option required when playing a bin file back, do not rely on auto-detect. Capture Bin files can be used to reliably replay P25, DMR, and NXDN decodings, and can replace the need for MBE file saving in the case of DMR Stereo, allowing for a complete replay of all events on a system, rather than just voice only.

Per Call wav file creation has been implemented, currently only for DMR Stereo. Complete audio decode wav is available for all over decoder types. LRRP has been revamped, working more consistently, and an option to dump LRRP data to ~/lrrp.txt in the user home directory now exists, and can be imported into open-source QGIS to provide mapping data for LRRP output. Simply open the newly included map file in QGIS and point it at your geographic region. (Be sure the lrrp.txt file exists, and has data populated in it prior to opening the map file in QGIS, otherwise, the map layer may not want to show back up!)

DMR Data Header decoding has been revamped, and most data burst decoding types tightened up and code cleaned. FEC error checking now implemented on CACH and Burst types, cleaning up some erroneous slot and burst errors.

## 2022.06.20 Update - Current Users Please Read!! ##
The executable for this project has been changed from `dsd` to `dsd-fme` so after pulling or cloning the lastest version, make sure to call the software with `dsd-fme`.

## 2022.05.05 Update ##
DMR Stereo method added for listening to voice audio in both TDMA channels/slots simultaneously.

### Example Usage and Note!
`dsd-fme` or `./dsd-fme` may need to be used in your usage, depending on if you are running it from the build folder, or if you have run make install. You will need to tweak the examples to your particular usage. Users can always check `dsd-fme -h`  for help and all command line switches to use.

`dsd-fme` is all you need to run for pulse input, pulse output, and auto detect for DMR, P25P1, D-STAR, and X2-TDMA decoding. To use other decoding methods which cannot be auto detected, please use the following command line switches. Make sure to route audio into and out of DSD-FME using pavucontrol and virtual sinks as needed.

```
-fi NXDN48
-fn NXDN96
-fp ProVoice
-fm dPMR, also may need to use -xd if inverted dPMR.
-fr DMR, also may need to use -xr if inverted DMR, use -T for Stereo, and -F for passive sync
-f1 P25P1
-fx X2-TDMA
```

## Example Usage - New DMR Stereo, Ncurses Terminal, Pulse Input/Output, and Log Console to file
`dsd-fme -fr -T -N 2> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/pulseaudio/dsd-fme2.png)

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

`dsd-fme -Z -pu -N 2>> voice.log`

and in a second terminal tab, same folder, run

`tail -n 40 -f voice.log`

## Roadmap
The Current list of objectives include:

1. Include P25 P2 Audio decoding with capture bin files, then RTL input and/or disc tap input.
~~Random Tinkering~~ ~~More Random Tinkering~~

2. ~~Implemented Pulse Audio~~ ~~Remove remaining PortAudio code,~~ ~~improved Pulse Audio for stereo output and channel/slot separation~~ ~~and reimplement wav file saving using DMR Stereo method for stereo wav files, or seperate as per call wav files.~~

3. ~~Improve NXDN and DMR support~~ Continue to improve ~~NXDN and DMR~~ all data and voice decoding.

4. ~~More Concise Printouts - Ncurses~~

5. ~~Make simple to use installer script.~~

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
##cmake usually isn't necesary, but could be if I update the cmakelist.txt
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
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
