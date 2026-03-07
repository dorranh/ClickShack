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
    PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "${quick_targets[@]}"
    ;;
  full)
    PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "${full_targets[@]}"
    ;;
  suite)
    PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "${quick_targets[@]}"
    PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "${full_targets[@]}"
    ;;
  *)
    echo "usage: $0 [quick|full|suite]" >&2
    exit 2
    ;;
esac
