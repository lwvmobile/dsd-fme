#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition
echo Automatic Git Pull and Rebuild
echo 
sleep 1
##Open your clone folder##
git pull
sleep 2
##cd into your build folder##
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig

