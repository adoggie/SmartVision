#!/bin/sh
export AS=arm-linux-androideabi-as
export LDFLAGS=--sysroot=/smart/android-ndk-r13b/platforms/android-22/arch-arm/
export CPPFLAGS=--sysroot=/smart/android-ndk-r13b/platforms/android-22/arch-arm/
export CXXFLAGS=--sysroot=/smart/android-ndk-r13b/platforms/android-19/arch-arm/
export CPP=arm-linux-androideabi-cpp
export PATH=/smart/android-ndk-r13b/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/:$PATH
export LD=arm-linux-androideabi-ld
export CXX=arm-linux-androideabi-g++
export CFLAGS=--sysroot=/smart/android-ndk-r13b/platforms/android-19/arch-arm/
export RANLIB=arm-linux-androideabi-ranlib
export CC=arm-linux-androideabi-gcc
export PS1="\[\e[32;1m\][linux-androidkit]\[\e[0m\]:\w> "
