# Stack Research

**Domain:** Parser-only C++ library extraction (ClickHouse SQL parser subset)
**Researched:** 2026-03-07
**Confidence:** HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended | Confidence |
|------------|---------|---------|-----------------|------------|
| C++ | C++20 (project minimum) | Implementation language for parser/AST/IR pipeline | Matches modern ClickHouse-style code patterns, strong compile-time safety, and keeps parser runtime fast without adding managed runtime overhead | HIGH |
| Bazel + Bzlmod | Bazel 8.x + `--enable_bzlmod` | Deterministic build graph, dependency isolation, repeatable parser-only targets | Already the project’s mandatory build system and best fit for strict dependency boundaries between parser layers | HIGH |
| Layered parser targets (`parser_core` -> `parser_lib`) | Current repository pattern | Enforce parser-only architecture and prevent accidental analyzer/runtime coupling | Existing architecture already maps this pattern and it scales safely as SQL coverage expands | HIGH |

### Supporting Libraries

| Library | Version (current repo) | Purpose | When to Use | Confidence |
|---------|-------------------------|---------|-------------|------------|
| `abseil-cpp` | `20250814.1` | Utility primitives (`string_view`, containers, status-like patterns where needed) | Keep for low-level parser utilities and compatibility with upstream-ish code patterns | HIGH |
| `fmt` | `11.2.0` | Formatting for diagnostics and debug output | Use at module boundaries (errors/log text), avoid deep parser hot-loop formatting | HIGH |
| `re2` | `2024-07-02.bcr.1` | Safe regex for bounded helper parsing tasks | Use only for non-core tokenization helpers; lexer/token iterator should remain explicit scanner logic | MEDIUM |
| `double-conversion` | `3.3.1` | Stable numeric text conversion | Use for literal parsing where exact float/string conversion behavior matters | HIGH |
| `magic_enum` | `0.9.7` | Enum reflection for parser diagnostics/debugging | Use for debug tooling and readable error reporting, not as core parsing control flow | MEDIUM |
| `boost` + `boost.config` | `1.89.0` | Transitional compatibility surface for ported code | Keep only while required by imported parser fragments; aggressively prune usage over time | HIGH |
| `openssl` | `3.5.5.bcr.0` | Transitive/compat dependency surface currently present | Retain only if forced by imported code paths; remove from parser-only graph when no longer needed | MEDIUM |

### Development Tools

| Tool | Purpose | Notes | Confidence |
|------|---------|-------|------------|
| `just` recipes | Standardized local build/test workflow | Keep a small command set (`build`, `build-parser-core`, smoke runs) to reduce operator variance | HIGH |
| Smoke binaries under `examples/bootstrap` | Fast parser behavior checks for key query shapes | Continue adding focused parser-only smoke coverage before broad test expansion | HIGH |
| `docs/ported_clickhouse_manifest.md` | Provenance + sync contract to upstream parser sources | Treat as mandatory update for every imported/pruned parser file change | HIGH |

## Installation / Setup Baseline

```bash
# Build parser-only aggregate
just build-parser-core
bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib

# Run parser smoke checks
just run-ported-smokes
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative | Confidence |
|-------------|-------------|-------------------------|------------|
| Bazel+Bzlmod | CMake + vcpkg/Conan | Only if organization-wide Bazel is unavailable; otherwise it weakens existing deterministic dependency control in this repo | HIGH |
| Ported parser subset in `ported_clickhouse/*` | Building against full ClickHouse tree | Use full tree only for temporary diffing/validation in `tmp/`, not for shipped parser library builds | HIGH |
| Minimal AST/IR contract generated in C++ | External Python/sqlglot transform in this milestone | Use sqlglot later for cross-dialect transformations, not for current parser reliability milestone | HIGH |

## What NOT to Use

| Avoid | Why | Use Instead | Confidence |
|-------|-----|-------------|------------|
| Linking analyzer/planner/execution subsystems | Violates parser-only scope and explodes dependency surface/maintenance cost | Keep strict parser-layer targets with explicit BUILD dependencies | HIGH |
| Depending on `tmp/` as build input | `tmp/` is non-canonical patch/reference workspace and must stay out of VCS/source graph | Curated, tracked sources only under `ported_clickhouse/*` | HIGH |
| Unbounded Boost/OpenSSL creep in parser targets | Increases compile/link cost and portability risk for little parser value | Isolate and prune to Abseil/std where possible | HIGH |
| Regex-driven core SQL parsing architecture | Harder to reason about correctness/parity vs recursive-descent/token parser approach | Keep explicit lexer/token iterator + parser combinator style | HIGH |
| Introducing runtime-heavy frameworks (LLVM/ANTLR runtime/protobuf/gRPC) into parser core | Adds substantial binary and build complexity for no immediate parser-only payoff | Preserve lightweight native parser core with minimal external deps | HIGH |

## Stack Patterns by Variant

**If goal is rapid SQL coverage growth:**
- Prefer adding new parser layers (e.g., clause/module stacks) that compose into `parser_lib`.
- Because this keeps feature growth incremental without destabilizing core lexer/token behavior.

**If goal is dependency reduction/hardening:**
- Prioritize replacing transitional Boost/OpenSSL touchpoints in parser paths first.
- Because this yields immediate build simplification while preserving parser semantics.

## Version Compatibility

| Package A | Compatible With | Notes | Confidence |
|-----------|-----------------|-------|------------|
| `rules_cc@0.2.14` | Bazel 8.x with Bzlmod enabled | Matches current module-mode build posture in repo | HIGH |
| `abseil-cpp@20250814.1` | `fmt@11.2.0`, `re2@2024-07-02.bcr.1` | Commonly coexisting C++ dependency set; verify if toolchain flags change | MEDIUM |
| `boost@1.89.0` + `boost.config@1.89.0` | Current parser ported subset | Keep versions aligned; treat as transitional footprint | HIGH |

## Recommendation Summary

- Keep Bazel+Bzlmod and C++20 as non-negotiable foundation.
- Continue parser-only layered architecture and smoke-first validation.
- Treat Boost/OpenSSL as reduction targets, not long-term parser-core requirements.
- Preserve strict source boundary: `ported_clickhouse/*` tracked, `tmp/*` reference-only.

## Sources

- `.planning/PROJECT.md` - scope, constraints, active requirements, and out-of-scope decisions
- `MODULE.bazel` - currently declared dependency graph and versions
- `.planning/codebase/ARCHITECTURE.md` - parser layering and dependency boundary intent
- `Justfile` + `.bazelrc` - practical build/run workflow and Bazel module mode configuration

---
*Stack research for: Clickshack Parser Bridge*
*Researched: 2026-03-07*
