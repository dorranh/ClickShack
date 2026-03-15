from __future__ import annotations

from typing import Annotated, Any, Literal

from pydantic import BaseModel, Field


class Span(BaseModel):
    start: int
    end: int


# ── Expression nodes ──────────────────────────────────────────────────────────

class LiteralNode(BaseModel):
    type: Literal["Literal"]
    span: Span
    kind: Literal["number", "string", "null"]
    value: str | None = None


class ColumnNode(BaseModel):
    type: Literal["Column"]
    span: Span
    name: str
    table: str | None = None


class StarNode(BaseModel):
    type: Literal["Star"]
    span: Span


class TableStarNode(BaseModel):
    type: Literal["TableStar"]
    span: Span
    table: str


class FunctionNode(BaseModel):
    type: Literal["Function"]
    span: Span
    name: str
    args: list[IrExpr] = Field(default_factory=list)
    over: WindowSpecInline | None = None


class BinaryOpNode(BaseModel):
    type: Literal["BinaryOp"]
    span: Span
    op: str
    left: IrExpr
    right: IrExpr


class UnaryOpNode(BaseModel):
    type: Literal["UnaryOp"]
    span: Span
    op: str
    expr: IrExpr


class CaseBranch(BaseModel):
    when: IrExpr
    then: IrExpr


class CaseNode(BaseModel):
    type: Literal["Case"]
    span: Span
    operand: IrExpr | None = None
    branches: list[CaseBranch] = Field(default_factory=list)
    else_expr: IrExpr | None = Field(None, alias="else")
    model_config = {"populate_by_name": True}


class CastNode(BaseModel):
    type: Literal["Cast"]
    span: Span
    expr: IrExpr
    cast_type: str


class InNode(BaseModel):
    type: Literal["In"]
    span: Span
    negated: bool
    expr: IrExpr
    list: list[IrExpr] | None = None
    subquery: SubqueryNode | None = None


class BetweenNode(BaseModel):
    type: Literal["Between"]
    span: Span
    negated: bool
    expr: IrExpr
    low: IrExpr
    high: IrExpr


class LikeNode(BaseModel):
    type: Literal["Like"]
    span: Span
    negated: bool
    ilike: bool
    expr: IrExpr
    pattern: IrExpr
    escape: IrExpr | None = None


class IsNullNode(BaseModel):
    type: Literal["IsNull"]
    span: Span
    negated: bool
    expr: IrExpr


class LambdaNode(BaseModel):
    type: Literal["Lambda"]
    span: Span
    params: list[IrExpr]
    body: IrExpr


class ArrayNode(BaseModel):
    type: Literal["Array"]
    span: Span
    elements: list[IrExpr]


class TupleNode(BaseModel):
    type: Literal["Tuple"]
    span: Span
    elements: list[IrExpr]


class SubqueryNode(BaseModel):
    type: Literal["Subquery"]
    span: Span
    query: IrQuery


class RawNode(BaseModel):
    type: Literal["Raw"]
    span: Span
    id: str | None = None


class AliasNode(BaseModel):
    type: Literal["Alias"]
    span: Span
    expr: IrExpr
    alias: str


class OverNode(BaseModel):
    type: Literal["Over"]
    span: Span
    function: IrExpr
    window: IrExpr | None = None


# ── Table expression nodes ────────────────────────────────────────────────────

class TableNode(BaseModel):
    type: Literal["Table"]
    span: Span
    name: str
    database: str | None = None
    alias: str | None = None
    final: bool | None = None
    sample: IrExpr | None = None
    sample_offset: IrExpr | None = None


class TableFunctionNode(BaseModel):
    type: Literal["TableFunction"]
    span: Span
    function: FunctionNode
    alias: str | None = None


class SubquerySourceNode(BaseModel):
    type: Literal["SubquerySource"]
    span: Span
    query: IrQuery
    alias: str | None = None


class JoinNode(BaseModel):
    type: Literal["Join"]
    span: Span
    kind: str
    locality: str | None = None
    strictness: str | None = None
    left: IrTableExpr
    right: IrTableExpr
    on: IrExpr | None = None
    using: list[IrExpr] | None = None


# ── Inline window spec ────────────────────────────────────────────────────────

class WindowSpecInline(BaseModel):
    name: str | None = None
    is_reference: bool = False
    body: str | None = None


# ── Clause types ─────────────────────────────────────────────────────────────

class LimitNode(BaseModel):
    type: Literal["Limit"]
    span: Span
    count: IrExpr | None = None
    offset: IrExpr | None = None
    with_ties: bool = False


class LimitByNode(BaseModel):
    type: Literal["LimitBy"]
    span: Span
    count: IrExpr | None = None
    offset: IrExpr | None = None
    by: Any | None = None


class OrderByElement(BaseModel):
    span: Span
    expr: IrExpr
    direction: Literal["ASC", "DESC"] = "ASC"
    nulls: Literal["FIRST", "LAST"] | None = None
    collate: str | None = None
    with_fill: dict[str, Any] | None = None
    interpolate: list[IrExpr] | None = None


class WindowDefNode(BaseModel):
    span: Span
    name: str | None = None
    is_reference: bool = False
    body: str | None = None


class CteDef(BaseModel):
    name: str | None = None
    query: IrQuery | None = None
    expr: IrExpr | None = None


# ── Root nodes ────────────────────────────────────────────────────────────────

class ArrayJoinInline(BaseModel):
    left: bool = False
    exprs: list[IrExpr] = Field(default_factory=list)


class SelectNode(BaseModel):
    type: Literal["Select"]
    span: Span
    distinct: bool = False
    all: bool = False
    distinct_on: list[IrExpr] | None = None
    top: dict[str, Any] | None = None
    projections: list[IrExpr] = Field(default_factory=list)
    from_: IrTableExpr | None = Field(None, alias="from")
    array_join: ArrayJoinInline | None = None
    prewhere: IrExpr | None = None
    where: IrExpr | None = None
    group_by: dict[str, Any] | None = None
    having: IrExpr | None = None
    window: list[WindowDefNode] | None = None
    qualify: IrExpr | None = None
    order_by: Any | None = None
    limit: LimitNode | None = None
    limit_by: LimitByNode | None = None
    settings: list[IrExpr] | None = None
    format: str | None = None
    model_config = {"populate_by_name": True}


class SelectUnionNode(BaseModel):
    type: Literal["SelectUnion"]
    span: Span
    op: str
    quantifier: str = ""
    left: IrQuery
    right: IrQuery


class WithNode(BaseModel):
    type: Literal["With"]
    span: Span
    recursive: bool = False
    ctes: list[CteDef] = Field(default_factory=list)
    body: IrQuery


# ── Discriminated unions ──────────────────────────────────────────────────────

IrExpr = Annotated[
    LiteralNode
    | ColumnNode
    | StarNode
    | TableStarNode
    | FunctionNode
    | BinaryOpNode
    | UnaryOpNode
    | CaseNode
    | CastNode
    | InNode
    | BetweenNode
    | LikeNode
    | IsNullNode
    | LambdaNode
    | ArrayNode
    | TupleNode
    | SubqueryNode
    | AliasNode
    | OverNode
    | RawNode,
    Field(discriminator="type"),
]

IrTableExpr = Annotated[
    TableNode | TableFunctionNode | SubquerySourceNode | JoinNode,
    Field(discriminator="type"),
]

IrQuery = Annotated[
    WithNode | SelectUnionNode | SelectNode,
    Field(discriminator="type"),
]

# Rebuild all models that contain forward references
for _cls in [
    CaseBranch, FunctionNode, SubqueryNode, SubquerySourceNode, JoinNode,
    ArrayJoinInline, CaseNode, InNode, BetweenNode, LikeNode, IsNullNode,
    LambdaNode, ArrayNode, TupleNode, BinaryOpNode, UnaryOpNode,
    CastNode, AliasNode, OverNode, WithNode, SelectUnionNode, SelectNode,
    LimitNode, LimitByNode, OrderByElement, CteDef, TableNode,
    TableFunctionNode,
]:
    _cls.model_rebuild()


def parse_node(data: dict) -> Any:
    """Parse a single IR expression node dict into the appropriate typed model."""
    from pydantic import TypeAdapter
    return TypeAdapter(IrExpr).validate_python(data)


# ── Envelope ──────────────────────────────────────────────────────────────────

class IrEnvelope(BaseModel):
    ir_version: str
    query: IrQuery
