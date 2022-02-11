# Digital Speech Decoder - Florida Man Edition
This version of DSD is a flavor blend of [szechyjs](https://github.com/szechyjs/dsd "szechyjs") RTL branch and some of my own additions, along with a few tweaks from the [LouisErigHerve](https://github.com/LouisErigHerve/dsd "LouisErigHerve") branch as well. NXDN voice decoding is currently working a lot better, thanks to the latter, although I have yet to explore the expanded NXDN or DMR decoding he has laid out. That is a goal. I have also implemented a few more RTL options, including rtl device gain, PPM error, device index selection, squelch, VFO bandwidth, and a UDP remote that works like the old rtl_udp fork, although its currently limited to changing frequency and squelch. The goal is to integrate this project into [EDACS-FM](https://github.com/lwvmobile/edacs-fm "EDACS-FM") but I also want it to be its own standalone project. 

![alt text](https://github.com/lwvmobile/dsd-fme/blob/master/Screenshot_216.png)

## Example Usage - RTL
`./dsd -fi -i rtl -o pulse -c 154.9875M -P -2 -D 1 -G 36 -L 25 -V 2 -U 6020 -Y 8`

```
-i rtl to use rtl_fm (default is -i pulse for pulse audio)

-o pulse to set output to pulse audio (is default anyways)

-c Set frequency

-P set PPM error

-D set device index number

-G set device gain (0-49) (default = 0 Auto Gain)

-L set rtl squelch to 25

-V set RTL sample 'volume' multiplier

-U set UDP port for rtl_fm remote control

-Y 8 set rtl VFO bandwidth in kHz, (default = 48)(6, 8, 12, 16, 24, 48)

-W Monitor Source Audio (WIP!) (may or may not decode audio if this is on, depending on selected decode type and luck)
(Also, should be noted that depending on modulation, may sound extremely terrible)
(Currently causing Pulse Audio server to restart on Mint, but works in Arch)
```
## Example Pulse Audio Input and Pulse Audio Output, Autodetect Frame Type
`./dsd`
```
Yes, it really is this simple now
-fa Auto-detect frame type
sans NXDN or Provoice, need to specify -fp, -fi, or -fn
pulse audio are set as default input and output methods

```
## Example STDIN UDP from GQRX or SDR++, output to Pulse Audio, and save wav files
`socat stdio udp-listen:7355 | ./dsd -fi -i - -o pulse -w nxdn.wa`

## Roadmap
The Current list of objectives include:

1. ~~Random Tinkering~~ More Random Tinkering

2. Implemented Pulse Audio and ~~Remove PortAudio and~~ Remove OSS, including SOLARIS/APPLE/BSD methods, and Retain PortAudio as Optional (need to re-enable in CMakeLists.txt file)

3. Improve NXDN and DMR support 

4. More Concise Printouts - Ncurses

5. ~~Improve Monitor Source Audio (if #2 on list is up and working)~~ Not currently playing well with Pulse Audio, need to re-evaluate

6. Make simple to use installer script.

## How to clone, and check out this branch

First, install dependency packages. This guide will assume you are using Debian/Ubuntu based distros. Check your package manager for equivalent packages if different. PortAudio is not currently used in this build, and is disabled in CMakeLists.txt, you can re-enable it if you wish, but it isn't recommended unless you have a very specific reason to do so. Some of these dependencies are not currently be used, but may be used in future builds.

```
sudo apt update
sudo apt install libpulse-dev pavucontrol libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev libncurses5 libncurses5-dev rtl-sdr libusb-1.0-0-dev cmake git wget make build-essential

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
Optional 'Virtual Sinks' for routing audio from SDR++ or GQRX, etc, into DSD-FME

You may wish to direct sound into DSD-FME via Virtual Sinks. You may set up a Virtual Sink or two on your machine for routing audio in and out of applications to other applications using the following command, and opening up pavucontrol "PulseAudio Volume Control" in the menu to change line out of application to virtual sink, and line in of DSD-FME to monitor of virtual sink. This command will not persist past a reboot, so you will need to invoke them each time you reboot, or search for how to add this to your conf files for persistency if desired.

```
pacmd load-module module-null-sink sink_name=virtual_sink  sink_properties=device.description=Virtual_Sink
pacmd load-module module-null-sink sink_name=virtual_sink2  sink_properties=device.description=Virtual_Sink2
```

Already have this branch, and just want to pull the latest build?

```
##Open your clone folder##
git pull https://github.com/lwvmobile/dsd-fme pulseaudio
##cd into your build folder##
cd build
##cmake usually isn't necesary, but could be if I update the cmakelist.txt
cmake ..
make
sudo make install
sudo ldconfig
```

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
    
   
