#!/usr/bin/env bash
set -euo pipefail

args=()
tmpfiles=()
cleanup() {
  if ((${#tmpfiles[@]} > 0)); then
    rm -f "${tmpfiles[@]}"
  fi
}
trap cleanup EXIT

for arg in "$@"; do
  if [[ "$arg" == "-fuse-ld=ld64.lld:" || "$arg" == "-fuse-ld=ld64.lld" ]]; then
    arg="-fuse-ld=lld"
  fi
  if [[ "$arg" == @* ]]; then
    param_file="${arg#@}"
    if [[ -f "$param_file" ]]; then
      tmpfile="$(mktemp)"
      sed -e 's/-fuse-ld=ld64\.lld:/-fuse-ld=lld/g' -e 's/-fuse-ld=ld64\.lld/-fuse-ld=lld/g' "$param_file" >"$tmpfile"
      tmpfiles+=("$tmpfile")
      arg="@$tmpfile"
    fi
  fi
  args+=("$arg")
done

exec /usr/bin/clang "${args[@]}"
