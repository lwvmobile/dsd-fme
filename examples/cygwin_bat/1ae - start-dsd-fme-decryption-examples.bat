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
@REM Important! Make sure to route audio in with volume mixer after opening!
@REM You may need to restart the software after setting it up to monitor input
@REM of VB-Cables, or allowing the network traffic for TCP or RIGCTL
@REM See the 'source code\examples\Audio_Plumbing.md' file for more information
@REM
@REM Notice: DSD-FME will not auto-detect NXDN48/96, Provoice, M17, or dPMR signals, 
@REM you will need to manually switch them on with the correct command.
@REM
@REM Notice: This .bat file contains numerous examples, it is recommended to find an
@REM an example that matches your use case, and use the @REM to disable the default
@REM or make your own .bat files with the examples provided here
@REM feel free to experiment and mix and match options, see what works best

@REM DMR with default /dev/dsp input and output, use basic privacy key 70
dsd-fme.exe -fs -b 70

@REM DMR with default /dev/dsp input and output, load .csv file with decimal basic privacy keys loaded by TG value,
@REM dsd-fme.exe -fs -k examples\multi_key.csv

@REM DMR with default /dev/dsp input and output, use Hytera basic privacy key (10, 32, and 64 hex char versions)
@REM dsd-fme.exe -fs -H 1111111111
@REM dsd-fme.exe -fs -H '2222222222222222 3333333333333333'
@REM dsd-fme.exe -fs -H '2222222222222222 3333333333333333 4444444444444444 5555555555555555'

@REM DMR with default /dev/dsp input and output, use RC4 key 0x7777777777
@REM dsd-fme.exe -fs -1 7777777777

@REM DMR with default /dev/dsp input and output, use RC4 key 0x7777777777, 
@REM Force RC4 Key over Missing PI header/LE Encryption Identifiers (-0)
@REM Disable Late Entry Encryption Signalling Identifiers (-3)
@REM dsd-fme.exe -fs -0 -3 -1 7777777777

@REM DMR with default /dev/dsp input and output, load RC4 and 10-char HBP keys from .csv file (hexidecimal format)
@REM dsd-fme.exe -fs -K examples\multi_key_hex.csv

@REM P25 with default /dev/dsp input and output, use RC4 key 0x7777777777
@REM dsd-fme.exe -ft -1 7777777777

@REM NXDN48 with default /dev/dsp input and output, use Scrambler key 32767
@REM dsd-fme.exe -fi -R 32767

@REM NXDN96 with default /dev/dsp input and output, use Scrambler key 15000
@REM dsd-fme.exe -fn -R 15000

@REM dPMR with default /dev/dsp input and output, use Scrambler key 32767
@REM dsd-fme.exe -fm -R 32767

@REM NXDN48 with default /dev/dsp input and output, load scrambler keys from .csv file (decimal format)
@REM dsd-fme.exe -fi -k examples\nxdn_sc_key.csv

@REM PAUSE
