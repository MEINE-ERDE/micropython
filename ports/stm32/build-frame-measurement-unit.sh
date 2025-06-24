#!/bin/bash

# Build the frame-measurement-unit

# Get commit hash of the current repository (BASE)
current_commit_hash=$(git rev-parse HEAD)
echo "Current repository commit hash: $current_commit_hash"

# Determine commit hash of the "reerdigung-frame-measurement-unit" repository (FRAME_MEASUREMENT_UNIT)
if [ -d "../../../reerdigung-frame-measurement-unit" ]; then
    pushd ../../../reerdigung-frame-measurement-unit > /dev/null
    reerdigung_commit_hash=$(git rev-parse HEAD)
    popd > /dev/null
    echo "reerdigung-frame-measurement-unit repository commit hash: $reerdigung_commit_hash"
else
    echo "Directory '../../../reerdigung-frame-measurement-unit' does not exist."
fi

rm -Rf build-ME_FRAME_MEASUREMENT_UNIT_F405 2>/dev/null || true

mkdir -p boards/ME_FRAME_MEASUREMENT_UNIT_F405
cp -r ../../../reerdigung-frame-measurement-unit/build-files/ME_FRAME_MEASUREMENT_UNIT_F405/* boards/ME_FRAME_MEASUREMENT_UNIT_F405/

# Pass the commit hashes via CFLAGS
make BOARD=ME_FRAME_MEASUREMENT_UNIT_F405 \
     CFLAGS_EXTRA="-DMICROPY_ME_COCOON_GIT_HASH='\"${reerdigung_commit_hash}\"' -DMICROPY_ME_BASE_GIT_HASH='\"${current_commit_hash}\"'"
