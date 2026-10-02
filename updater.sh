#!/bin/bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

# Update only by fast-forward so an unattended update cannot create a merge.
git pull --ff-only

# Use the exact dependency revisions pinned by the updated firmware commit.
git submodule sync --recursive
git submodule update --init --recursive --checkout

./build.sh -s
systemctl restart OpenSprinkler.service
