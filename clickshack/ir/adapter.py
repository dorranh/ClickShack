"""SQLGlot adapter: converts IR node tree → sqlglot AST."""
from __future__ import annotations

from typing import Union

import sqlglot.expressions as exp

from clickshack.ir.models import (
    ArrayNode, BetweenNode, BinaryOpNode, CaseNode, CastNode,
    ColumnNode, FunctionNode, InNode, IrExpr, IrQuery, IrTableExpr,
    IsNullNode, JoinNode, LambdaNode, LikeNode, LiteralNode,
    RawNode, SelectNode, SelectUnionNode, StarNode, SubqueryNode,
    SubquerySourceNode, TableFunctionNode, TableNode, TableStarNode,
    TupleNode, UnaryOpNode, WithNode,
)


def to_sqlglot(query: IrQuery) -> exp.Expression:
    """Convert an IR root node to a sqlglot Expression."""
    if isinstance(query, WithNode):
        return _with_to_sqlglot(query)
    if isinstance(query, SelectUnionNode):
        return _union_to_sqlglot(query)
    if isinstance(query, SelectNode):
        return _select_to_sqlglot(query)
    raise NotImplementedError(f"to_sqlglot: unsupported root type {type(query).__name__}")


def expr_to_sqlglot(node: IrExpr) -> exp.Expression:
    """Convert an IR expression node to a sqlglot Expression."""
    if isinstance(node, LiteralNode):
        if node.kind == "null":
            return exp.Null()
        if node.kind == "number":
            return exp.Literal.number(node.value or "0")
        return exp.Literal.string(node.value or "")

    if isinstance(node, ColumnNode):
        col = exp.Column(this=exp.Identifier(this=node.name))
        if node.table:
            col = exp.Column(
                this=exp.Identifier(this=node.name),
                table=exp.Identifier(this=node.table),
            )
        return col

    if isinstance(node, StarNode):
        return exp.Star()

    if isinstance(node, TableStarNode):
        return exp.Column(this=exp.Star(), table=exp.Identifier(this=node.table))

    if isinstance(node, FunctionNode):
        return exp.Anonymous(
            this=node.name,
            expressions=[expr_to_sqlglot(a) for a in node.args],
        )

    if isinstance(node, BinaryOpNode):
        left = expr_to_sqlglot(node.left)
        right = expr_to_sqlglot(node.right)
        _op_map = {
            "=": exp.EQ, "!=": exp.NEQ, "<>": exp.NEQ,
            "<": exp.LT, "<=": exp.LTE, ">": exp.GT, ">=": exp.GTE,
            "+": exp.Add, "-": exp.Sub, "*": exp.Mul, "/": exp.Div,
            "AND": exp.And, "OR": exp.Or,
            "||": exp.DPipe,
        }
        cls = _op_map.get(node.op.upper())
        if cls:
            return cls(this=left, expression=right)
        return exp.Anonymous(this=f"_op_{node.op}", expressions=[left, right])

    if isinstance(node, UnaryOpNode):
        inner = expr_to_sqlglot(node.expr)
        if node.op.upper() == "NOT":
            return exp.Not(this=inner)
        if node.op == "-":
            return exp.Neg(this=inner)
        return exp.Anonymous(this=f"_unary_{node.op}", expressions=[inner])

    if isinstance(node, IsNullNode):
        inner = expr_to_sqlglot(node.expr)
        is_null = exp.Is(this=inner, expression=exp.Null())
        return exp.Not(this=is_null) if node.negated else is_null

    if isinstance(node, InNode):
        inner = expr_to_sqlglot(node.expr)
        if node.list:
            result = exp.In(this=inner, expressions=[expr_to_sqlglot(x) for x in node.list])
        elif node.subquery:
            result = exp.In(this=inner, query=to_sqlglot(node.subquery.query))
        else:
            result = exp.In(this=inner, expressions=[])
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, BetweenNode):
        result = exp.Between(
            this=expr_to_sqlglot(node.expr),
            low=expr_to_sqlglot(node.low),
            high=expr_to_sqlglot(node.high),
        )
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, LikeNode):
        this = expr_to_sqlglot(node.expr)
        pattern = expr_to_sqlglot(node.pattern)
        cls = exp.ILike if node.ilike else exp.Like
        result = cls(this=this, expression=pattern)
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, SubqueryNode):
        return exp.Subquery(this=to_sqlglot(node.query))

    if isinstance(node, ArrayNode):
        return exp.Array(expressions=[expr_to_sqlglot(e) for e in node.elements])

    if isinstance(node, TupleNode):
        return exp.Tuple(expressions=[expr_to_sqlglot(e) for e in node.elements])

    if isinstance(node, CastNode):
        return exp.Cast(
            this=expr_to_sqlglot(node.expr),
            to=exp.DataType.build(node.cast_type),
        )

    if isinstance(node, LambdaNode):
        return exp.Lambda(
            this=expr_to_sqlglot(node.body),
            expressions=[expr_to_sqlglot(p) for p in node.params],
        )

    if isinstance(node, CaseNode):
        ifs = [
            exp.If(this=expr_to_sqlglot(b.when), true=expr_to_sqlglot(b.then))
            for b in node.branches
        ]
        default = expr_to_sqlglot(node.else_expr) if node.else_expr else None
        return exp.Case(
            this=expr_to_sqlglot(node.operand) if node.operand else None,
            ifs=ifs,
            default=default,
        )

    if isinstance(node, RawNode):
        raise NotImplementedError(
            f"expr_to_sqlglot: Raw node not supported (id={node.id!r})"
        )

    raise NotImplementedError(
        f"expr_to_sqlglot: unsupported node type {type(node).__name__}"
    )


# ── Private helpers ───────────────────────────────────────────────────────────

def _select_to_sqlglot(sel: SelectNode) -> exp.Select:
    projections = [expr_to_sqlglot(p) for p in sel.projections]
    result = exp.select(*projections)
    if sel.from_ is not None:
        result = result.from_(_table_to_sqlglot(sel.from_))
    if sel.where is not None:
        result = result.where(expr_to_sqlglot(sel.where))
    if sel.distinct:
        result = result.distinct()
    return result


def _table_to_sqlglot(te: IrTableExpr) -> exp.Expression:
    if isinstance(te, TableNode):
        if te.database:
            tbl = exp.Table(
                db=exp.Identifier(this=te.database),
                this=exp.Identifier(this=te.name),
            )
        else:
            tbl = exp.Table(this=exp.Identifier(this=te.name))
        if te.alias:
            return exp.Alias(this=tbl, alias=exp.Identifier(this=te.alias))
        return tbl

    if isinstance(te, SubquerySourceNode):
        sq = exp.Subquery(this=to_sqlglot(te.query))
        if te.alias:
            return exp.Alias(this=sq, alias=exp.Identifier(this=te.alias))
        return sq

    if isinstance(te, TableFunctionNode):
        return exp.Anonymous(
            this=te.function.name,
            expressions=[expr_to_sqlglot(a) for a in te.function.args],
        )

    if isinstance(te, JoinNode):
        raise NotImplementedError(
            "JoinNode: use _select_with_joins for join trees (not yet implemented)"
        )

    raise NotImplementedError(f"_table_to_sqlglot: unsupported {type(te).__name__}")


def _union_to_sqlglot(node: SelectUnionNode) -> exp.Expression:
    left = to_sqlglot(node.left)
    right = to_sqlglot(node.right)
    distinct = node.quantifier.upper() != "ALL"
    op = node.op.upper()
    if op == "UNION":
        return exp.Union(this=left, expression=right, distinct=distinct)
    if op == "INTERSECT":
        return exp.Intersect(this=left, expression=right, distinct=distinct)
    if op == "EXCEPT":
        return exp.Except(this=left, expression=right, distinct=distinct)
    raise NotImplementedError(f"_union_to_sqlglot: unknown op {node.op!r}")


def _with_to_sqlglot(node: WithNode) -> exp.Expression:
    body = to_sqlglot(node.body)
    ctes = [
        exp.CTE(
            this=to_sqlglot(cte.query),
            alias=exp.TableAlias(this=exp.Identifier(this=cte.name)),
        )
        for cte in node.ctes
    ]
    return exp.With(expressions=ctes, this=body)
