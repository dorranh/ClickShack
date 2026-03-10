#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
usage: $0 <command> [options]

commands:
  audit          validate manifest schema and fixture files
  list           enumerate fixture corpus

options:
  --manifest <path>   manifest path (required)
USAGE
}

manifest=""
command="${1:-}"
if [[ -z "${command}" ]]; then
  usage
  exit 2
fi
shift || true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --manifest)
      manifest="${2:-}"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ -z "${manifest}" ]]; then
  echo "--manifest is required" >&2
  exit 2
fi

manifest_dir="$(cd "$(dirname "${manifest}")" && pwd)"

audit_manifest() {
  python3 - "${manifest}" "${manifest_dir}" <<'PY'
import json
import os
import sys

manifest_path = sys.argv[1]
manifest_dir = sys.argv[2]

with open(manifest_path, encoding="utf-8") as f:
    data = json.load(f)

fixtures = data.get("fixtures")
if not isinstance(fixtures, list) or not fixtures:
    raise SystemExit("manifest fixtures must be a non-empty list")

required = [
    "id",
    "workload_source_id",
    "path",
    "priority",
    "clause_tags",
    "expected_result_kind",
    "expected_canonical_summary",
    "exclusion_reason",
]

seen_ids = set()
seen_paths = set()
seen_source_ids = set()

for item in fixtures:
    for key in required:
        if key not in item:
            raise SystemExit(f"fixture missing required field: {key}")

    fid = item["id"]
    if fid in seen_ids:
        raise SystemExit(f"duplicate fixture id: {fid}")
    seen_ids.add(fid)

    source_id = item["workload_source_id"]
    if source_id in seen_source_ids:
        raise SystemExit(f"duplicate workload_source_id: {source_id}")
    seen_source_ids.add(source_id)

    rel_path = item["path"]
    if rel_path in seen_paths:
        raise SystemExit(f"duplicate fixture path: {rel_path}")
    seen_paths.add(rel_path)

    fixture_path = os.path.join(manifest_dir, rel_path)
    if not os.path.isfile(fixture_path):
        raise SystemExit(f"fixture file not found: {rel_path}")

    with open(fixture_path, encoding="utf-8") as qf:
        if not qf.read().strip():
            raise SystemExit(f"fixture file is empty: {rel_path}")

    kind = item["expected_result_kind"]
    summary = item["expected_canonical_summary"].strip()
    reason = item["exclusion_reason"].strip()

    if kind == "success":
        if not summary:
            raise SystemExit(f"success fixture missing expected_canonical_summary: {fid}")
        if reason:
            raise SystemExit(f"success fixture must not set exclusion_reason: {fid}")
    elif kind == "excluded":
        if summary:
            raise SystemExit(f"excluded fixture must not set expected_canonical_summary: {fid}")
        if not reason:
            raise SystemExit(f"excluded fixture missing exclusion_reason: {fid}")
    else:
        raise SystemExit(f"expected_result_kind must be success|excluded: {fid}")

success_count = sum(1 for f in fixtures if f["expected_result_kind"] == "success")
excluded_count = sum(1 for f in fixtures if f["expected_result_kind"] == "excluded")
print(f"audit ok: total={len(fixtures)} success={success_count} excluded={excluded_count}")
PY
}

list_manifest() {
  python3 - "${manifest}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as f:
    data = json.load(f)

rows = sorted(data["fixtures"], key=lambda r: (r["priority"], r["expected_result_kind"], r["id"]))
print("id\tkind\tpriority\tworkload_source_id\tpath\tclause_tags")
for row in rows:
    tags = ",".join(row.get("clause_tags", []))
    print(f"{row['id']}\t{row['expected_result_kind']}\t{row['priority']}\t{row['workload_source_id']}\t{row['path']}\t{tags}")
PY
}

case "${command}" in
  audit)
    audit_manifest
    ;;
  list)
    list_manifest
    ;;
  *)
    echo "unknown command: ${command}" >&2
    usage
    exit 2
    ;;
esac
