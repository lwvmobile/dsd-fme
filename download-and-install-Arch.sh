#! /bin/bash
#
cdir=$(pwd)
clear
printf "Digital Speech Decoder: Florida Man Edition - Auto Installer For Arch Linux\n
This will install the required packages, clone, build, and install DSD-FME only.
This has been tested on Arch 2023.08.01 and Manjaro XFCE 22.1.3 Minimal.\n
MBELib is considered a requirement on this build.
You must view the Patent Notice prior to continuing.
The Patent Notice can be found at the site below.
https://github.com/lwvmobile/mbelib#readme
Please confirm that you have viewed the patent notice by entering y below.\n\n"
read -p "Have you viewed the patent notice? y/N " ANSWER
ANSWER=$(printf "$ANSWER"|tr '[:upper:]' '[:lower:]')
if [ "$ANSWER" = "y" ]; then

  sudo pacman -Syu #always run a full update first, partial upgrades aren't supported in Arch -- including downloading dependencies, that may require an updated dependency, and breaks same dependency on another package, a.k.a, dependency hell
  sudo pacman -S libpulse cmake ncurses lapack perl fftw rtl-sdr codec2 base-devel libsndfile git wget

  printf "Installing itpp 4.3.1 from Arch Strike --Tested on Arch 2023.08.01 and Manjaro XFCE 22.1.3 Minimal\n"
  wget https://mirror.archstrike.org/x86_64/archstrike/itpp-4.3.1-3-x86_64.pkg.tar.xz
  sudo pacman -U itpp-4.3.1-3-x86_64.pkg.tar.xz
  sudo rm -R itpp-4.3.1-3-x86_64.pkg.tar.xz

  git clone https://github.com/lwvmobile/mbelib
  cd mbelib
  git checkout ambe_tones
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  sudo make install
  sudo ldconfig
  sudo cp /usr/local/lib/libmbe.so /usr/lib/libmbe.so 
  cd $cdir

  git clone https://github.com/lwvmobile/dsd-fme
  ### For branch audio_work - git clone --branch audio_work https://github.com/lwvmobile/dsd-fme
  cd dsd-fme
  #git checkout audio_work
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  sudo make install
  sudo ldconfig

  printf "Any issues, Please report to either:\nhttps://github.com/lwvmobile/dsd-fme/issues or\nhttps://forums.radioreference.com/threads/dsd-fme.438137/\n\n"

else
  printf "Sorry, you cannot build DSD-FME without acknowledging the Patent Notice.\n\n"
fi