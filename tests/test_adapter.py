import pytest
import sqlglot.expressions as exp

from clickshack.ir.models import IrEnvelope
from clickshack.ir.adapter import to_sqlglot, expr_to_sqlglot


def _ir(raw: dict):
    return IrEnvelope.model_validate(raw).query


def test_simple_select_star():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 14},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
        "from": {"type": "Table", "name": "t", "span": {"start": 14, "end": 15}},
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)
    assert len(result.expressions) == 1
    assert isinstance(result.expressions[0], exp.Star)


def test_select_literal():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 8},
        "projections": [{"type": "Literal", "kind": "number", "value": "42",
                         "span": {"start": 7, "end": 9}}]
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)


def test_select_column_with_table():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 20},
        "projections": [{"type": "Column", "name": "id", "table": "u",
                         "span": {"start": 7, "end": 11}}],
        "from": {"type": "Table", "name": "users", "alias": "u",
                 "span": {"start": 15, "end": 20}}
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)


def test_unsupported_raw_node_raises():
    from clickshack.ir.models import RawNode, Span
    node = RawNode(type="Raw", span=Span(start=0, end=0), id="unknown")
    with pytest.raises(NotImplementedError):
        expr_to_sqlglot(node)


def test_union():
    query = _ir({"ir_version": "1", "query": {
        "type": "SelectUnion", "span": {"start": 0, "end": 40},
        "op": "UNION", "quantifier": "ALL",
        "left": {"type": "Select", "span": {"start": 0, "end": 15},
                 "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
                 "from": {"type": "Table", "name": "a", "span": {"start": 14, "end": 15}}},
        "right": {"type": "Select", "span": {"start": 25, "end": 40},
                  "projections": [{"type": "Star", "span": {"start": 32, "end": 33}}],
                  "from": {"type": "Table", "name": "b", "span": {"start": 39, "end": 40}}}
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Union)
