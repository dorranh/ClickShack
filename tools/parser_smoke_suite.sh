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

# Check that expected artifacts already exist in bazel-bin/ without rebuilding.
# Used in CI where a prior bazel build step has already produced the artifacts.
check_artifacts() {
  local missing=0
  for target in "$@"; do
    local binary
    binary="$(target_to_binary "${target}")"
    if [[ ! -x "${binary}" ]]; then
      echo "missing expected artifact: ${binary}" >&2
      missing=1
    fi
  done
  if [[ ${missing} -ne 0 ]]; then
    return 1
  fi
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

# Parse --skip-build flag (must precede the mode argument).
skip_build=0
if [[ "${1:-}" == "--skip-build" ]]; then
  skip_build=1
  shift
fi

mode="${1:-suite}"

if [[ ${skip_build} -eq 1 ]]; then
  case "${mode}" in
    quick)
      check_artifacts "${quick_targets[@]}"
      ;;
    full)
      check_artifacts "${full_targets[@]}"
      ;;
    suite)
      result=0
      check_artifacts "${quick_targets[@]}" || result=$?
      check_artifacts "${full_targets[@]}" || result=$?
      exit "${result}"
      ;;
    *)
      echo "usage: $0 [--skip-build] [quick|full|suite]" >&2
      exit 2
      ;;
  esac
else
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
      echo "usage: $0 [--skip-build] [quick|full|suite]" >&2
      exit 2
      ;;
  esac
fi
