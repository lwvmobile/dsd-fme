#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition
echo Cygwin Automatic Git Pull and Rebuild
echo 
sleep 1
##Open your clone folder##
git pull
sleep 2
##cd into your build folder##
cd build
##cmake usually isn't necesary, but could be if I update the cmakelist.txt
cmake ..
make -j `nproc`
make install


