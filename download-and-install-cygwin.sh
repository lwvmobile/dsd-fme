#! /bin/bash
#
cdir=$(pwd)
clear
printf "Digital Speech Decoder: Florida Man Edition - Auto Installer For Cygwin\n
This will install the required packages, clone, build, and install DSD-FME only.
This has been tested on Cygwin x86-64 as of 20250211. This may take a while!\n
MBELib is considered a requirement on this build.
You must view the Patent Notice prior to continuing.
The Patent Notice can be found at the site below.
https://github.com/lwvmobile/mbelib#readme
Please confirm that you have viewed the patent notice by entering y below.\n\n"
read -p "Have you viewed the patent notice? y/N " ANSWER
ANSWER=$(printf "$ANSWER"|tr '[:upper:]' '[:lower:]')
if [ "$ANSWER" = "y" ]; then

  #is this needed?
  LD_LIBRARY_PATH=/usr/local/lib
  export LD_LIBRARY_PATH
  echo $LD_LIBRARY_PATH

  #ITPP
  cd $cdir
  printf "Installing itpp 4.3.1 from source http://sourceforge.net/projects/itpp/files/latest/download?source=files\n Please wait!\n"
  wget -O itpp-latest.tar.bz2 http://sourceforge.net/projects/itpp/files/latest/download?source=files
  tar xjf itpp-latest.tar.bz2
  cd itpp-4.3.1/
  #newer Cygwin has Cmake 4.1.3 (or newer) so we need to replace the line with cmake_minimum_required to a newer version
  #this doesn't seem to break anything, its just that cmake will refuse to run properly without a newer min version in it
  #I'm just going to use the version that is used for most all my other cmake projects, seems to still be okay with new cmake
  sed -i '/^cmake_minimum_required *( *VERSION *2\.8\.6 *)$/c\cmake_minimum_required(VERSION 3.10.2)' CMakeLists.txt
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  make install

  #MBELIB
  cd $cdir
  printf "Installing mbelib\n Please wait!\n"
  git clone https://github.com/lwvmobile/mbelib.git
  cd mbelib
  git checkout ambe_tones
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  make install
  cp cygmbe-1.dll /bin

  #CODEC2
  cd $cdir
  printf "Installing codec2\n Please wait!\n"
  git clone https://github.com/drowe67/codec2.git
  cd codec2
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  make install

  #RTL-SDR
  cd $cdir
  printf "Installing RTL-SDR\n Please wait!\n"
  git clone https://github.com/lwvmobile/rtl-sdr.git
  cd rtl-sdr
  mkdir build
  cd build
  cmake ..
  make -j $(nproc)
  make install

  #DSD-FME
  cd $cdir
  printf "Installing DSD-FME\n Please wait!\n"
  git clone https://github.com/lwvmobile/dsd-fme.git
  cd dsd-fme
  #git checkout aw_dev
  mkdir build
  cd build
  cmake -DCOLORSLOGS=OFF ..
  make -j $(nproc)
  make install
  cd $cdir

  #call the cyg_portable script
  sh dsd-fme/cyg_portable.sh

  printf "Any issues, Please report to either:\nhttps://github.com/lwvmobile/dsd-fme/issues or\nhttps://forums.radioreference.com/threads/dsd-fme.438137/\n\n"

else
  printf "Sorry, you cannot build DSD-FME without acknowledging the Patent Notice.\n\n"
fi