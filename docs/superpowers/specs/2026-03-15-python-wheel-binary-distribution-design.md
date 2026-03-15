# Design: Python Wheel with Bundled C++ Binary

**Date:** 2026-03-15
**Status:** Approved

## Goal

Distribute `clickshack` as a platform-specific Python wheel that bundles the pre-built `ir_dump` C++ binary, so users can `pip install clickshack` and call a Python function to parse ClickHouse SQL — no Bazel or C++ toolchain required.

## Background

`ir_dump` is a Bazel-built C++ binary that reads SQL from stdin and writes a JSON IR envelope to stdout. The Python package (`clickshack`) already contains Pydantic IR models and a SQLGlot adapter that consume this JSON. Currently they are not distributed together.

## Architecture

```
Bazel builds ir_dump binary
        ↓
  bin/ir_dump  (local staging dir, gitignored)
        ↓
Hatchling build hook copies it into wheel
        ↓
  clickshack/_bin/ir_dump  (inside installed wheel)
        ↓
clickshack.ir.parser.parse_sql() locates binary via
importlib.resources, calls subprocess, returns IrEnvelope
```

## Components

### 1. Binary Staging (`bin/`)

A gitignored `bin/` directory at the repo root serves as the staging area between Bazel and the Python build. These are additions to the existing Justfile (which already defines the `bazel` variable and `build-ir` recipe):

```just
stage-bin:
    cp bazel-bin/examples/bootstrap/ir_dump bin/ir_dump
    chmod +x bin/ir_dump

build-wheel:
    just build-ir
    just stage-bin
    python -m build --wheel
```

`bazel-bin/` is the workspace symlink created by Bazel; the `--output_user_root=/tmp/bazel-root` flag used by the existing Justfile does not suppress this symlink. `bin/ir_dump` is added to `.gitignore`. The `build` package (pypa/build, the PEP 517 frontend) must be available in the dev environment — add it to `[dependency-groups].dev` in `pyproject.toml`.

### 2. Hatchling Build Hook (`hatch_build.py`)

A custom `BuildHookInterface` at the repo root, activated via `[tool.hatch.build.hooks.custom]` in `pyproject.toml`. During `python -m build --wheel` it:

1. Verifies `bin/ir_dump` exists and is executable — fails loudly with a clear message if not (must run `just stage-bin` first)
2. Copies it into `clickshack/_bin/ir_dump` and registers it via `build_data["force_include"]`

`force_include` is needed because `clickshack/_bin/ir_dump` is gitignored — Hatchling respects `.gitignore` exclusions by default and would omit the binary. `force_include` overrides VCS-based exclusions unconditionally.

`clickshack/_bin/.gitkeep` keeps the directory in source control. `clickshack/_bin/ir_dump` is gitignored.

### 3. Platform Tagging

The wheel platform tag is auto-detected from the build machine by Python's `build` tooling. No manual `--plat-name` override is needed for local builds. Platform-specific wheels are uploaded to PyPI separately; pip selects the right one at install time.

> **macOS note:** The auto-detected tag reflects the current OS version (e.g. `macosx_15_0_arm64`), which may be tighter than necessary. Set `MACOSX_DEPLOYMENT_TARGET` (e.g. `11.0`) before running `python -m build --wheel` to produce a more broadly compatible tag.

> **Future CI note:** Linux wheels require a `manylinux` tag for PyPI (e.g. `manylinux_2_17_x86_64`). Linux CI jobs should build inside a `manylinux` Docker container to ensure glibc compatibility and correct tagging. `cibuildwheel` handles this automatically.

**Target platforms:**
- `macosx_11_0_arm64` (or later)
- `linux_x86_64` / `manylinux_2_17_x86_64`
- `linux_aarch64` / `manylinux_2_17_aarch64`

### 4. Python Wrapper (`clickshack/ir/parser.py`)

Single public function:

```python
def parse_sql(sql: str) -> IrEnvelope:
    """Parse a ClickHouse SQL string and return a validated IR envelope."""
```

Implementation:

1. Locate the binary using `importlib.resources.as_file`, which is required to materialise the resource as a real filesystem path (necessary for executing a binary — it cannot be run from a zip-imported package):

```python
ref = importlib.resources.files("clickshack._bin").joinpath("ir_dump")
with importlib.resources.as_file(ref) as binary_path:
    result = subprocess.run([str(binary_path)], input=sql, ...)
```

2. Call `subprocess.run` with `sql` on stdin, capturing stdout and stderr. Use a timeout (default: 30 seconds); let `subprocess.TimeoutExpired` propagate uncaught in v1.
3. On non-zero exit, raise `ParseError(stderr)`.
4. Parse stdout JSON into `IrEnvelope` via `IrEnvelope.model_validate(json.loads(stdout))`. Note: the existing `parse_node` utility handles `IrExpr` discriminated unions, not the top-level envelope — use `model_validate` directly.

**Exceptions:**
- `ParseError(ValueError)` — `ir_dump` rejected the SQL (bad syntax, empty input)
- `BinaryNotFoundError(RuntimeError)` — bundled binary missing; internal/development error, not re-exported in the public API

### 5. Public API (`clickshack/__init__.py`)

Re-exports `parse_sql` and `ParseError` at the top level:

```python
from clickshack import parse_sql, ParseError
envelope = parse_sql("SELECT id FROM users")
```

`BinaryNotFoundError` is intentionally not re-exported — it signals a packaging misconfiguration, not a user-facing error condition.

IR model types (`IrEnvelope`, `IrExpr`, etc.) remain in `clickshack.ir.models` for users working with the parsed tree directly.

## Files Changed

| File | Change |
|------|--------|
| `hatch_build.py` | New — custom Hatchling build hook |
| `clickshack/_bin/.gitkeep` | New — keeps dir in source control |
| `clickshack/ir/parser.py` | New — `parse_sql`, `ParseError`, `BinaryNotFoundError` |
| `clickshack/__init__.py` | New — re-exports `parse_sql`, `ParseError` |
| `pyproject.toml` | Add `[tool.hatch.build.hooks.custom]`; add `build` to dev deps |
| `Justfile` | Add `stage-bin`, `build-wheel` targets |
| `.gitignore` | Add `bin/` and `clickshack/_bin/ir_dump` |

## Out of Scope

- CI pipeline for automated multi-platform wheel builds (future work)
- Wheel signing or attestation
- Source distribution (`sdist`) — platform wheels only
- `subprocess.TimeoutExpired` handling beyond default propagation
