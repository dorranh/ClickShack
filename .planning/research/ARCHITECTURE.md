# Architecture Research

**Domain:** Bazel-based C++ SQL parser bridge (ClickHouse parser extraction)
**Researched:** 2026-03-07
**Confidence:** HIGH

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    API / Consumer Boundary Layer                        │
├──────────────────────────────────────────────────────────────────────────┤
│  ┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐  │
│  │ Smoke Binaries    │   │ Parse CLI / App   │   │ Future Transpiler │  │
│  │ (examples/*)      │   │ (IR producer)     │   │ Adapter Consumer  │  │
│  └─────────┬─────────┘   └─────────┬─────────┘   └─────────┬─────────┘  │
├────────────┴───────────────────────┴───────────────────────┴────────────┤
│                    Parser Domain Layer (ported_clickhouse)              │
├──────────────────────────────────────────────────────────────────────────┤
│  ┌───────────────┐  ┌──────────────────┐  ┌──────────────────────────┐  │
│  │ Lexer/Token   │→ │ Parser Stacks    │→ │ AST Node Graph           │  │
│  │ Core          │  │ (USE/SELECT...)  │  │ (IAST + concrete nodes)  │  │
│  └───────┬───────┘  └────────┬─────────┘  └──────────────┬───────────┘  │
│          │                   │                            │              │
│          └───────────────────┴─────────────┬──────────────┘              │
│                                            ↓                              │
│                               IR Normalization Layer                      │
│                            (stable, versioned schema)                     │
├──────────────────────────────────────────────────────────────────────────┤
│                     Build / Provenance / Support Layer                   │
├──────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐ ┌─────────────────────┐ ┌────────────────────┐  │
│  │ Bazel Targets       │ │ Manifest + Sync     │ │ base/common/core   │  │
│  │ (BUILD.bazel)       │ │ (docs/*manifest*)   │ │ Minimal utilities  │  │
│  └─────────────────────┘ └─────────────────────┘ └────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| Parser Core | Tokenization, token iteration, parser entry orchestration | `Lexer.cpp`, `TokenIterator.cpp`, `IParser.cpp`, `:parser_core` |
| Grammar Stacks | Statement-level parsing (USE, SELECT-lite, SELECT-rich) | Layered parser targets in `ported_clickhouse/parsers/BUILD.bazel` |
| AST Model | Lossless parse structure for SQL statements and expressions | `IAST` base + concrete AST classes under `ported_clickhouse/parsers/*` |
| IR Normalizer | Convert AST to stable machine contract for downstream tooling | Dedicated AST visitor/walker + deterministic field ordering |
| Contract Boundary | Versioned schema + compatibility policy for consumers | `ir/schema/v1/*` and explicit semantic versioning of fields |
| Validation Surface | Prevent regressions in parse/AST/IR behavior | Smoke tests + parser fixtures + IR snapshot tests |
| Provenance/Sync | Track upstream origin and prune decisions | `docs/ported_clickhouse_manifest.md` and related notes |
| Build System | Reproducible parser-only build graph | Bazel modules + layered targets + hermetic test rules |

## Recommended Project Structure

```
ported_clickhouse/
├── parsers/                    # Parser core + grammar + AST definitions
│   ├── BUILD.bazel             # Layered parser target graph
│   ├── Lexer.cpp               # Lexing/token stream source
│   ├── IParser.cpp             # Parse control and contracts
│   ├── ParserSelectRichQuery.cpp
│   ├── AST*.h / AST*.cpp       # Parser-facing AST node types
│   └── ...
├── ir/                         # Stable parser output contract
│   ├── schema/
│   │   └── v1/                 # Versioned IR schema structures
│   ├── ast_to_ir/              # AST visitors / normalization logic
│   ├── serialization/          # JSON/proto/text emitters (deterministic)
│   └── BUILD.bazel
├── base/ common/ core/         # Minimal shared utility dependencies
└── BUILD.bazel                 # Public aggregate targets

examples/
└── bootstrap/                  # Smoke apps and parser/IR verification binaries

docs/
└── ported_clickhouse_manifest.md

.planning/
└── research/                   # Architecture, technical decisions, sequencing

tmp/                            # Upstream patch workspace (non-canonical, untracked)
```

### Structure Rationale

- **`ported_clickhouse/parsers/`:** Keeps parser behavior and AST ownership local, avoiding analyzer/runtime coupling.
- **`ported_clickhouse/ir/`:** Makes AST->IR conversion explicit and testable as a separate contract layer.
- **`examples/bootstrap/`:** Preserves executable behavior checks at parser and IR boundaries.
- **`docs/ported_clickhouse_manifest.md`:** Enforces upstream provenance and pruning transparency.
- **`tmp/`:** Maintains upstream reference workspace without polluting canonical source history.

## Architectural Patterns

### Pattern 1: Layered Grammar Expansion

**What:** Grow parsing capabilities through additive, target-scoped grammar layers (`USE -> SELECT lite -> SELECT rich`).
**When to use:** Any new SQL clause family or expression class.
**Trade-offs:** Strong containment and easier bisecting; more BUILD edges and integration glue.

### Pattern 2: Stable Output Boundary (AST Internal, IR External)

**What:** Treat AST as internal parse artifact and IR as public contract for apps/tooling.
**When to use:** Any consumer outside parser internals (CLI tools, future transpiler).
**Trade-offs:** Prevents consumer breakage from AST churn; requires conversion maintenance and version policy.

### Pattern 3: Provenance-Driven Porting

**What:** Every upstream-derived parser file maps to manifest metadata and pruning rationale.
**When to use:** New imports from ClickHouse parser sources.
**Trade-offs:** Slower import velocity; much better auditability and lower long-term drift risk.

## Data Flow

### Parse + IR Flow

```
[SQL text]
    ↓
[Lexer + TokenIterator]
    ↓
[Parser entrypoint (IParser + layered grammar)]
    ↓
[AST graph (IAST tree)]
    ↓
[AST validator / normalizer]
    ↓
[Versioned IR builder (v1)]
    ↓
[IR serializer]
    ↓
[Consumer: smoke/assertion tooling, future transpiler adapter]
```

### Build/Test Flow

```
[Bazel target selection]
    ↓
[parser_core]
    ↓
[grammar stack targets]
    ↓
[parser_lib aggregate]
    ↓
[smoke tests + IR snapshot tests]
    ↓
[release gate for parser/IR contract]
```

### Key Data Flows

1. **Parser stabilization flow:** SQL fixture -> parser layers -> AST assertions (structure + consumed tokens).
2. **Contract emission flow:** AST fixture -> AST->IR transform -> deterministic serialized IR snapshot.
3. **Transpiler handoff flow (future):** Stable IR v1 -> adapter mapping -> external transpiler runtime.

## Build Order (Recommended Execution Sequence)

1. **Foundation stabilization**
   - Repair Bazel breakage from upgrade, ensure `:parser_core` and existing smoke binaries build/test cleanly.
   - Freeze current behavior with fixtures for known internal queries.

2. **Parser parity hardening**
   - Incrementally close gaps in SELECT workload coverage.
   - Keep each grammar addition isolated to a stack target with targeted tests.

3. **AST contract tightening**
   - Define AST invariants required for downstream conversion (node presence, normalized aliases, clause ordering semantics).
   - Add AST-level regression assertions for critical query families.

4. **IR v1 introduction**
   - Create versioned IR schema and AST->IR normalization pipeline.
   - Add deterministic serialization and snapshot tests.

5. **Public parser+IR application target**
   - Provide a consumable binary/library outputting IR from SQL input.
   - Expose only stable IR structures across boundary.

6. **Transpiler handoff prep (future milestone)**
   - Add adapter-friendly metadata to IR (dialect, unsupported feature markers, source spans).
   - Keep transpiler integration out-of-process from parser core to avoid dependency contamination.

## Scaling Considerations

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Single internal team | Monorepo parser+IR with smoke/snapshot tests is sufficient |
| Multi-team internal consumers | Enforce strict IR versioning, deprecation policy, compatibility test matrix |
| Broad external adoption | Split parser core and IR SDK artifacts, publish schema docs and migration tooling |

### Scaling Priorities

1. **First bottleneck:** Parser regressions from grammar edits; mitigate with fixture-driven smoke + AST tests per layer.
2. **Second bottleneck:** Contract churn at AST boundary; mitigate by making IR the only external compatibility surface.

## Anti-Patterns

### Anti-Pattern 1: Exposing raw AST as public API

**What people do:** Allow downstream tools to bind directly to internal AST node classes.
**Why it's wrong:** AST evolves with parser internals and causes cascading consumer breakage.
**Do this instead:** Expose only versioned IR and keep AST internal.

### Anti-Pattern 2: Direct upstream dump into canonical source

**What people do:** Copy broad `src/` trees or keep `tmp/`-derived code as primary source.
**Why it's wrong:** Bloats dependency graph, obscures ownership, and destabilizes builds.
**Do this instead:** Curate parser-only imports into `ported_clickhouse/*` with manifest provenance.

### Anti-Pattern 3: Single giant parser target

**What people do:** Collapse all parser code into one monolithic Bazel target.
**Why it's wrong:** Slow iteration, poor isolation, and difficult failure diagnosis.
**Do this instead:** Maintain layered parser targets aligned with grammar growth.

## Integration Points

### External Services / Systems

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Bazel module ecosystem | `MODULE.bazel` deps pinned per target needs | Keep dependency surface minimal and parser-scoped |
| Future transpiler runtime (deferred) | Consume stable IR via adapter boundary | No direct parser internals linkage |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Lexer/Core -> Grammar Parsers | Direct C++ API/token stream contracts | Keep token semantics stable and well-tested |
| Grammar Parsers -> AST | AST node construction | AST is internal and can evolve with parser logic |
| AST -> IR | Visitor/transform pipeline | Versioned schema; deterministic output required |
| IR -> Consumer tools | Serialized payload / C++ structs | Backward compatibility guaranteed within major version |
| Canonical source -> upstream workspace (`tmp/`) | Manual curated sync via manifest | `tmp/` stays untracked and non-canonical |

## Sources

- `.planning/PROJECT.md`
- `.planning/codebase/ARCHITECTURE.md`
- `/Users/dorran/.codex/get-shit-done/templates/research-project/ARCHITECTURE.md`

---
*Architecture research for: Clickshack Parser Bridge*
*Researched: 2026-03-07*
