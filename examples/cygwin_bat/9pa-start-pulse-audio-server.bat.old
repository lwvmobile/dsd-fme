@REM Set the console / terminal to 90 columns, 50 lines
@REM mode con: cols=90 lines=50

@ECHO OFF

@REM change directory into the dsd-fme folder
cd dsd-fme

@REM Start the Pulse Audio backend with a 10 minute time out
@REM start pulse audio server with 600 second idle time and no CPU limit (prevent closing while idle in menu)

@REM output from pulse server routed to NUL to supress "capabilities dropped, nag messages, etc" messages
pulseaudio.exe --start --no-cpu-limit=TRUE --exit-idle-time=600 2> NUL

@REM End pulse start up

ECHO The Pulse Audio Server should now be active!

@REM Setup Some null-sinks (if desired, remove @REM below)

@REM pactl load-module module-null-sink sink_name=virtual_sink1  sink_properties=device.description=Virtual_Sink1
@REM pactl load-module module-null-sink sink_name=virtual_sink2  sink_properties=device.description=Virtual_Sink2

@REM Setup Default Sink to device 0 (output)

pactl set-default-sink 0

@REM List Avaiable Pulse Audio Sinks and Sources

@ECHO ON

dsd-fme -O

PAUSE
