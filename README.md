# Digital Speech Decoder - Florida Man Edition

DSD-FME is an evolution of the original DSD project from 'DSD Author' using the base code of [szechyjs](https://github.com/szechyjs/dsd "szechyjs"), some code and ideas from [LouisErigHerve](https://github.com/LouisErigHerve/dsd "LouisErigHerve"), [Boatbod OP25](https://github.com/boatbod/op25 "Boatbod OP25") and [Osmocom OP25](https://gitea.osmocom.org/op25/op25 "Osmocom OP25"), along with other snippets of code, information, and inspirations from other projects including [DSDcc](https://github.com/f4exb/dsdcc "DSDcc"), [SDRTRunk](https://github.com/DSheirer/sdrtrunk "SDRTrunk"), [MMDVMHost](https://github.com/g4klx/MMDVMHost "MMDVMHost"), [LFSR](https://github.com/mattames/LFSR "LFSR"), [OK-DMRlib](https://github.com/OK-DMR/ok-dmrlib "OK-DMRlib"), and [EZPWD-Reed-Solomon](https://github.com/pjkundert/ezpwd-reed-solomon "EZPWD"), Eric Cottrell, SP5WWP and others. Finally, this is all brought together with original code to extend the fuctionality and add new features including NCurses Terminal and Menu system, Pulse Audio, TCP Direct Link Audio, RIGCTL, Trunking Features, LRRP/GPS Mapping, P25 Phase 2, EDACS, YSF, M17, OP25 Capture Bin compatability, etc. DSD-FME is primarily focused with Linux Desktop users in mind, so please understand that this version may not compile, compile easily, or run correctly in other environments.

This project wouldn't be possible without a few good people providing me plenty of sample audio files to run over and over again. Special thanks to jurek1111, KrisMar, noamlivne, racingfan360, iScottyBotty, LimaZulu, Forts, thewraithe2008, RayAir, Cretu, ilyacodes, and others for the many hours of wav samples and information provided by them. Most importantly, HRH17, whose insight, information, samples, and willingness to let me remote into a computer half-way across the globe in order to test trunking features are what make DSD-FME what it has become. I'd also like to thank mrscanner2008 for providing an additional remote where additional NXDN Type-C, 'Idas' Type-D, and XPT decoding and trunking could be sorted out. Thanks to volo-zyko for cleaning up a lot of code. Thank you everybody.

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/audio_work/dsd-fme2.png)

![DSD-FME](https://github.com/lwvmobile/dsd-fme/blob/audio_work/dsd-fme3.png)

## Prerequisites

### Supported Distributions
- **Ubuntu** (20.04+, 22.04 LTS, 24.04 LTS)
- **Debian** (11+, 12+)
- **Arch Linux** 
- **Fedora** (38+)

### Hardware Recommendations
- A multicore cpu with atleast 1.5GHz or higher
- 4GB+ RAM

---

## Quick Installation

### Ubuntu/Debian
```bash
# Clone the repository
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme

# Run the automated installer
bash download-and-install-ubuntu2404lts.sh

# Alternative: Use general installer
bash download-and-install.sh
```

### Arch Linux
```bash
# Install dependencies
sudo pacman -S git cmake gcc make libpulse audiofile libsndfile

# Clone and build
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
bash install.sh
```

### Fedora
```bash
# Install dependencies
sudo dnf install git cmake gcc-c++ make pulseaudio-libs-devel audiofile-devel libsndfile-devel

# Clone and build
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
bash install.sh
```

---

## Manual Installation (All Distributions)

If the automated scripts fail, follow these manual steps:

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install git build-essential cmake libpulse-dev audiofile-dev libsndfile1-dev
```

**Arch Linux:**
```bash
sudo pacman -S git base-devel cmake pulseaudio audiofile libsndfile
```

**Fedora:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake pulseaudio-libs-devel audiofile-devel libsndfile-devel
```

### 2. Build DSD-FME
```bash
# Clone repository
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme

# Clean build (remove any existing build directory)
rm -rf build

# Create build directory and compile
mkdir build && cd build
cmake ..
make -j$(nproc)

# Install (optional)
sudo make install
```

---

## Troubleshooting Common Issues

### MBE Library Errors
If you encounter `Could NOT find MBE` errors:

```bash
# Try the distribution-specific installers first
bash download-and-install-ubuntu2404lts.sh
# OR
bash download-and-install.sh

# If manual installation is needed for MBE:
sudo apt install libmbe-dev  # Ubuntu/Debian
# OR build from source if not in repositories
```

### Permission Issues
```bash
# Never use sudo for mkdir build or cmake commands
# If you get permission errors, clean up and retry:
sudo rm -rf build
bash install.sh  # Without sudo!
```

### Build Directory Conflicts
```bash
# If build directory exists and causes issues:
rm -rf build
mkdir build
cd build
cmake ..
make -j4
```

---

## Basic Usage Examples

After successful installation, test DSD-FME:

```bash
# Decode from audio file
./dsd-fme -i input.wav

# Monitor system audio input
./dsd-fme -i /dev/dsp

# With PulseAudio
./dsd-fme -i pulse

# Enable verbose output
./dsd-fme -i input.wav -v -T

# Use NCurses terminal interface
./dsd-fme -i input.wav -T
```

### Common Options:
- `-i <input>`: Input source (file, device, or pulse)
- `-o <output>`: Output audio device
- `-T`: Enable terminal/NCurses interface
- `-v`: Verbose output
- `-h`: Show help with all options

---

## Advanced Features

DSD-FME supports several advanced features:

- **Trunking Systems**: P25, EDACS, NXDN
- **Protocols**: P25 Phase 2, M17, DMR, YSF
- **Audio Output**: PulseAudio, ALSA, TCP streaming
- **Mapping**: LRRP/GPS location data
- **Remote Control**: RIGCTL compatibility

### Trunking Example:
```bash
./dsd-fme -i pulse -T -x -U -g 10 -2 -O -3 -4 -5
```

---

## Important Notes

- **Audio Setup**: Make sure your audio devices are properly configured
- **Real-time Decoding**: For SDR input use tools like `rtl_fm` or `gqrx` to pipe audio
- **Permissions**: Uncommon issue but you may need audio group permissions for device access: `sudo usermod -a -G audio $USER`

---

## Information
See the [examples](https://github.com/lwvmobile/dsd-fme/tree/audio_work/examples "examples") folder for information on [cloning and installing](https://github.com/lwvmobile/dsd-fme/blob/audio_work/examples/Install_Notes.md "cloning and installing"), [example usage](https://github.com/lwvmobile/dsd-fme/blob/audio_work/examples/Example_Usage.md "example usage"), and [trunking examples](https://github.com/lwvmobile/dsd-fme/blob/audio_work/examples/trunking.sh "trunking examples").

## ⚖️ License
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
