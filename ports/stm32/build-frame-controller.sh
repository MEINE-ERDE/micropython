#! /bin/bash

# Build the frame controller

rm -Rf build-ME_FRAME_CONTROLLER_H743 2>/dev/null || true


mkdir -p boards/ME_FRAME_CONTROLLER_H743
cp -r ../../../reerdigung-frame-controller/build-files/ME_FRAME_CONTROLLER_H743/* boards/ME_FRAME_CONTROLLER_H743/

make BOARD=ME_FRAME_CONTROLLER_H743

