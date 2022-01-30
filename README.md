# Digital Speech Decoder - Florida Man Edition
This version of DSD shouldn't be used by anybody wanting a stable DSD experience, please see the original version for that.
Also, please don't submit bug reports to the links on this page, those will direct them to the original version. Links will be updated at a later date. 

The purpose of this is to experiment with the RTL code and other things, and is purely for academic research into the inner workings of DSD. Basically, its for me to poke around with and see what I can manage to break.

# Example Usage
padsp -m dsdfme -- ./dsd -fp -i rtl -o /dev/dsp -c 851.8M -d ./MBE/ -P -2 -D 1 -G 36

-i rtl to use rtlsdr 
-P set PPM error
-D set device index number
-G set device gain (0-49 typical)

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
