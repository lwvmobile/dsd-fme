#!/bin/bash
# Force bash for better compatibility (I think)
#
# Digital Speech Decoder: Florida Man Edition
#
# The source code of this software can be downloaded here:
# https://github.com/lwvmobile/dsd-fme
#
# If you paid for, or were otherwise fleeced for this software, DEMAND YOUR MONEY BACK!
#
# This software is designed to be used with an SDR receiver (like SDR++ or SDR#) with TCP input,
# RTL Input, media file playback over virtual cables or null-sinks, or a Discriminator Tap input
#
# The complete options list can be seen by viewing complete_usage_options.ans
# Many examples can be found in example_options.ans
#
# Enjoy!
###
#### IMPORTANT!  Set your dsd-fme options here! ####
# Example : opts=' -ft -i rtl:0:460.350M:0:-1:8 -N -Z '
####
# set options to pass to dsd-fme
opts=' -N -Z '
####
# Initialize variables
xe="0"
gt="0"
xt="0"
ko="0"
# Functions to check for available terminals
chk_xe(){
# Check if x-terminal-emulator is installed
xe=$(which x-terminal-emulator 2>/dev/null)
if [ -z $xe ];then xe="0";else xe="1";fi
}
chk_gt(){
# Check if gnome-terminal is installed
gt=$(which gnome-terminal 2>/dev/null)
if [ -z $gt ];then gt="0";else gt="1";fi
}
chk_ko(){
# Check if Konsole is installed
ko=$(which konsole 2>/dev/null)
if [ -z $ko ];then ko="0";else ko="1";fi
}
chk_xt(){
# Check if xterm is installed
xt=$(which xterm 2>/dev/null)
if [ -z $xt ];then xt="0";else xt="1";fi
}
# Check for and set terminal
chk_xe
if [ $xe = "1" ];then
  term=$(which x-terminal-emulator 2>/dev/null)
  printf "Using X-Terminal-Emulator!\n"
else
  chk_gt
fi
if [ $gt = "1" ];then
  term=$(which gnome-terminal 2>/dev/null)
  printf "Using Gnome-Terminal!\n"
elif [ $xe = "0" ]&&[ $gt = "0" ];then
  chk_xt
fi
if [ $xt = "1" ];then
  term=$(which xterm 2>/dev/null)
  printf "Using XTerm!\n"
elif [ $xe = "0" ]&&[ $gt = "0" ]&&[ $xt = "0" ];then
  chk_ko
fi
if [ $ko = "1" ];then
  term=$(which konsole 2>/dev/null)
  printf "Using Konsole!\n"
fi
# Bail if no terminal found
if [ "$ko" -eq "0" ]&&[ "$gt" -eq "0" ]&&[ "$xt" -eq "0" ]&&[ "$xe" -eq "0" ];then printf "No known terminals available!\n";exit 1;fi
# Get current directory
wd=$(pwd)
# Set date/time
# dt=$(date +%F_%H%M-%S-%N) #with hyphens
dt=$(date +%Y%m%d_%H%M%S) #just an underscore between date and time
if [ ! -d "logs" ]; then
  mkdir logs
fi
# set log file relative filepath
clog="$wd/logs/console_log_$dt.txt"
elog="$wd/logs/event_log_$dt.txt"
# create the log file now with touch
touch $clog
touch $elog
# open new terminal windows to watch live log files
if [ "$term" == "$(which konsole)" ]||[ "$term" == "$(which xterm)" ];then
  $term -e "tail -n 40 -f $clog" 2>/dev/null
  $term -e "tail -n 40 -f $elog" 2>/dev/null
else
  $term --window -e "tail -n 40 -f $clog" 2>/dev/null
  $term --window -e "tail -n 40 -f $elog" 2>/dev/null
fi
# start dsd-fme with options and log files
dsd-fme $opts -J $elog 2> $clog
echo -------------------------------------------------------------------------------
echo -------------------------------------------------------------------------------
echo For any errors, see: $clog 
echo Forward $clog and 
echo Options: \" $opts \"
echo to developer on Github or Radio Reference for troubleshooting.
echo -------------------------------------------------------------------------------
echo -------------------------------------------------------------------------------

# Set pause to diplay above message
read -p "Press ENTER to continue... " x

