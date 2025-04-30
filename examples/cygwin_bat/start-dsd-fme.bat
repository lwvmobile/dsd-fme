@REM Digital Speech Decoder: Florida Man Edition
@REM DSD-FME Cygwin Windows Portable Builds
@REM
@REM The source code of this software can be downloaded here:
@REM https://github.com/lwvmobile/dsd-fme
@REM
@REM If you paid for, or were otherwise fleeced for this software, DEMAND YOUR MONEY BACK!
@REM
@REM This software is designed to be used with an SDR receiver (like SDR++ or SDR#) with TCP input,
@REM RTL Input, media file playback over virtual cables or null-sinks, or a Discriminator Tap input
@REM
@REM The complete options list can be seen by viewing complete_usage_options.txt
@REM Many examples can be found in example_options.txt
@REM
@REM Enjoy!

@REM Regarding the options string (or any string in .bat files)
@REM If your string may contain spaces, set the first '"' before options, and then an ending '"' 
@REM Incorrect: set options="-fs -N -Z"
@REM Correct: set "options=-fs -N -Z"

@REM set options to pass to dsd-fme
set "options= -N -Z "

@REM Set Date Time for log (sourced from: https://stackoverflow.com/questions/1192476/format-date-and-time-in-a-windows-batch-script)
@echo off
REM "%date: =0%" replaces spaces with zeros
set d=%date: =0%
REM "set yyyy=%d:~-4%" pulls the last 4 characters
set yyyy=%d:~-4%
set mm=%d:~4,2%
set dd=%d:~7,2%
set dow=%d:~0,3%
set d=%yyyy%%mm%%dd%

set t=%TIME: =0%
REM "%t::=%" removes semi-colons
REM Instead of above, you could use "%t::=-%" to 
REM replace semi-colons with hyphens (or any 
REM non-special character)
set t=%t::=%
set t=%t:.=%

set datetimestr=%d%_%t%
@REM @echo Long date time str = %datetimestr%

@REM random number is used so that two log files don't share the same name
set rnd=%RANDOM%

@REM set log file relative filepath
set "clog=.\logs\console_log_%datetimestr%_%rnd%.txt"
set "elog=.\logs\event_log_%datetimestr%_%rnd%.txt"

@REM create the log file now with touch
.\dsd-fme\touch.exe %clog%
.\dsd-fme\touch.exe %elog%

@REM Launch Tail to display the console log and event log in a seperate console windows
start .\dsd-fme\tail.exe -n 40 -f %clog%
start .\dsd-fme\tail.exe -n 40 -f %elog%

@REM start dsd-fme with options and logs 
.\dsd-fme\dsd-fme.exe %options% -J %elog% 2> %clog%

echo ----------------------------------------------------------------------------------
echo ----------------------------------------------------------------------------------
echo For any errors, see: %clog% 
echo Forward %clog% and Options: "%options%" 
echo to developer on Github or Radio Reference for troubleshooting.
echo ----------------------------------------------------------------------------------
echo ----------------------------------------------------------------------------------

@REM Set pause to diplay above message
PAUSE
