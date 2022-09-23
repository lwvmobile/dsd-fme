#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition Auto Installer 
echo This will clone, build, and install DSD-FME only.
echo If you need dependencies installed, please run download-and-install.sh instead. 
echo

read -p "Press enter to continue"

git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
git branch -a
git checkout remotes/origin/pulseaudio
git checkout -b pulseaudio
git branch -a #double check to see if you are on pulseaudio branch
sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
mkdir build
cd build
cmake ..
make -j `nproc`
##only run make install if you don't have another version already installed##
sudo make install
sudo ldconfig
