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
- Supported fixtures: `20`
- Excluded fixtures: `7`
- Missing source mappings: `none`
- Extra fixture mappings: `EX-001, EX-W2-001..EX-W2-007, IW-013..IW-018, null_literal_p1`

## Mapping Notes

- All `IW-001` through `IW-012` source identifiers map to exactly one manifest fixture.
- Supplemental exclusions and wave-2 coverage fixtures (`EX-001`, `EX-W2-001..EX-W2-007`, `IW-013..IW-018`) are intentionally manifest-only to keep explicit unsupported policies and phase-02.1 fidelity regressions under deterministic coverage.
- Deferred source item `IW-012` is intentionally represented as an excluded fixture to preserve explicit scope boundaries in Phase 02.
- Excluded workload classes (non-SELECT and MSSQL-specific forms) remain covered with deterministic failure fixtures.

## Sign-Off

- Reviewer: `GSD Executor`
- Date: `2026-03-10`
- Approval: `Approved`

## Audit Metadata

```yaml
snapshot_id: internal-priority-select-2026-03-07
source_total: 12
mapped_total: 12
included_total: 7
excluded_total: 4
deferred_total: 1
supported_fixtures: 20
excluded_fixtures: 7
missing_source_ids: []
extra_manifest_ids: ['EX-001', 'EX-W2-001', 'EX-W2-002', 'EX-W2-003', 'EX-W2-004', 'EX-W2-005', 'EX-W2-006', 'EX-W2-007', 'IW-013', 'IW-014', 'IW-015', 'IW-016', 'IW-017', 'IW-018', 'null_literal_p1']
```
