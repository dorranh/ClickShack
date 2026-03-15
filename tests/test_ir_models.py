from clickshack.ir.models import AliasNode, IrEnvelope, OverNode, SelectNode, parse_node


def test_envelope_parses_simple_select():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "Select",
            "span": {"start": 0, "end": 10},
            "projections": [
                {"type": "Literal", "kind": "number", "value": "1", "span": {"start": 0, "end": 1}}
            ],
        },
    }
    envelope = IrEnvelope.model_validate(raw)
    assert envelope.ir_version == "1"
    assert envelope.query.type == "Select"
    assert isinstance(envelope.query, SelectNode)


def test_literal_null():
    node = {"type": "Literal", "kind": "null", "span": {"start": 0, "end": 4}}
    lit = parse_node(node)
    assert lit.kind == "null"
    assert lit.value is None


def test_select_with_where():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "Select",
            "span": {"start": 0, "end": 50},
            "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
            "from": {"type": "Table", "name": "users", "span": {"start": 14, "end": 19}},
            "where": {
                "type": "BinaryOp",
                "op": "=",
                "left": {"type": "Column", "name": "id", "span": {"start": 26, "end": 28}},
                "right": {
                    "type": "Literal",
                    "kind": "number",
                    "value": "1",
                    "span": {"start": 31, "end": 32},
                },
                "span": {"start": 26, "end": 32},
            },
        },
    }
    env = IrEnvelope.model_validate(raw)
    assert env.query.type == "Select"
    assert env.query.where is not None
    assert env.query.where.type == "BinaryOp"


def test_with_node():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "With",
            "span": {"start": 0, "end": 80},
            "recursive": False,
            "ctes": [
                {
                    "name": "cte1",
                    "query": {
                        "type": "Select",
                        "span": {"start": 14, "end": 30},
                        "projections": [{"type": "Star", "span": {"start": 21, "end": 22}}],
                    },
                }
            ],
            "body": {
                "type": "Select",
                "span": {"start": 34, "end": 60},
                "projections": [{"type": "Star", "span": {"start": 41, "end": 42}}],
                "from": {"type": "Table", "name": "cte1", "span": {"start": 48, "end": 52}},
            },
        },
    }
    env = IrEnvelope.model_validate(raw)
    assert env.query.type == "With"
    assert len(env.query.ctes) == 1
    assert env.query.ctes[0].name == "cte1"


def test_alias_node():
    node = {
        "type": "Alias",
        "span": {"start": 0, "end": 10},
        "expr": {"type": "Column", "name": "x", "span": {"start": 0, "end": 1}},
        "alias": "my_col",
    }
    parsed = parse_node(node)
    assert isinstance(parsed, AliasNode)
    assert parsed.alias == "my_col"
    assert parsed.expr.name == "x"


def test_over_node():
    node = {
        "type": "Over",
        "span": {"start": 0, "end": 20},
        "function": {
            "type": "Function",
            "name": "sum",
            "args": [{"type": "Column", "name": "v", "span": {"start": 0, "end": 1}}],
            "span": {"start": 0, "end": 5},
        },
        "window": {"type": "Raw", "span": {"start": 10, "end": 20}, "id": "win"},
    }
    parsed = parse_node(node)
    assert isinstance(parsed, OverNode)
    assert parsed.function.name == "sum"


def test_cte_def_bare_expr():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "With",
            "span": {"start": 0, "end": 50},
            "recursive": False,
            "ctes": [
                {
                    "expr": {
                        "type": "Literal",
                        "kind": "number",
                        "value": "1",
                        "span": {"start": 0, "end": 1},
                    }
                },
            ],
            "body": {
                "type": "Select",
                "span": {"start": 10, "end": 50},
                "projections": [{"type": "Star", "span": {"start": 17, "end": 18}}],
            },
        },
    }
    env = IrEnvelope.model_validate(raw)
    assert env.query.type == "With"
    assert len(env.query.ctes) == 1
    assert env.query.ctes[0].name is None
    assert env.query.ctes[0].expr is not None
