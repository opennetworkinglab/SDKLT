#!/bin/bash

set -e
set -x

export YAML=/usr
export YAML_LIBDIR=$YAML/src/.libs

export KDIR=$HOME/linux-4.14.49-OpenNetworkLinux

export TOOLCHAIN_DIR=/usr
export TARGET_ARCHITECTURE=""
export CROSS_COMPILE=""

export SDKLT=$HOME/project
export SDK=$SDKLT/src


# Add your flags here
# export CC=clang-3.9
# export LD=lld-3.9
# export ADD_CFLAGS="-g -fno-omit-frame-pointer -O0"

cd $SDK/appl/demo
make TARGET_PLATFORM=xlr_linux clean -j
make TARGET_PLATFORM=xlr_linux -j 4

tar czf $HOME/sdklt-4.14.49.tgz \
  -C $SDKLT/src/appl/linux/build/xlr_linux/lkm/knet/ linux_ngknet.ko \
  -C $SDKLT/src/appl/linux/build/xlr_linux/lkm/bde/ linux_ngbde.ko \
  -C $SDKLT/src/appl/demo/build/xlr_linux/ sdklt \
  -C $SDKLT/src/appl/sdklib/build/xlr_linux/ include/sdklt \
  -C $SDKLT/src/appl/sdklib/build/xlr_linux/ lib/libsdklt.a lib/libsdklt.so
