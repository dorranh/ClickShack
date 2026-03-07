#!/usr/bin/env bash
set -euo pipefail

tmp_refs="$(rg -n '(^|["'"'"' /])tmp/' \
  MODULE.bazel \
  MODULE.bazel.lock \
  .bazelrc \
  examples/bootstrap \
  ported_clickhouse \
  tools \
  --glob '*.bzl' \
  --glob 'BUILD.bazel' 2>/dev/null || true)"

if [[ -n "${tmp_refs}" ]]; then
  printf 'tracked build/config files reference tmp/:\n%s\n' "${tmp_refs}" >&2
  exit 1
fi

tracked_tmp_count="$(git ls-files tmp | wc -l | tr -d ' ')"
if [[ "${tracked_tmp_count}" != "0" ]]; then
  printf 'tracked tmp/ entries detected: %s\n' "${tracked_tmp_count}" >&2
  exit 1
fi
