#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
usage: $0 <command> [options]

commands:
  audit          validate manifest schema and fixture files
  list           enumerate fixture corpus
  reconcile      compare manifest mappings against source snapshot
  audit-signoff  verify sign-off metadata matches reconciliation totals

options:
  --manifest <path>   manifest path (required)
  --source <path>     workload-source snapshot json (reconcile/audit-signoff)
  --signoff <path>    reconciliation markdown doc (reconcile/audit-signoff)
USAGE
}

manifest=""
source=""
signoff=""
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
    --source)
      source="${2:-}"
      shift 2
      ;;
    --signoff)
      signoff="${2:-}"
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

reconcile_core() {
  local mode="$1"
  if [[ -z "${source}" || -z "${signoff}" ]]; then
    echo "--source and --signoff are required for ${mode}" >&2
    exit 2
  fi

  python3 - "${mode}" "${manifest}" "${source}" "${signoff}" <<'PY'
import json
import re
import sys

mode = sys.argv[1]
manifest_path = sys.argv[2]
source_path = sys.argv[3]
signoff_path = sys.argv[4]

with open(manifest_path, encoding="utf-8") as f:
    manifest = json.load(f)
with open(source_path, encoding="utf-8") as f:
    source = json.load(f)
with open(signoff_path, encoding="utf-8") as f:
    signoff = f.read()

fixtures = manifest.get("fixtures", [])
entries = source.get("entries", [])
snapshot_id = source.get("snapshot_id", "")

fixture_by_source = {f["workload_source_id"]: f for f in fixtures}
source_ids = [e["source_id"] for e in entries]
missing_source_ids = sorted([sid for sid in source_ids if sid not in fixture_by_source])
extra_manifest_ids = sorted([sid for sid in fixture_by_source if sid not in source_ids])

included_total = sum(1 for e in entries if e.get("status") == "included")
excluded_total = sum(1 for e in entries if e.get("status") == "excluded")
deferred_total = sum(1 for e in entries if e.get("status") == "deferred")
supported_fixtures = sum(1 for f in fixtures if f.get("expected_result_kind") == "success")
excluded_fixtures = sum(1 for f in fixtures if f.get("expected_result_kind") == "excluded")

result = {
    "snapshot_id": snapshot_id,
    "source_total": len(entries),
    "mapped_total": len(source_ids) - len(missing_source_ids),
    "included_total": included_total,
    "excluded_total": excluded_total,
    "deferred_total": deferred_total,
    "supported_fixtures": supported_fixtures,
    "excluded_fixtures": excluded_fixtures,
    "missing_source_ids": missing_source_ids,
    "extra_manifest_ids": extra_manifest_ids,
}

meta_match = re.search(r"```yaml\n(.*?)\n```", signoff, re.DOTALL)
if not meta_match:
    raise SystemExit("signoff missing yaml metadata block")
meta_text = meta_match.group(1)

meta = {}
for line in meta_text.splitlines():
    if ":" not in line:
        continue
    key, value = line.split(":", 1)
    meta[key.strip()] = value.strip()

if mode == "reconcile":
    print("reconcile report")
    for key in [
        "snapshot_id",
        "source_total",
        "mapped_total",
        "included_total",
        "excluded_total",
        "deferred_total",
        "supported_fixtures",
        "excluded_fixtures",
    ]:
        print(f"- {key}: {result[key]}")
    print(f"- missing_source_ids: {result['missing_source_ids']}")
    print(f"- extra_manifest_ids: {result['extra_manifest_ids']}")
    if missing_source_ids:
        raise SystemExit("reconcile failed: source entries missing from manifest")

if mode == "audit-signoff":
    if f"Snapshot ID: `{snapshot_id}`" not in signoff:
        raise SystemExit("signoff snapshot id does not match source snapshot")

    if "Reviewer: `" not in signoff or "Date: `" not in signoff:
        raise SystemExit("signoff missing reviewer/date fields")

    expected = {
        "snapshot_id": str(result["snapshot_id"]),
        "source_total": str(result["source_total"]),
        "mapped_total": str(result["mapped_total"]),
        "included_total": str(result["included_total"]),
        "excluded_total": str(result["excluded_total"]),
        "deferred_total": str(result["deferred_total"]),
        "supported_fixtures": str(result["supported_fixtures"]),
        "excluded_fixtures": str(result["excluded_fixtures"]),
        "missing_source_ids": str(result["missing_source_ids"]),
        "extra_manifest_ids": str(result["extra_manifest_ids"]),
    }

    for key, value in expected.items():
        actual = meta.get(key)
        if actual != value:
            raise SystemExit(f"signoff metadata mismatch for {key}: expected {value} got {actual}")

    if missing_source_ids:
        raise SystemExit("audit-signoff failed: unresolved missing source mappings")

    print("audit-signoff ok: signoff metadata and source mappings are in sync")
PY
}

case "${command}" in
  audit)
    audit_manifest
    ;;
  list)
    list_manifest
    ;;
  reconcile)
    reconcile_core "reconcile"
    ;;
  audit-signoff)
    reconcile_core "audit-signoff"
    ;;
  *)
    echo "unknown command: ${command}" >&2
    usage
    exit 2
    ;;
esac
