# Digital Speech Decoder - Florida Man Edition
This version of DSD is a flavor blend of szechyjs RTL branch and some of my own additions, along with a few tweaks from the LouisErigHerve branch as well. NXDN voice decoding is currently working a lot better, thanks to the latter, although I have yet to explore the expanded NXDN decoding he has laid out as of yet. That is a goal. I have also implemented a few more RTL options, including a UDP remote that works like the old rtl_udp fork, although its currently limited to changing frequency and squelch. The goal is to integrate this project into EDACS-FM but I also want it to be its own standalone project. 

# Example Usage
padsp -m dsdfme -- ./dsd -fp -i rtl -o /dev/dsp -c 851.8M -d ./MBE/ -P -2 -D 1 -G 36 -L 25 -V 2 -U 6020

-i rtl to use rtl_fm 

-c Set frequency

-P set PPM error

-D set device index number

-G set device gain (0-49 typical)

-L set rtl squelch to 25

-V set RTL sample 'gain' multiplier

-U set UDP port for rtl_fm remote control

-W Monitor Source Audio (WIP!) (may or may not decode audio if this is on, depending on selected decode type)
(Also, should be noted that depending on modulation, may sound extremely terrible)

# Roadmap
The Current list of objectives include:

1. Random Tinkering

2. Implement Pulse Audio and Remove PortAudio and OSS

3. Improve NXDN support 

4. More Concise Printouts - Ncurses

4. Improve Monitor Source Audio (if #2 on list is up and working)


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
