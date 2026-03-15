# Python Wheel with Bundled C++ Binary — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bundle the pre-built `ir_dump` C++ binary into the `clickshack` Python wheel so users can `pip install clickshack` and call `parse_sql()` to parse ClickHouse SQL.

**Architecture:** A Hatchling build hook copies `bin/ir_dump` (Bazel output, staged locally) into `clickshack/_bin/ir_dump` at wheel-build time via `force_include`. A Python wrapper in `clickshack/ir/parser.py` locates the binary with `importlib.resources.as_file`, calls it via subprocess, and returns a validated `IrEnvelope`.

**Tech Stack:** Python 3.11+, Hatchling (build hook), importlib.resources, subprocess, Pydantic v2 (`IrEnvelope.model_validate`), pytest.

**Spec:** `docs/superpowers/specs/2026-03-15-python-wheel-binary-distribution-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `.gitignore` | Modify | Add `bin/` and `clickshack/_bin/ir_dump` |
| `clickshack/_bin/.gitkeep` | Create | Keep `_bin/` dir in source control |
| `clickshack/_bin/__init__.py` | Create | Make `_bin` a package so `importlib.resources.files("clickshack._bin")` resolves |
| `clickshack/ir/parser.py` | Create | `parse_sql`, `ParseError`, `BinaryNotFoundError` |
| `clickshack/__init__.py` | Modify (currently empty) | Re-export `parse_sql`, `ParseError` |
| `tests/test_parser.py` | Create | Integration + unit tests for `parse_sql` |
| `hatch_build.py` | Create | Hatchling build hook — copies binary into wheel |
| `pyproject.toml` | Modify | Add `build` dev dep, `[tool.hatch.build.hooks.custom]` |
| `Justfile` | Modify | Add `stage-bin`, `build-wheel` targets |

---

## Chunk 1: Scaffolding + Parser Module

### Task 1: Update `.gitignore` and create `_bin` placeholder

**Files:**
- Modify: `.gitignore`
- Create: `clickshack/_bin/.gitkeep`
- Create: `clickshack/_bin/__init__.py`

- [ ] **Step 1: Add entries to `.gitignore`**

Append to `.gitignore` (after existing entries):
```
bin/
clickshack/_bin/ir_dump
```

- [ ] **Step 2: Create the `_bin` package**

```bash
touch clickshack/_bin/.gitkeep
touch clickshack/_bin/__init__.py
```

`__init__.py` makes `clickshack._bin` a proper Python package so that `importlib.resources.files("clickshack._bin")` resolves correctly in both development and installed environments.

- [ ] **Step 3: Commit**

```bash
git add .gitignore clickshack/_bin/.gitkeep clickshack/_bin/__init__.py
git commit -m "chore: add _bin package and gitignore entries for wheel binary staging"
```

---

### Task 2: Write failing tests for `parse_sql`

**Files:**
- Create: `tests/test_parser.py`

The binary (`ir_dump`) must already be staged at `clickshack/_bin/ir_dump` for the integration tests to run. The `BinaryNotFoundError` test uses `unittest.mock.patch` to simulate a missing binary without needing the real one.

- [ ] **Step 1: Write the tests**

Create `tests/test_parser.py`:
```python
from unittest.mock import patch, MagicMock
from pathlib import Path

import pytest

from clickshack import parse_sql, ParseError
from clickshack.ir.models import IrEnvelope
from clickshack.ir.parser import BinaryNotFoundError


# --- Integration tests (require staged binary at clickshack/_bin/ir_dump) ---

def test_parse_simple_select():
    result = parse_sql("SELECT 1")
    assert isinstance(result, IrEnvelope)
    assert result.ir_version == "1"


def test_parse_select_with_column():
    result = parse_sql("SELECT id FROM users")
    assert isinstance(result, IrEnvelope)


def test_parse_error_on_invalid_sql():
    with pytest.raises(ParseError):
        parse_sql("NOT VALID SQL !!!@#$")


def test_parse_error_on_empty_input():
    with pytest.raises(ParseError):
        parse_sql("")


def test_parse_returns_envelope_with_query():
    result = parse_sql("SELECT a, b FROM t WHERE x = 1")
    assert result.query is not None


# --- Unit test: BinaryNotFoundError when binary is absent ---

def test_binary_not_found_error(tmp_path):
    """parse_sql raises BinaryNotFoundError when the binary path does not exist."""
    missing = tmp_path / "ir_dump"  # does not exist

    mock_traversable = MagicMock()
    mock_traversable.joinpath.return_value = mock_traversable

    with patch("importlib.resources.files", return_value=mock_traversable):
        with patch("importlib.resources.as_file") as mock_as_file:
            mock_as_file.return_value.__enter__ = lambda s: missing
            mock_as_file.return_value.__exit__ = MagicMock(return_value=False)
            with pytest.raises(BinaryNotFoundError):
                parse_sql("SELECT 1")
```

- [ ] **Step 2: Run tests — expect ImportError (module doesn't exist yet)**

```bash
uv run pytest tests/test_parser.py -v
```

Expected: `ImportError` or `ModuleNotFoundError` — `parse_sql` and `ParseError` don't exist yet.

---

### Task 3: Implement `clickshack/ir/parser.py`

**Files:**
- Create: `clickshack/ir/parser.py`

- [ ] **Step 1: Write the implementation**

Create `clickshack/ir/parser.py`:
```python
"""ClickHouse SQL parser — thin subprocess wrapper around the ir_dump binary."""

from __future__ import annotations

import importlib.resources
import json
import subprocess

from clickshack.ir.models import IrEnvelope


class ParseError(ValueError):
    """Raised when ir_dump rejects the SQL input."""


class BinaryNotFoundError(RuntimeError):
    """Raised when the bundled ir_dump binary cannot be located.

    This should never occur in a properly installed wheel. It indicates
    a packaging misconfiguration during development.
    """


def parse_sql(sql: str) -> IrEnvelope:
    """Parse a ClickHouse SQL string and return a validated IR envelope.

    Args:
        sql: A ClickHouse SQL statement.

    Returns:
        IrEnvelope with ir_version and parsed query tree.

    Raises:
        ParseError: If ir_dump rejects the SQL (syntax error, empty input).
        BinaryNotFoundError: If the bundled ir_dump binary is missing.
        subprocess.TimeoutExpired: If the binary takes longer than 30 seconds.
    """
    try:
        ref = importlib.resources.files("clickshack._bin").joinpath("ir_dump")
        with importlib.resources.as_file(ref) as binary_path:
            if not binary_path.exists():
                raise BinaryNotFoundError(
                    f"ir_dump binary not found at {binary_path}. "
                    "Run `just stage-bin` and rebuild the wheel."
                )
            result = subprocess.run(
                [str(binary_path)],
                input=sql,
                capture_output=True,
                text=True,
                timeout=30,
            )
    except FileNotFoundError as exc:
        raise BinaryNotFoundError(
            "ir_dump binary could not be executed. "
            "Run `just stage-bin` and rebuild the wheel."
        ) from exc

    if result.returncode != 0:
        raise ParseError(result.stderr.strip())

    return IrEnvelope.model_validate(json.loads(result.stdout))
```

---

### Task 4: Update `clickshack/__init__.py` and run tests

**Files:**
- Modify (currently empty): `clickshack/__init__.py`

- [ ] **Step 1: Add the public API re-exports**

Write `clickshack/__init__.py` (file already exists but is empty):
```python
from clickshack.ir.parser import ParseError, parse_sql

__all__ = ["parse_sql", "ParseError"]
```

- [ ] **Step 2: Stage the binary so integration tests can run**

```bash
just build-ir   # builds bazel-bin/examples/bootstrap/ir_dump
mkdir -p bin
cp bazel-bin/examples/bootstrap/ir_dump bin/ir_dump
chmod +x bin/ir_dump
cp bin/ir_dump clickshack/_bin/ir_dump
chmod +x clickshack/_bin/ir_dump
```

Note: this manual copy is a one-time dev step. After Task 5, `just stage-bin` handles `bin/`; the `clickshack/_bin/` copy is the dev workaround until wheel-build time is the normal path.

- [ ] **Step 3: Run the parser tests**

```bash
uv run pytest tests/test_parser.py -v
```

Expected: all 6 tests PASS.

- [ ] **Step 4: Run the full test suite**

```bash
uv run pytest tests/ -v
```

Expected: all 21 tests PASS (15 existing + 6 new).

- [ ] **Step 5: Run checks**

```bash
just check
```

Expected: ruff and pyright both pass clean.

- [ ] **Step 6: Commit**

```bash
git add clickshack/ir/parser.py clickshack/__init__.py tests/test_parser.py
git commit -m "feat: add parse_sql wrapper and public API entry point"
```

---

## Chunk 2: Build Infrastructure

### Task 5: Add `stage-bin` and `build-wheel` to Justfile

**Files:**
- Modify: `Justfile`

- [ ] **Step 1: Add the new recipes after `build-ir`**

In `Justfile`, after the `build-ir` recipe, add:
```just
# Stage the ir_dump binary from Bazel output into bin/ for wheel packaging.
stage-bin:
    mkdir -p bin
    cp bazel-bin/examples/bootstrap/ir_dump bin/ir_dump
    chmod +x bin/ir_dump

# Build a platform wheel (runs build-ir + stage-bin first).
build-wheel:
    just build-ir
    just stage-bin
    MACOSX_DEPLOYMENT_TARGET=11.0 python -m build --wheel
```

- [ ] **Step 2: Verify `just stage-bin` works**

```bash
just build-ir
just stage-bin
ls -la bin/ir_dump
```

Expected: `bin/ir_dump` is present and executable.

- [ ] **Step 3: Commit**

```bash
git add Justfile
git commit -m "chore: add stage-bin and build-wheel Just targets"
```

---

### Task 6: Update `pyproject.toml` and create `hatch_build.py`

**Files:**
- Modify: `pyproject.toml`
- Create: `hatch_build.py`

- [ ] **Step 1: Add `build` to dev deps and activate the custom hook**

In `pyproject.toml`, update `[dependency-groups]`:
```toml
dev = ["pytest>=8.0", "pyright>=1.1", "ruff>=0.4", "build>=1.0"]
```

Add a new section after `[tool.hatch.build.targets.wheel]`:
```toml
[tool.hatch.build.hooks.custom]
```

- [ ] **Step 2: Create `hatch_build.py`**

Create `hatch_build.py` at the repo root:
```python
"""Hatchling build hook — copies the staged ir_dump binary into the wheel."""

from __future__ import annotations

import shutil
from pathlib import Path

from hatchling.builders.hooks.plugin.interface import BuildHookInterface

# Anchor all paths to the repo root so the hook works regardless of the
# working directory from which `python -m build` is invoked.
_ROOT = Path(__file__).parent


class ClickshackBuildHook(BuildHookInterface):
    PLUGIN_NAME = "custom"

    def initialize(self, version: str, build_data: dict) -> None:  # type: ignore[override]
        src = _ROOT / "bin" / "ir_dump"
        dst = _ROOT / "clickshack" / "_bin" / "ir_dump"
        dst_in_wheel = "clickshack/_bin/ir_dump"

        if not src.exists():
            raise FileNotFoundError(
                f"Staged binary not found at {src}. "
                "Run `just stage-bin` before building the wheel."
            )
        if not src.stat().st_mode & 0o111:
            raise PermissionError(f"{src} is not executable. Run `chmod +x {src}`.")

        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        dst.chmod(dst.stat().st_mode | 0o111)

        # force_include overrides Hatchling's VCS-based exclusions (.gitignore).
        # Without this, the gitignored binary would be silently dropped from the wheel.
        # Key = absolute source path on disk; value = destination path inside the wheel.
        build_data["force_include"][str(dst.resolve())] = dst_in_wheel
```

- [ ] **Step 3: Sync to install `build`**

```bash
uv sync --group dev
```

- [ ] **Step 4: Run checks**

```bash
just check
```

Expected: ruff and pyright pass clean.

- [ ] **Step 5: Smoke-test the hook by building a wheel**

```bash
just build-wheel
ls dist/
```

Expected: a `.whl` file appears in `dist/` with a platform tag (e.g. `clickshack-0.1.0-cp311-cp311-macosx_11_0_arm64.whl`).

- [ ] **Step 6: Verify the binary is inside the wheel**

```bash
python -c "
import zipfile
from pathlib import Path
# Use mtime to select the most recently built wheel (avoids stale wheel confusion).
whl = max(Path('dist').glob('*.whl'), key=lambda p: p.stat().st_mtime)
print('Inspecting:', whl.name)
with zipfile.ZipFile(whl) as z:
    bins = [n for n in z.namelist() if '_bin/ir_dump' in n]
    print('Found:', bins)
    assert bins, 'ir_dump not found in wheel!'
    print('OK')
"
```

Expected: prints the wheel filename, `Found: ['clickshack/_bin/ir_dump']`, and `OK`.

- [ ] **Step 7: Commit**

```bash
# clickshack/_bin/__init__.py and .gitkeep should already be committed from Task 1.
# Include them here if they were not committed earlier.
git add pyproject.toml hatch_build.py uv.lock clickshack/_bin/__init__.py clickshack/_bin/.gitkeep
git commit -m "feat: add Hatchling build hook to bundle ir_dump binary in wheel"
```

---

### Task 7: Final verification

- [ ] **Step 1: Run the full test suite**

```bash
uv run pytest tests/ -v
```

Expected: all 21 tests PASS.

- [ ] **Step 2: Run all checks**

```bash
just check
```

Expected: clean.

- [ ] **Step 3: Test wheel install in a fresh venv**

```bash
python -m venv /tmp/clickshack-test-venv
# Select the most recently built wheel to avoid stale-wheel ambiguity.
WHL=$(python -c "from pathlib import Path; print(max(Path('dist').glob('*.whl'), key=lambda p: p.stat().st_mtime))")
/tmp/clickshack-test-venv/bin/pip install "$WHL"
/tmp/clickshack-test-venv/bin/python -c "
from clickshack import parse_sql
result = parse_sql('SELECT 1')
print('ir_version:', result.ir_version)
print('query type:', result.query.type)
print('OK — wheel install works end-to-end')
"
```

Expected:
```
ir_version: 1
query type: Select
OK — wheel install works end-to-end
```

- [ ] **Step 4: Clean up test venv**

```bash
rm -rf /tmp/clickshack-test-venv
```
