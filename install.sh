sudo cp tone8.wav /usr/share/
sudo cp tone24.wav /usr/share/
sudo chmod 777 /usr/share/tone8.wav
sudo chmod 777 /usr/share/tone24.wav
mkdir build
cd build
cmake ..
make -j `nproc`
sudo make install
sudo ldconfig
