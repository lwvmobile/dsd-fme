#!/bin/bash
#
mkdir build
cd build
cmake ..
make -j $(nproc)
sudo make install
sudo ldconfig