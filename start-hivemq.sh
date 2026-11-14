#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v docker >/dev/null 2>&1; then
  if [[ -x "$HOME/.docker/bin/docker" ]]; then
    PATH="$HOME/.docker/bin:$PATH"
    export PATH
  elif [[ -x "/Applications/Docker.app/Contents/Resources/bin/docker" ]]; then
    PATH="/Applications/Docker.app/Contents/Resources/bin:$PATH"
    export PATH
  else
    echo "Docker CLI not found. Start Docker Desktop and confirm its CLI tools are installed."
    exit 1
  fi
fi
if ! docker info >/dev/null 2>&1; then
  echo "Docker Desktop is installed but its engine is not running. Open Docker Desktop and wait for it to start."
  exit 1
fi

printf 'Enter the rotated HiveMQ license key (input hidden): '
IFS= read -r -s HIVEMQ_LICENSE_KEY
printf '\n'
if [[ -z "$HIVEMQ_LICENSE_KEY" ]]; then
  echo "No license key entered. Nothing started."
  exit 1
fi
export HIVEMQ_LICENSE_KEY
trap 'unset HIVEMQ_LICENSE_KEY' EXIT
docker compose up -d
docker compose ps
docker compose logs --tail=40 hivemq
