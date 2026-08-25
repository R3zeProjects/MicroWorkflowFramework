#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
for attempt in 1 2 3; do
    if sudo apt-get update -o Acquire::Retries=3 &&
       sudo apt-get install -y --no-install-recommends -o Acquire::Retries=3 "$@"; then
        exit 0
    fi
    if (( attempt == 3 )); then
        exit 1
    fi
    sleep $((attempt * 5))
done
