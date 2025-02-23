@REM Set the console / terminal to 90 columns, 50 lines
@REM mode con: cols=90 lines=50

@REM only run this command while running DSD-FME with pulse output in a stereo 2 channel configuration
@REM and only IF you have a stereo volume panning issue (this resolves a possibility that a user may have
@REM used an older supplied .bat file to pan channels left or right when two pulse output streams were used)

@REM change directory into the dsd-fme folder
cd dsd-fme

@REM Manually set output volume for left and right channel to 100%
@REM you can also adjust the value of 65535 (100%) to any value from 0 to 65535

@REM                        //dev//left//right
pactl.exe set-sink-input-volume 0 65535 65535
pactl.exe set-sink-input-volume 1 65535 65535

PAUSE
