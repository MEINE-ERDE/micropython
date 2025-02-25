#!/bin/bash

# Build the cocoon controller

# Get commit hash of the current repository (BASE)
current_commit_hash=$(git rev-parse HEAD)
echo "Current repository commit hash: $current_commit_hash"

# Determine commit hash of the "reerdigung-cocoon-controller" repository (COCOON)
if [ -d "../../../reerdigung-cocoon-controller" ]; then
    pushd ../../../reerdigung-cocoon-controller > /dev/null
    reerdigung_commit_hash=$(git rev-parse HEAD)
    popd > /dev/null
    echo "reerdigung-cocoon-controller repository commit hash: $reerdigung_commit_hash"
else
    echo "Directory '../../../reerdigung-cocoon-controller' does not exist."
fi

rm -Rf build-ME_COCOON_CONTROLLER_F405 2>/dev/null || true

mkdir -p boards/ME_COCOON_CONTROLLER_F405
cp -r ../../../reerdigung-cocoon-controller/build-files/ME_COCOON_CONTROLLER_F405/* boards/ME_COCOON_CONTROLLER_F405/

# Pass the commit hashes via CFLAGS
make BOARD=ME_COCOON_CONTROLLER_F405 \
     CFLAGS_EXTRA="-DMICROPY_ME_COCOON_GIT_HASH='\"${reerdigung_commit_hash}\"' -DMICROPY_ME_BASE_GIT_HASH='\"${current_commit_hash}\"'"
