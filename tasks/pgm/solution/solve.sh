#!/bin/bash
# Oracle: apply the upstream optimal segmentation (reference.patch) to the stripped tree.
set -euo pipefail
patch -d /home/dev/PGM-index -p4 < "$(dirname "$0")/reference.patch"
