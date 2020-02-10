#!/bin/bash

set -e
set -x

export YAML=/usr
export YAML_LIBDIR=$YAML/src/.libs

KERNEL_VERSION=${KERNEL_VERSION:-4.14.49}
export KDIR=$HOME/linux-${KERNEL_VERSION}-OpenNetworkLinux

export TOOLCHAIN_DIR=/usr
export TARGET_ARCHITECTURE=""
export CROSS_COMPILE=""

export SDKLT=$HOME/project
export SDK=$SDKLT/src


# Add your flags here
# export CC=clang-3.9
# export LD=lld-3.9
# export ADD_CFLAGS="-g -fno-omit-frame-pointer -O0"
# export ADD_LDFLAGS=""

cd $SDK/appl/demo
make TARGET_PLATFORM=xlr_linux clean -j
make TARGET_PLATFORM=xlr_linux -j 2

TAR_FOLDER=/tmp/sdklt-${KERNEL_VERSION}
mkdir -p ${TAR_FOLDER}

cp -r $SDKLT/src/appl/linux/build/xlr_linux/lkm/knet/linux_ngknet.ko \
      $SDKLT/src/appl/linux/build/xlr_linux/lkm/bde/linux_ngbde.ko \
      $SDKLT/src/appl/demo/build/xlr_linux/sdklt \
      $SDKLT/src/appl/sdklib/build/xlr_linux/include \
      $SDKLT/src/appl/sdklib/build/xlr_linux/lib \
      ${TAR_FOLDER}

tar czf $HOME/sdklt-${KERNEL_VERSION}.tgz \
  -C /tmp sdklt-${KERNEL_VERSION}
