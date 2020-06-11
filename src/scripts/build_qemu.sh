#!/bin/bash

echo "Downloading QEMU"
savedDir=$PWD
cd $HYPERSIO_HOME/build
wget https://download.qemu.org/qemu-3.0.0.tar.bz2
echo "Untarring QEMU"
tar xf qemu-3.0.0.tar.bz2
cd qemu-3.0.0
echo "Patching QEMU"
patch -p0 < ../../src/scripts/qemu_patch.diff
mkdir build
cd build
echo "Configuring QEMU"
../configure --target-list=x86_64-softmmu
echo "Bulding QEMU"
make -j4
echo "Done!"
cd $savedDir
