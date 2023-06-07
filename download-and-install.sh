#! /bin/bash
clear
echo DSD-FME Digital Speech Decoder - Florida Man Edition Auto Installer
echo MBELib is considered a requirement on this build.
echo You must view this notice prior to continuing. 
echo The Patent Notice can be found at the site below.
echo https://github.com/lwvmobile/mbelib#readme
echo Please confirm that you have viewed the patent notice by entering y below.
echo
echo y/N
read ANSWER
Y='y'
if [[ $Y == $ANSWER ]]; then

sudo apt update
sudo apt install libpulse-dev pavucontrol libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev libncurses5 libncurses5-dev rtl-sdr librtlsdr-dev libusb-1.0-0-dev cmake git wget make build-essential libitpp-dev libncursesw5-dev

echo ITPP Manual Build and Install has been temporarily removed from this script due to issues with older versions of Ubuntu.
echo Please check to see that libitpp-dev has successfully been downloaded and installed from the repo.
read -p "Press enter to continue"

#wget -O itpp-latest.tar.bz2 http://sourceforge.net/projects/itpp/files/latest/download?source=files
#tar xjf itpp*
#if you can't cd into this folder, double check folder name first
#cd itpp-4.3.1 
#mkdir build
#cd build
#cmake ..
#make -j `nproc`
#sudo make install
#sudo ldconfig
#cd ..
#cd ..

git clone https://github.com/lwvmobile/mbelib
cd mbelib
mkdir build
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
cd ..
cd ..

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

else
echo
echo Sorry, you cannot build DSD-FME without acknowledging the Patent Notice.
fi



