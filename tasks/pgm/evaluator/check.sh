#!/bin/bash
# Builds /home/dev/PGM-index against an untouched copy and compares it with the upstream implementation.
D=/opt/pgm-tools
exec python3 "$D/check.py" --repo "${1:-/home/dev/PGM-index}" --upstream "$D/upstream" --harness "$D/harness.cpp" --data "$D/data" --counts "$D/upstream_counts.json"
