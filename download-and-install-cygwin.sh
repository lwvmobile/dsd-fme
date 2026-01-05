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