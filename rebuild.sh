#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition
echo Automatic Git Pull and Rebuild
echo 
sleep 1
##Open your clone folder##
git pull https://github.com/lwvmobile/dsd-fme pulseaudio
sleep 2
sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
##cd into your build folder##
cd build
##cmake usually isn't necesary, but could be if I update the cmakelist.txt
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig

