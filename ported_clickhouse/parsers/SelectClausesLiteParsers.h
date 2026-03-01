#pragma once

#include "ported_clickhouse/parsers/IParser.h"

namespace DB
{

struct SelectClausesLiteResult
{
    ASTPtr prewhere_expression;
    ASTPtr where_expression;
    ASTPtr group_by_expressions;
    ASTPtr having_expression;
    ASTPtr window_list;
    ASTPtr qualify_expression;
    ASTPtr order_by_list;
    ASTPtr limit;
    ASTPtr limit_by;
};

bool parseSelectClausesLite(IParser::Pos & pos, SelectClausesLiteResult & result, Expected & expected);

}
