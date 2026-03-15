# CI Pipeline Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set up a GitHub Actions CI pipeline with lint/type-check, test, multi-platform binary build, and 9-wheel build jobs.

**Architecture:** Single `ci.yml` workflow, two independent branches: `check` (ruff + pyright) runs in parallel with `test → build-binary → build-wheel`. Binary builds use Bazel inside manylinux_2_28 Docker containers (Linux) or natively (macOS). `cibuildwheel` handles multi-Python (cp311/cp312/cp313) wheel packaging per platform.

**Tech Stack:** GitHub Actions, Bazel + `bazel-contrib/setup-bazel`, `uv`, `ruff`, `pyright`, `pytest`, `pypa/cibuildwheel`, Docker + QEMU (Linux aarch64), `quay.io/pypa/manylinux_2_28`.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `.bazelrc` | Modify | Add `build:ci` config block overriding PATH, CC, CXX |
| `tools/parser_smoke_suite.sh` | Modify | Add `--skip-build` flag: skip Bazel, check artifact presence only |
| `pyproject.toml` | Modify | Add `[tool.cibuildwheel]` section (Python versions, manylinux image, macOS deployment target) |
| `.github/workflows/ci.yml` | Create | Full 4-job CI pipeline |

---

## Chunk 1: Prerequisites

### Task 1: Add `build:ci` block to `.bazelrc`

**Files:**
- Modify: `.bazelrc`

The current `.bazelrc` has a hardcoded local PATH and unconditional `CC=cc_no_lld.sh` / `CXX=cc_no_lld.sh` that break on Linux CI runners. The `build:ci` config overrides these when workflows pass `--config=ci`.

- [ ] **Step 1: Read current `.bazelrc`**

  ```bash
  cat .bazelrc
  ```

- [ ] **Step 2: Append the CI config block**

  Add after the existing lines:

  ```ini
  # CI overrides: use system PATH and plain compiler names.
  # cc_no_lld.sh is a local macOS workaround and is not available on CI runners.
  build:ci --repo_env=PATH=/usr/local/bin:/usr/bin:/bin
  build:ci --repo_env=CC=cc
  build:ci --repo_env=CXX=c++
  ```

- [ ] **Step 3: Verify syntax is accepted**

  ```bash
  bazel --config=ci info output_base 2>&1 | head -5
  ```

  Expected: no "unrecognized config" error. (May fail on targets — that's fine here, just checking config parsing.)

- [ ] **Step 4: Commit**

  ```bash
  git add .bazelrc
  git commit -m "chore: add build:ci bazelrc config for GitHub Actions"
  ```

---

### Task 2: Add `--skip-build` flag to `tools/parser_smoke_suite.sh`

**Files:**
- Modify: `tools/parser_smoke_suite.sh`

The script currently only accepts `quick | full | suite` as its first argument and unconditionally re-invokes Bazel with hardcoded local settings. On Linux CI, this second Bazel invocation fails because `cc_no_lld.sh` is not in PATH. With `--skip-build`, the script skips the Bazel call and just verifies that expected artifacts exist in `bazel-bin/` — the `test` job's earlier `bazel --config=ci build` step already produced them.

- [ ] **Step 1: Read the current script**

  ```bash
  cat tools/parser_smoke_suite.sh
  ```

- [ ] **Step 2: Write `check_artifacts` function and `--skip-build` argument parsing**

  The full replacement for `tools/parser_smoke_suite.sh` (preserving all existing logic, adding only new code):

  ```bash
  #!/usr/bin/env bash
  set -euo pipefail

  bazel_cmd=(
    bazel
    --batch
    --output_user_root=/tmp/bazel-root
    build
    --repo_env=CC=cc_no_lld.sh
    --repo_env=CXX=cc_no_lld.sh
  )

  target_to_binary() {
    local label="$1"
    local pkg="${label#//}"
    pkg="${pkg%%:*}"
    local name="${label##*:}"
    echo "bazel-bin/${pkg}/${name}"
  }

  run_bazel_build() {
    local rc=0
    PATH="${PWD}/tools:${PATH}" "${bazel_cmd[@]}" "$@" || rc=$?
    if [[ ${rc} -eq 0 ]]; then
      return 0
    fi

    local missing=0
    for target in "$@"; do
      local binary
      binary="$(target_to_binary "${target}")"
      if [[ ! -x "${binary}" ]]; then
        echo "missing expected artifact after bazel failure: ${binary}" >&2
        missing=1
      fi
    done

    if [[ ${missing} -eq 0 ]]; then
      echo "warning: bazel exited ${rc}, but all requested smoke artifacts were produced" >&2
      return 0
    fi
    return "${rc}"
  }

  # Check that expected artifacts already exist in bazel-bin/ without rebuilding.
  # Used in CI where a prior bazel build step has already produced the artifacts.
  check_artifacts() {
    local missing=0
    for target in "$@"; do
      local binary
      binary="$(target_to_binary "${target}")"
      if [[ ! -x "${binary}" ]]; then
        echo "missing expected artifact: ${binary}" >&2
        missing=1
      fi
    done
    if [[ ${missing} -ne 0 ]]; then
      return 1
    fi
  }

  quick_targets=(
    //examples/bootstrap:use_parser_smoke
    //examples/bootstrap:select_lite_smoke
  )

  full_targets=(
    //examples/bootstrap:use_parser_smoke
    //examples/bootstrap:select_lite_smoke
    //examples/bootstrap:select_from_lite_smoke
    //examples/bootstrap:select_rich_smoke
  )

  # Parse --skip-build flag (must precede the mode argument).
  skip_build=0
  if [[ "${1:-}" == "--skip-build" ]]; then
    skip_build=1
    shift
  fi

  mode="${1:-suite}"

  if [[ ${skip_build} -eq 1 ]]; then
    case "${mode}" in
      quick)
        check_artifacts "${quick_targets[@]}"
        ;;
      full)
        check_artifacts "${full_targets[@]}"
        ;;
      suite)
        check_artifacts "${quick_targets[@]}"
        check_artifacts "${full_targets[@]}"
        ;;
      *)
        echo "usage: $0 [--skip-build] [quick|full|suite]" >&2
        exit 2
        ;;
    esac
  else
    case "${mode}" in
      quick)
        run_bazel_build "${quick_targets[@]}"
        ;;
      full)
        run_bazel_build "${full_targets[@]}"
        ;;
      suite)
        run_bazel_build "${quick_targets[@]}"
        run_bazel_build "${full_targets[@]}"
        ;;
      *)
        echo "usage: $0 [--skip-build] [quick|full|suite]" >&2
        exit 2
        ;;
    esac
  fi
  ```

- [ ] **Step 3: Verify `--skip-build` rejects an unknown mode**

  ```bash
  ./tools/parser_smoke_suite.sh --skip-build badmode 2>&1; echo "exit: $?"
  ```

  Expected output:
  ```
  usage: ./tools/parser_smoke_suite.sh [--skip-build] [quick|full|suite]
  exit: 2
  ```

- [ ] **Step 4: Build quick smoke targets so we can test the happy path**

  ```bash
  bazel build //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke
  ```

- [ ] **Step 5: Verify `--skip-build quick` passes when artifacts exist**

  ```bash
  ./tools/parser_smoke_suite.sh --skip-build quick
  echo "exit: $?"
  ```

  Expected: no output, `exit: 0`

- [ ] **Step 6: Verify `--skip-build quick` fails when artifacts are absent and reports all missing**

  ```bash
  mv bazel-bin/examples/bootstrap/use_parser_smoke /tmp/use_parser_smoke_bak
  mv bazel-bin/examples/bootstrap/select_lite_smoke /tmp/select_lite_smoke_bak
  ./tools/parser_smoke_suite.sh --skip-build quick 2>&1; echo "exit: $?"
  mv /tmp/use_parser_smoke_bak bazel-bin/examples/bootstrap/use_parser_smoke
  mv /tmp/select_lite_smoke_bak bazel-bin/examples/bootstrap/select_lite_smoke
  ```

  Expected: two "missing" lines and `exit: 1`:
  ```
  missing expected artifact: bazel-bin/examples/bootstrap/use_parser_smoke
  missing expected artifact: bazel-bin/examples/bootstrap/select_lite_smoke
  exit: 1
  ```

- [ ] **Step 7: Commit**

  ```bash
  git add tools/parser_smoke_suite.sh
  git commit -m "feat: add --skip-build flag to parser_smoke_suite.sh for CI use"
  ```

---

## Chunk 2: Python Config and CI Workflow

### Task 3: Add cibuildwheel config to `pyproject.toml`

**Files:**
- Modify: `pyproject.toml`

- [ ] **Step 1: Read `pyproject.toml`**

  ```bash
  cat pyproject.toml
  ```

- [ ] **Step 2: Append cibuildwheel config**

  Add at the end of `pyproject.toml`:

  ```toml
  [tool.cibuildwheel]
  build = ["cp311-*", "cp312-*", "cp313-*"]

  [tool.cibuildwheel.linux]
  manylinux-x86_64-image = "manylinux_2_28"
  manylinux-aarch64-image = "manylinux_2_28"

  [tool.cibuildwheel.macos]
  environment = {MACOSX_DEPLOYMENT_TARGET = "11.0"}
  ```

  The `MACOSX_DEPLOYMENT_TARGET = "11.0"` matches the value used in `just build-wheel`, ensuring consistent macOS platform tagging.

- [ ] **Step 3: Verify TOML parses cleanly**

  ```bash
  python3 -c "import tomllib; tomllib.load(open('pyproject.toml','rb')); print('ok')"
  ```

  Expected: `ok`

- [ ] **Step 4: Commit**

  ```bash
  git add pyproject.toml
  git commit -m "chore: add cibuildwheel config for cp311/cp312/cp313 multi-platform wheels"
  ```

---

### Task 4: Create `.github/workflows/ci.yml`

**Files:**
- Create: `.github/workflows/ci.yml`

This is the main deliverable. Four jobs: `check`, `test`, `build-binary`, `build-wheel`.

**Key implementation notes:**

- `setup-bazel` caches external dependency archives in `~/.cache/bazel`. For Linux Docker builds, this directory is volume-mounted to `/root/.cache/bazel` inside the container (manylinux runs as root — `$HOME` inside the container is `/root`, not `/home/runner`). Using `$HOME:$HOME` for the mount would silently miss.
- Bazel is not pre-installed in manylinux containers. Install Bazelisk inside the container via `curl` (~5MB, fast). The Bazel binary itself is cached via the `~/.cache/bazel` mount, so only the small Bazelisk CLI is re-fetched per run. No `~/.cache/bazelisk` mount is needed — the `curl` command downloads directly to `/usr/local/bin/bazel` and does not use Bazelisk's cache directory.
- `actions/download-artifact` requires `path: bin` to land the binary at `bin/ir_dump` where `hatch_build.py` expects it. Without `path:`, the action creates `ir_dump-<platform>/ir_dump` instead.
- `mkdir -p bin` must precede `download-artifact` — `bin/` is gitignored and absent after checkout.
- For `build-wheel`, `cibuildwheel` mounts the full project (including `bin/`) into the Linux container automatically. The hatch hook running inside the container finds `bin/ir_dump` at the project root.

- [ ] **Step 1: Create `.github/workflows/` directory**

  ```bash
  mkdir -p .github/workflows
  ```

- [ ] **Step 2: Write `ci.yml`**

  ```yaml
  name: CI

  on:
    push:
      branches: [main]
    pull_request:

  jobs:
    check:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v4
        - uses: astral-sh/setup-uv@v5
        - run: uv sync --group dev
        - run: uv run ruff check clickshack/ tests/
        - run: uv run ruff format --check clickshack/ tests/
        - run: uv run pyright clickshack/

    test:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v4
        - uses: bazel-contrib/setup-bazel@0.9.0
          with:
            bazelisk-cache: true
            disk-cache: ${{ github.workflow }}-${{ runner.os }}
            repository-cache: true
        - name: Build ir_dump and smoke targets
          run: |
            bazel --config=ci build \
              //examples/bootstrap:ir_dump \
              //examples/bootstrap:use_parser_smoke \
              //examples/bootstrap:select_lite_smoke
        - name: Stage ir_dump for pytest
          run: |
            cp bazel-bin/examples/bootstrap/ir_dump clickshack/_bin/ir_dump
            chmod +x clickshack/_bin/ir_dump
        - uses: astral-sh/setup-uv@v5
          with:
            python-version: "3.11"
        - run: uv sync --group dev
        - run: uv run pytest tests/ -v
        - run: ./tools/parser_smoke_suite.sh --skip-build quick

    build-binary:
      needs: test
      strategy:
        matrix:
          include:
            - platform: linux-x86_64
              runner: ubuntu-latest
              docker-image: quay.io/pypa/manylinux_2_28_x86_64
              bazelisk-arch: amd64
            - platform: linux-aarch64
              runner: ubuntu-latest
              docker-image: quay.io/pypa/manylinux_2_28_aarch64
              bazelisk-arch: arm64
            - platform: macos-arm64
              runner: macos-14
      runs-on: ${{ matrix.runner }}
      steps:
        - uses: actions/checkout@v4
        - name: Set up QEMU
          if: matrix.platform == 'linux-aarch64'
          uses: docker/setup-qemu-action@v3
        - uses: bazel-contrib/setup-bazel@0.9.0
          with:
            bazelisk-cache: true
            disk-cache: ${{ github.workflow }}-${{ matrix.platform }}
            repository-cache: true
        - name: Build ir_dump (Linux, manylinux_2_28)
          if: runner.os == 'Linux'
          run: |
            docker run --rm \
              -v "$GITHUB_WORKSPACE:/work" \
              -v "$HOME/.cache/bazel:/root/.cache/bazel" \
              -w /work \
              "${{ matrix.docker-image }}" \
              /bin/bash -c "
                curl -fsSL -o /usr/local/bin/bazel \
                  https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-${{ matrix.bazelisk-arch }} &&
                chmod +x /usr/local/bin/bazel &&
                bazel --config=ci build //examples/bootstrap:ir_dump
              "
        - name: Build ir_dump (macOS)
          if: runner.os == 'macOS'
          run: bazel --config=ci build //examples/bootstrap:ir_dump
        - uses: actions/upload-artifact@v4
          with:
            name: ir_dump-${{ matrix.platform }}
            path: bazel-bin/examples/bootstrap/ir_dump

    build-wheel:
      needs: build-binary
      strategy:
        matrix:
          include:
            - platform: linux-x86_64
              runner: ubuntu-latest
            - platform: linux-aarch64
              runner: ubuntu-latest
            - platform: macos-arm64
              runner: macos-14
      runs-on: ${{ matrix.runner }}
      steps:
        - uses: actions/checkout@v4
        - name: Set up QEMU
          if: matrix.platform == 'linux-aarch64'
          uses: docker/setup-qemu-action@v3
        - name: Create binary staging directory
          run: mkdir -p bin
        - uses: actions/download-artifact@v4
          with:
            name: ir_dump-${{ matrix.platform }}
            path: bin
        - run: chmod +x bin/ir_dump
        - uses: pypa/cibuildwheel@v2.22.0
          env:
            CIBW_ARCHS_LINUX: ${{ matrix.platform == 'linux-aarch64' && 'aarch64' || 'x86_64' }}
            CIBW_ARCHS_MACOS: arm64
        - uses: actions/upload-artifact@v4
          with:
            name: wheels-${{ matrix.platform }}
            path: wheelhouse/*.whl
  ```

- [ ] **Step 3: Verify YAML parses**

  ```bash
  python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml')); print('ok')"
  ```

  Expected: `ok`. (Install PyYAML if needed: `pip install pyyaml`)

- [ ] **Step 4: Commit**

  ```bash
  git add .github/workflows/ci.yml
  git commit -m "feat: add GitHub Actions CI pipeline"
  ```

---

## Verification

After all tasks are complete, verify the full set of changes:

- [ ] **Confirm all four files changed**

  ```bash
  git log --oneline -4
  ```

  Expected: four commits, one per task.

- [ ] **Confirm CI is syntactically valid at the GitHub level**

  Push to a branch and open a PR, or use the GitHub Actions `workflow_dispatch` to trigger manually.  The `check` job and `test` job should complete within a few minutes. `build-binary` and `build-wheel` follow.
