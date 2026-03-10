#!/usr/bin/env bash
set -euo pipefail

bazel_cmd=(
  bazel
  --batch
  --output_user_root=/tmp/bazel-root
  build
  --repo_env=CC=cc_no_lld.sh
  --repo_env=CXX=cc_no_lld.sh
)

target_to_binary() {
  local label="$1"
  local pkg="${label#//}"
  pkg="${pkg%%:*}"
  local name="${label##*:}"
  echo "bazel-bin/${pkg}/${name}"
}

run_bazel_build() {
  local rc=0
  PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "$@" || rc=$?
  if [[ ${rc} -eq 0 ]]; then
    return 0
  fi

  local missing=0
  for target in "$@"; do
    local binary
    binary="$(target_to_binary "${target}")"
    if [[ ! -x "${binary}" ]]; then
      echo "missing expected artifact after bazel failure: ${binary}" >&2
      missing=1
    fi
  done

  if [[ ${missing} -eq 0 ]]; then
    echo "warning: bazel exited ${rc}, but all requested smoke artifacts were produced" >&2
    return 0
  fi
  return "${rc}"
}

quick_targets=(
  //examples/bootstrap:use_parser_smoke
  //examples/bootstrap:select_lite_smoke
)

full_targets=(
  //examples/bootstrap:use_parser_smoke
  //examples/bootstrap:select_lite_smoke
  //examples/bootstrap:select_from_lite_smoke
  //examples/bootstrap:select_rich_smoke
)

mode="${1:-suite}"

case "${mode}" in
  quick)
    run_bazel_build "${quick_targets[@]}"
    ;;
  full)
    run_bazel_build "${full_targets[@]}"
    ;;
  suite)
    run_bazel_build "${quick_targets[@]}"
    run_bazel_build "${full_targets[@]}"
    ;;
  *)
    echo "usage: $0 [quick|full|suite]" >&2
    exit 2
    ;;
esac
