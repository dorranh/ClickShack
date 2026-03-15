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
            "ir_dump binary could not be executed. Run `just stage-bin` and rebuild the wheel."
        ) from exc

    if result.returncode != 0:
        raise ParseError(result.stderr.strip())

    return IrEnvelope.model_validate(json.loads(result.stdout))
