#! /bin/bash
#
cdir=$(pwd)
clear
printf "Digital Speech Decoder: Florida Man Edition - Auto Installer\n
MBELib is considered a requirement on this build.
You must view the Patent Notice prior to continuing.
The Patent Notice can be found at the site below.
https://github.com/lwvmobile/mbelib#readme
Please confirm that you have viewed the patent notice by entering y below:\n\n"
read -p "Have you viewed the patent notice? y/N " ANSWER
ANSWER=$(printf "$ANSWER"|tr '[:upper:]' '[:lower:]')
if [ "$ANSWER" = "y" ]; then
  sudo apt update
  sudo apt install libpulse-dev pavucontrol libsndfile1-dev libfftw3-dev liblapack-dev socat libusb-1.0-0-dev rtl-sdr librtlsdr-dev libusb-1.0-0-dev cmake git wget make build-essential libitpp-dev libncurses-dev libncurses6 libcodec2-dev
  chkitpp=$(sudo apt list libitpp-dev 2>&1|grep -ci "installed")
  if [ "$chkitpp" -lt "1" ];then 
    printf "\nFor some reason, ITPP did NOT install!\nYou may have to manually install it.\n\n"
    read -p "Press Enter to exit..." x
    exit 1
  fi
  git clone https://github.com/lwvmobile/mbelib
  cd mbelib
  git checkout ambe_tones
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  sudo make install
  sudo ldconfig
  cd $cdir
  git clone https://github.com/lwvmobile/dsd-fme
  cd dsd-fme
  #git checkout audio_work
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  sudo make install
  sudo ldconfig
else
  printf "\nSorry, you cannot build DSD-FME without acknowledging the Patent Notice.\nExiting...\n\n"
  exit 2
fi