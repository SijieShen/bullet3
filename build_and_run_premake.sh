#!/bin/sh
cd build3
./premake4_linux64 gmake
./premake4_osx gmake
cd gmake
make -j12 --double --enable_pybullet
../../bin/App_BulletExampleBrowser_gmake_x64_release
