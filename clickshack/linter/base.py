from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol, runtime_checkable

from pydantic import BaseModel

from clickshack.ir.models import IrQuery, Span


@dataclass
class LintViolation:
    rule: str  # rule identifier, e.g. "no-star"
    message: str  # human-readable description
    span: Span  # source location from IR node


@runtime_checkable
class LintRule(Protocol):
    def check(self, node: object) -> list[LintViolation]: ...


class LintRunner:
    def __init__(self, rules: list[LintRule]) -> None:
        self.rules = rules

    def run(self, root: IrQuery) -> list[LintViolation]:
        violations: list[LintViolation] = []
        self._visit(root, violations)
        return violations

    def _visit(self, node: object, acc: list[LintViolation]) -> None:
        if node is None:
            return
        for rule in self.rules:
            acc.extend(rule.check(node))
        # Recurse into Pydantic model fields
        if isinstance(node, BaseModel):
            for value in node.__dict__.values():
                self._recurse(value, acc)

    def _recurse(self, value: object, acc: list[LintViolation]) -> None:
        if value is None:
            return
        if isinstance(value, BaseModel):
            self._visit(value, acc)
        elif isinstance(value, list):
            for item in value:
                self._recurse(item, acc)
        elif isinstance(value, dict):
            for v in value.values():
                self._recurse(v, acc)
