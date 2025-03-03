@REM Shutdown pulseaudio and clean out any remenants in /dev/shm if left behind

cd dsd-fme
pactl.exe exit

@REM now navigate to the /dev/shm folder and automatically remove any 
@REM remnants from other times pulse wasn't shut down properly

cd ..
cd dev
cd shm
del pulse*

PAUSE
