#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition Auto Installer 
echo This will clone, build, and install DSD-FME only.
echo If you need dependencies installed, please run download-and-install.sh instead. 
echo

read -p "Press enter to continue"

git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo cp tone48.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
sudo chmod 777 /usr/share/tone48.wav
mkdir build
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
