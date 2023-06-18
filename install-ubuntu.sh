sudo apt-get update
sudo apt-get --yes install libsystemd-dev make g++ kdialog pkg-config cmake
make clean
make
sudo systemctl stop scheduled-poweroff
sudo make install
sudo systemctl enable scheduled-poweroff
sudo systemctl start scheduled-poweroff
