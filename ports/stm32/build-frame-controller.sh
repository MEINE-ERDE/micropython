#!/bin/bash

# Build the frame controller

# Get commit hash of the current repository (BASE)
current_commit_hash=$(git rev-parse HEAD)
echo "Current repository commit hash: $current_commit_hash"

# Determine commit hash of the "reerdigung-frame-controller" repository (FRAME)
if [ -d "../../../reerdigung-frame-controller" ]; then
    pushd ../../../reerdigung-frame-controller > /dev/null
    reerdigung_commit_hash=$(git rev-parse HEAD)
    popd > /dev/null
    echo "reerdigung-frame-controller repository commit hash: $reerdigung_commit_hash"
else
    echo "Directory '../../../reerdigung-frame-controller' does not exist."
fi

rm -Rf build-ME_FRAME_CONTROLLER_H743 2>/dev/null || true

mkdir -p boards/ME_FRAME_CONTROLLER_H743
cp -r ../../../reerdigung-frame-controller/build-files/ME_FRAME_CONTROLLER_H743/* boards/ME_FRAME_CONTROLLER_H743/

# Pass the commit hashes via CFLAGS
make BOARD=ME_FRAME_CONTROLLER_H743 \
     CFLAGS_EXTRA="-DMICROPY_ME_FRAME_GIT_HASH='\"${reerdigung_commit_hash}\"' -DMICROPY_ME_BASE_GIT_HASH='\"${current_commit_hash}\"'"

