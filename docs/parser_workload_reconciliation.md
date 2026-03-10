# Parser Workload Reconciliation Sign-Off

## Source Snapshot

- Snapshot ID: `internal-priority-select-2026-03-07`
- Exported At: `2026-03-07`
- Source File: `testdata/parser_workload/prioritized_internal_workload.json`
- Manifest File: `testdata/parser_workload/manifest.json`

## Coverage Summary

- Source entries: `12`
- Manifest mappings: `12`
- Included entries: `7`
- Excluded entries: `4`
- Deferred entries: `1`
- Supported fixtures: `7`
- Excluded fixtures: `5`
- Missing source mappings: `none`
- Extra fixture mappings: `none`

## Mapping Notes

- All `IW-001` through `IW-012` source identifiers map to exactly one manifest fixture.
- Deferred source item `IW-012` is intentionally represented as an excluded fixture to preserve explicit scope boundaries in Phase 02.
- Excluded workload classes (non-SELECT and MSSQL-specific forms) remain covered with deterministic failure fixtures.

## Sign-Off

- Reviewer: `TBD`
- Date: `TBD`
- Approval: `Pending`

## Audit Metadata

```yaml
snapshot_id: internal-priority-select-2026-03-07
source_total: 12
mapped_total: 12
included_total: 7
excluded_total: 4
deferred_total: 1
supported_fixtures: 7
excluded_fixtures: 5
missing_source_ids: []
extra_manifest_ids: []
```
