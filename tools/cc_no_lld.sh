#!/usr/bin/env bash
set -euo pipefail

# rules_cc probes -fuse-ld={lld,gold}. On Apple Clang this can be misdetected
# as supported and emit GNU-only linker flags that fail at link time.
for arg in "$@"; do
  case "$arg" in
    -fuse-ld=lld|-fuse-ld=gold|-fuse-ld=ld64.lld:|-fuse-ld=ld64.lld)
      exit 1
      ;;
  esac
done

exec /usr/bin/cc "$@"
