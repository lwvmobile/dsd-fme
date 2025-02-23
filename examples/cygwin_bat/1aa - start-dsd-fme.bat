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


@REM Default Decoders (DMR, P25, X2, DSTAR, YSF):
@REM -i /dev/dsp, -o /dev/dsp, and -fa are implied

@REM dsd-fme.exe
dsd-fme.exe -N 2> log.ans

@REM DMR

@REM dsd-fme.exe -fs
@REM dsd-fme.exe -fs -N 2> log.ans
@REM dsd-fme.exe -fs -xr #(inverted polarity)

@REM P25

@REM dsd-fme.exe -ft
@REM dsd-fme.exe -ft -N 2> log.ans


@REM P25 (Phase 2 Only, specify WACN/SYS/CC value, if not control channel)

@REM dsd-fme.exe -f2 -X BEE00ABC123
@REM dsd-fme.exe -f2 -X BEE00ABC123 -N 2> log.ans

@REM X2-TDMA

@REM dsd-fme.exe -fx
@REM dsd-fme.exe -fx -N 2> log.ans
@REM dsd-fme.exe -fx -xx #(inverted polarity)


@REM NXDN48

@REM dsd-fme.exe -fi
@REM dsd-fme.exe -fi -N 2> log.ans


@REM NXDN96

@REM dsd-fme.exe -fn
@REM dsd-fme.exe -fn -N 2> log.ans


@REM YSF

@REM dsd-fme.exe -fy
@REM dsd-fme.exe -fy -N 2> log.ans


@REM DSTAR

@REM dsd-fme.exe -fd
@REM dsd-fme.exe -fd -N 2> log.ans


@REM dPMR

@REM dsd-fme.exe -fm
@REM dsd-fme.exe -fm -N 2> log.ans
@REM dsd-fme.exe -fm -xd #(inverted polarity)


@REM M17

@REM dsd-fme.exe -fz
@REM dsd-fme.exe -fz -N 2> log.ans
@REM dsd-fme.exe -fz -xz #(inverted polarity)


@REM ProVoice

@REM dsd-fme.exe -fp
@REM dsd-fme.exe -fp -N 2> log.ans


@REM ProVoice (Conventional / Simplex)

@REM dsd-fme-pvc.exe -ft
@REM dsd-fme-pvc.exe -ft -N 2> log.ans


@REM PAUSE
