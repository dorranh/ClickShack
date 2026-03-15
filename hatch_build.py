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
        if dst.exists():
            dst.unlink()
        shutil.copy2(src, dst)
        dst.chmod(dst.stat().st_mode | 0o111)

        # force_include overrides Hatchling's VCS-based exclusions (.gitignore).
        # Without this, the gitignored binary would be silently dropped from the wheel.
        # Key = absolute source path on disk; value = destination path inside the wheel.
        build_data["force_include"][str(dst.resolve())] = dst_in_wheel
        # Signal to Hatchling that this is a platform-specific distribution.
        build_data["pure_python"] = False
        build_data["infer_tag"] = True
