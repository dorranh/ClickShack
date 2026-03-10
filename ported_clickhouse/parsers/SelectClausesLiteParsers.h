#pragma once

#include "ported_clickhouse/parsers/IParser.h"

namespace DB
{

struct SelectClausesLiteResult
{
    ASTPtr prewhere_expression;
    ASTPtr where_expression;
    ASTPtr group_by_expressions;
    ASTPtr grouping_sets_expressions;
    bool group_by_all = false;
    bool group_by_with_rollup = false;
    bool group_by_with_cube = false;
    bool group_by_with_totals = false;
    ASTPtr having_expression;
    ASTPtr window_list;
    ASTPtr qualify_expression;
    ASTPtr order_by_list;
    bool order_by_all = false;
    ASTPtr limit;
    ASTPtr limit_by;
};

bool parseSelectClausesLite(IParser::Pos & pos, SelectClausesLiteResult & result, Expected & expected);

}
