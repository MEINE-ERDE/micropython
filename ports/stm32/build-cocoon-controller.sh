#! /bin/bash

# Build the frame controller

rm -Rf build-ME_COCOON_CONTROLLER_F405 2>/dev/null || true


mkdir -p boards/ME_COCOON_CONTROLLER_F405
cp -r ../../../reerdigung-cocoon-controller/build-files/ME_COCOON_CONTROLLER_F405/ boards/ME_COCOON_CONTROLLER_F405/

make BOARD=ME_COCOON_CONTROLLER_F405