# Digital Speech Decoder - Florida Man Edition
This version of DSD is a flavor blend of [szechyjs](https://github.com/szechyjs/dsd "szechyjs") RTL branch and some of my own additions, along with a few tweaks from the [LouisErigHerve](https://github.com/LouisErigHerve/dsd "LouisErigHerve") branch as well. NXDN voice decoding is currently working a lot better, thanks to the latter, although I have yet to explore the expanded NXDN or DMR decoding he has laid out. That is a goal. I have also implemented a few more RTL options, including rtl device gain, PPM error, device index selection, squelch, VFO bandwidth, and a UDP remote that works like the old rtl_udp fork, although its currently limited to changing frequency and squelch. The goal is to integrate this project into [EDACS-FM](https://github.com/lwvmobile/edacs-fm "EDACS-FM") but I also want it to be its own standalone project. 

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/pulseaudio/dsd-fme.png)

## Example Usage - RTL
`dsd -fi -i rtl -o pulse -c 154.9875M -P -2 -D 1 -G 36 -L 70 -U 6021 -Y 12`

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

-Y 12 set rtl VFO bandwidth in kHz, (default = 48)(6, 8, 12, 16, 24, 48)

-W Monitor Source Audio (WIP!) (Currently disabled on PulseAudio branch)
```

### Note

`dsd` and `./dsd` may need to be used in your usage, depending on if you are running it from the build folder, or if you have run make install. You will need to tweak the examples to your particular usage


## Example Pulse Audio Input and Pulse Audio Output, Autodetect Frame Type
`dsd` or `./dsd`
```
Yes, it really is this simple now

-fa Auto-detect frame type

sans NXDN or Provoice, need to specify -fp, -fi, or -fn
pulse audio are set as default input and output methods

```
## Example STDIN UDP from SDR++, output to Pulse Audio, and save wav files
`socat stdio udp-listen:7355 | dsd -fi -i - -o pulse -w nxdn.wav`

## Roadmap
The Current list of objectives include:

1. ~~Random Tinkering~~ More Random Tinkering

2. Implemented Pulse Audio and ~~Remove PortAudio and~~ Remove OSS, including SOLARIS/APPLE/BSD methods, and Retain PortAudio as Optional (need to re-enable in CMakeLists.txt file)

3. Improve NXDN and DMR support 

4. More Concise Printouts - Ncurses

5. ~~Improve Monitor Source Audio (if #2 on list is up and working)~~ Not currently playing well with Pulse Audio, need to re-evaluate

6. ~~Make simple to use installer script.~~ Working on script now, also have full build and install guide down below

## How to clone, check out, and build this branch

Manual Installation: Follow the steps down below.

First, install dependency packages. Check in the Cygwin Cheatsheet folder to see dependencies to install using setup.exe that came with Cygwin.  The guide below will be a loose guide, based on the Debian/Ubuntu guide, as I cannot predict exactly which dependencies will be absolutely necesary in Cygwin. Regardless, the build instructions should be nearly identical once the proper dependencies have been met. The user will also need to manually build and install rtl-sdr and librtlsdr in Cygwin if support for RTL dongles is desired. As of this writing, I have not successfully been able to build a current version inside of Cygwin.

```
#libpulse-dev pavucontrol libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev libncurses5 libncurses5-dev rtl-sdr librtlsdr-dev libusb-1.0-0-dev cmake git wget make build-essential

wget -O itpp-latest.tar.bz2 http://sourceforge.net/projects/itpp/files/latest/download?source=files
tar xjf itpp*
#if you can't cd into this folder, double check folder name first
cd itpp-4.3.1 
mkdir build
cd build
cmake ..
make 
make install
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
make
make install
cd ..
cd ..
```

Finish by running these steps to clone and build DSD-FME w/ pulseaudio support in Cygwin.

```
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
git branch -a
git checkout remotes/origin/cygwin
git checkout -b cygwin
git branch -a #double check to see if you are on cygwin branch
mkdir build
cd build
cmake ..
make 
##only run make install if you want this to be your main DSD install##
make install

```
After make has successfully completed, go into the mbelib/build folder and copy cygmbe-1.dll and paste it into the dsd-fme/build folder.

Already have this branch, and just want to pull the latest build?

```
##Open your clone folder##
git pull https://github.com/lwvmobile/dsd-fme cygwin
##cd into your build folder##
cd build
##cmake usually isn't necesary, but could be if I update the cmakelist.txt
cmake ..
make 
##only run make install if you want this to be your main DSD install##
make install
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
    
   
