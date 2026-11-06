#!/usr/bin/env bash
# Runs Harbor on a Docker engine that can enforce the tasks' agent network block.
#
# The block needs CONFIG_NFT_FIB_INET in the engine's kernel. Docker on Linux normally has it, so this
# is a plain `harbor` call there. Docker Desktop's VM lacks it, so on a Mac this falls back to a Colima
# VM for this command only; the default Docker context is left alone.
#
# Usage: scripts/harbor.sh run -p tasks/pgm -a <agent> [...]
set -euo pipefail

# Same test as Harbor's preflight, which passes when the kernel config is not exposed.
supports_network_block() {
  docker run --rm alpine:3.20 sh -c \
    "[ ! -f /proc/config.gz ] || zcat /proc/config.gz | grep -qE '^CONFIG_NFT_FIB_INET=[ym]'" \
    >/dev/null 2>&1
}

if ! supports_network_block; then
  if ! command -v colima >/dev/null; then
    echo "This Docker engine can't block agent internet access. On a Mac: brew install colima" >&2
    exit 1
  fi
  colima status >/dev/null 2>&1 || colima start --activate=false --cpu 4 --memory 8 --disk 60
  export DOCKER_CONTEXT=colima
  if ! supports_network_block; then
    echo "Colima's Docker engine can't block agent internet access either." >&2
    exit 1
  fi
  echo "Using Colima: this Docker engine can't block agent internet access." >&2
fi

exec harbor "$@"
