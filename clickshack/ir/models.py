from __future__ import annotations

from typing import Annotated, Any, Literal, Optional, Union

from pydantic import BaseModel, Field


class Span(BaseModel):
    start: int
    end: int


# ── Expression nodes ──────────────────────────────────────────────────────────

class LiteralNode(BaseModel):
    type: Literal["Literal"]
    span: Span
    kind: Literal["number", "string", "null"]
    value: Optional[str] = None


class ColumnNode(BaseModel):
    type: Literal["Column"]
    span: Span
    name: str
    table: Optional[str] = None


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
    over: Optional[WindowSpecInline] = None


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
    operand: Optional[IrExpr] = None
    branches: list[CaseBranch] = Field(default_factory=list)
    else_expr: Optional[IrExpr] = Field(None, alias="else")
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
    list: Optional[list[IrExpr]] = None
    subquery: Optional[SubqueryNode] = None


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
    escape: Optional[IrExpr] = None


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
    id: Optional[str] = None


# ── Table expression nodes ────────────────────────────────────────────────────

class TableNode(BaseModel):
    type: Literal["Table"]
    span: Span
    name: str
    database: Optional[str] = None
    alias: Optional[str] = None
    final: Optional[bool] = None
    sample: Optional[IrExpr] = None
    sample_offset: Optional[IrExpr] = None


class TableFunctionNode(BaseModel):
    type: Literal["TableFunction"]
    span: Span
    function: FunctionNode
    alias: Optional[str] = None


class SubquerySourceNode(BaseModel):
    type: Literal["SubquerySource"]
    span: Span
    query: IrQuery
    alias: Optional[str] = None


class JoinNode(BaseModel):
    type: Literal["Join"]
    span: Span
    kind: str
    locality: Optional[str] = None
    strictness: Optional[str] = None
    left: IrTableExpr
    right: IrTableExpr
    on: Optional[IrExpr] = None
    using: Optional[list[IrExpr]] = None


# ── Inline window spec ────────────────────────────────────────────────────────

class WindowSpecInline(BaseModel):
    name: Optional[str] = None
    is_reference: bool = False
    body: Optional[str] = None


# ── Clause types ─────────────────────────────────────────────────────────────

class LimitNode(BaseModel):
    type: Literal["Limit"]
    span: Span
    count: Optional[IrExpr] = None
    offset: Optional[IrExpr] = None
    with_ties: bool = False


class LimitByNode(BaseModel):
    type: Literal["LimitBy"]
    span: Span
    count: Optional[IrExpr] = None
    offset: Optional[IrExpr] = None
    by: Optional[Any] = None


class OrderByElement(BaseModel):
    span: Span
    expr: IrExpr
    direction: Literal["ASC", "DESC"] = "ASC"
    nulls: Optional[Literal["FIRST", "LAST"]] = None
    collate: Optional[str] = None
    with_fill: Optional[dict[str, Any]] = None
    interpolate: Optional[list[IrExpr]] = None


class WindowDefNode(BaseModel):
    span: Span
    name: Optional[str] = None
    is_reference: bool = False
    body: Optional[str] = None


class CteDef(BaseModel):
    name: str
    query: IrQuery


# ── Root nodes ────────────────────────────────────────────────────────────────

class ArrayJoinInline(BaseModel):
    left: bool = False
    exprs: list[IrExpr] = Field(default_factory=list)


class SelectNode(BaseModel):
    type: Literal["Select"]
    span: Span
    distinct: bool = False
    all: bool = False
    distinct_on: Optional[list[IrExpr]] = None
    top: Optional[dict[str, Any]] = None
    projections: list[IrExpr] = Field(default_factory=list)
    from_: Optional[IrTableExpr] = Field(None, alias="from")
    array_join: Optional[ArrayJoinInline] = None
    prewhere: Optional[IrExpr] = None
    where: Optional[IrExpr] = None
    group_by: Optional[dict[str, Any]] = None
    having: Optional[IrExpr] = None
    window: Optional[list[WindowDefNode]] = None
    qualify: Optional[IrExpr] = None
    order_by: Optional[Any] = None
    limit: Optional[LimitNode] = None
    limit_by: Optional[LimitByNode] = None
    settings: Optional[list[IrExpr]] = None
    format: Optional[str] = None
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
    Union[
        LiteralNode, ColumnNode, StarNode, TableStarNode,
        FunctionNode, BinaryOpNode, UnaryOpNode, CaseNode,
        CastNode, InNode, BetweenNode, LikeNode, IsNullNode,
        LambdaNode, ArrayNode, TupleNode, SubqueryNode, RawNode,
    ],
    Field(discriminator="type"),
]

IrTableExpr = Annotated[
    Union[TableNode, TableFunctionNode, SubquerySourceNode, JoinNode],
    Field(discriminator="type"),
]

IrQuery = Annotated[
    Union[WithNode, SelectUnionNode, SelectNode],
    Field(discriminator="type"),
]

# Rebuild all models that contain forward references
for _cls in [
    CaseBranch, FunctionNode, SubqueryNode, SubquerySourceNode, JoinNode,
    ArrayJoinInline, CaseNode, InNode, BetweenNode, LikeNode, IsNullNode,
    LambdaNode, ArrayNode, TupleNode, BinaryOpNode, UnaryOpNode,
    CastNode, WithNode, SelectUnionNode, SelectNode,
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
