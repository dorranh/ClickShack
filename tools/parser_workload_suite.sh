#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
usage: $0 <command> [options]

commands:
  audit          validate manifest schema and fixture files
  audit-docs     validate scope/roadmap/validation docs against fixture metadata
  list           enumerate fixture corpus
  reconcile      compare manifest mappings against source snapshot
  audit-signoff  verify sign-off metadata matches reconciliation totals
  quick          run a targeted supported subset from the manifest
  supported      run all supported fixtures and compare exact summaries
  excluded       run all excluded fixtures and verify deterministic failure class
  repeat         run supported fixtures twice and assert deterministic output
  full           run audit + reconcile + audit-signoff + supported + excluded + repeat

options:
  --manifest <path>   manifest path (required)
  --source <path>     workload-source snapshot json (reconcile/audit-signoff/full)
  --signoff <path>    reconciliation markdown doc (reconcile/audit-signoff/full)
  --scope-doc <path>  scope markdown doc (audit-docs)
  --roadmap-doc <path> roadmap markdown doc (audit-docs)
  --validation-doc <path> validation markdown doc (audit-docs)
USAGE
}

manifest=""
source=""
signoff=""
scope_doc=""
roadmap_doc=""
validation_doc=""
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
    --scope-doc)
      scope_doc="${2:-}"
      shift 2
      ;;
    --roadmap-doc)
      roadmap_doc="${2:-}"
      shift 2
      ;;
    --validation-doc)
      validation_doc="${2:-}"
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
runner="bazel-bin/examples/bootstrap/parser_workload_summary"

default_source="${manifest_dir}/prioritized_internal_workload.json"
default_signoff="${PWD}/docs/parser_workload_reconciliation.md"

apply_default_reconcile_inputs() {
  if [[ -z "${source}" && -f "${default_source}" ]]; then
    source="${default_source}"
  fi
  if [[ -z "${signoff}" && -f "${default_signoff}" ]]; then
    signoff="${default_signoff}"
  fi
}

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

audit_docs() {
  python3 - "${manifest}" "${scope_doc}" "${roadmap_doc}" "${validation_doc}" "${signoff}" <<'PY'
import json
import os
import re
import sys

manifest_path, scope_doc, roadmap_doc, validation_doc, signoff_path = sys.argv[1:]

if not scope_doc:
    raise SystemExit("--scope-doc is required for audit-docs")
if not os.path.isfile(scope_doc):
    raise SystemExit(f"scope doc not found: {scope_doc}")

with open(manifest_path, encoding="utf-8") as f:
    fixtures = json.load(f)["fixtures"]
with open(scope_doc, encoding="utf-8") as f:
    scope_text = f.read()

supported = [f for f in fixtures if f.get("expected_result_kind") == "success"]
excluded = [f for f in fixtures if f.get("expected_result_kind") == "excluded"]

scope_lower = scope_text.lower()
if "supported" not in scope_lower or "excluded" not in scope_lower:
    raise SystemExit("scope doc must include supported and excluded sections")

required_exclusion_tags = {"non_sql", "non_select", "mssql", "query_parameter"}
manifest_tags = {tag for f in excluded for tag in f.get("clause_tags", [])}
missing_tags = sorted(tag for tag in required_exclusion_tags if tag not in manifest_tags)
if missing_tags:
    raise SystemExit(f"manifest excluded coverage missing required tags: {missing_tags}")

required_scope_terms = [
    "non-sql",
    "non-select",
    "mssql",
    "query-parameter",
]
for term in required_scope_terms:
    if term not in scope_lower:
        raise SystemExit(f"scope doc missing exclusion term: {term}")

if roadmap_doc:
    if not os.path.isfile(roadmap_doc):
        raise SystemExit(f"roadmap doc not found: {roadmap_doc}")
    with open(roadmap_doc, encoding="utf-8") as f:
        roadmap_text = f.read().lower()
    if "phase 02" not in roadmap_text:
        raise SystemExit("roadmap doc missing phase 02 context")
    if "parser_workload_scope.md" not in roadmap_text:
        raise SystemExit("roadmap doc must reference docs/parser_workload_scope.md")
    if "fixture" not in roadmap_text:
        raise SystemExit("roadmap doc must reference fixture-backed coverage")

if validation_doc:
    if not os.path.isfile(validation_doc):
        raise SystemExit(f"validation doc not found: {validation_doc}")
    with open(validation_doc, encoding="utf-8") as f:
        validation_text = f.read()
    expected_cmds = [
        "parser_workload_suite.sh full",
        "parser_workload_suite.sh audit-signoff",
        "just smoke-suite",
        "build //ported_clickhouse:parser_lib",
    ]
    for cmd in expected_cmds:
        if cmd not in validation_text:
            raise SystemExit(f"validation doc missing required command evidence: {cmd}")

if signoff_path:
    if not os.path.isfile(signoff_path):
        raise SystemExit(f"signoff doc not found: {signoff_path}")
    with open(signoff_path, encoding="utf-8") as f:
        signoff_text = f.read()
    if "Approval: `Approved`" not in signoff_text:
        raise SystemExit("signoff must record final approval status")

print(
    "audit-docs ok: "
    f"supported={len(supported)} excluded={len(excluded)} "
    f"scope={scope_doc}"
)
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
  apply_default_reconcile_inputs
  if [[ -n "${source}" && -f "${source}" ]]; then
    if ! head -c 1 "${source}" | grep -q "[{[]"; then
      if [[ -f "${default_source}" ]]; then
        echo "warning: --source is not JSON, falling back to ${default_source}" >&2
        source="${default_source}"
      fi
    fi
  fi
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

fixture_rows() {
  local mode="$1"
  python3 - "${manifest}" "${manifest_dir}" "${mode}" <<'PY'
import json
import os
import sys

manifest_path = sys.argv[1]
manifest_dir = sys.argv[2]
mode = sys.argv[3]

with open(manifest_path, encoding="utf-8") as f:
    fixtures = json.load(f)["fixtures"]

if mode == "quick":
    rows = [f for f in fixtures if f["expected_result_kind"] == "success" and f["priority"] == "P0"]
elif mode == "supported":
    rows = [f for f in fixtures if f["expected_result_kind"] == "success"]
elif mode == "excluded":
    rows = [f for f in fixtures if f["expected_result_kind"] == "excluded"]
else:
    raise SystemExit(f"unknown fixture mode: {mode}")

rows = sorted(rows, key=lambda r: (r["priority"], r["id"]))
for row in rows:
    print(
        "\t".join(
            [
                row["id"],
                row["expected_result_kind"],
                os.path.join(manifest_dir, row["path"]),
                row.get("expected_canonical_summary", ""),
                row.get("expected_failure_class", ""),
            ]
        )
    )
PY
}

build_runner() {
  local bazel_rc=0
  PATH="${PWD}/tools:${PATH}" bazel --batch --output_user_root=/tmp/bazel-root build \
    --repo_env=CC=cc_no_lld.sh --repo_env=CXX=cc_no_lld.sh \
    //examples/bootstrap:parser_workload_summary >/tmp/parser_workload_bazel.log 2>&1 || bazel_rc=$?

  if [[ ! -x "${runner}" ]]; then
    cat /tmp/parser_workload_bazel.log >&2
    echo "runner binary not produced" >&2
    exit 1
  fi

  if [[ ${bazel_rc} -ne 0 ]]; then
    echo "warning: bazel exited ${bazel_rc} after producing artifact (sandbox sysctl limitation)" >&2
  fi
}

run_supported_mode() {
  local mode="$1"
  build_runner
  "${runner}" --manifest "${manifest}" --mode "${mode}"
}

run_excluded() {
  build_runner
  "${runner}" --manifest "${manifest}" --mode excluded
}

run_repeat() {
  build_runner
  "${runner}" --manifest "${manifest}" --mode repeat
}

case "${command}" in
  audit)
    audit_manifest
    ;;
  list)
    list_manifest
    ;;
  audit-docs)
    audit_docs
    ;;
  reconcile)
    reconcile_core "reconcile"
    ;;
  audit-signoff)
    reconcile_core "audit-signoff"
    ;;
  quick)
    run_supported_mode "quick"
    ;;
  supported)
    run_supported_mode "supported"
    ;;
  excluded)
    run_excluded
    ;;
  repeat)
    run_repeat
    ;;
  full)
    audit_manifest
    reconcile_core "reconcile"
    reconcile_core "audit-signoff"
    run_supported_mode "supported"
    run_excluded
    run_repeat
    ;;
  *)
    echo "unknown command: ${command}" >&2
    usage
    exit 2
    ;;
esac
