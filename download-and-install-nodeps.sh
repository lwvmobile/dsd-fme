#! /bin/bash
clear
printf "\nDigital Speech Decoder: Florida Man Edition - Auto Installer\n
This will clone, build, and install DSD-FME only.
If DSD-FME has never been installed on this machine, 
then you will need to install the dependencies first!
Please run download-and-install.sh instead.\n\n"
read -p "Press Enter to continue..." x
git clone https://github.com/lwvmobile/dsd-fme
cd dsd-fme
git checkout audio_work
mkdir build
cd build
cmake ..
make -j $(nproc)
sudo make install
sudo ldconfig