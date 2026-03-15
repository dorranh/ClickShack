from unittest.mock import MagicMock, patch

import pytest

from clickshack import ParseError, parse_sql
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
