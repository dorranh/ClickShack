from clickshack.ir.models import IrEnvelope, StarNode
from clickshack.linter.base import LintRunner, LintViolation


class NoStarRule:
    """Prohibit SELECT *"""
    def check(self, node) -> list[LintViolation]:
        if isinstance(node, StarNode):
            return [LintViolation(
                rule="no-star",
                message="SELECT * is not allowed",
                span=node.span,
            )]
        return []


def test_lint_violation_found():
    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 14},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
        "from": {"type": "Table", "name": "t", "span": {"start": 14, "end": 15}},
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule()])
    violations = runner.run(query)
    assert len(violations) == 1
    assert violations[0].rule == "no-star"
    assert violations[0].span.start == 7


def test_lint_no_violations():
    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 20},
        "projections": [{"type": "Column", "name": "id", "span": {"start": 7, "end": 9}}],
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule()])
    violations = runner.run(query)
    assert violations == []


def test_lint_multiple_rules():
    class AlwaysViolates:
        def check(self, node) -> list[LintViolation]:
            if hasattr(node, "span"):
                return [LintViolation(rule="always", message="x", span=node.span)]
            return []

    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 8},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule(), AlwaysViolates()])
    violations = runner.run(query)
    # NoStarRule fires once (on Star node), AlwaysViolates fires on every node with span
    assert any(v.rule == "no-star" for v in violations)
    assert any(v.rule == "always" for v in violations)
