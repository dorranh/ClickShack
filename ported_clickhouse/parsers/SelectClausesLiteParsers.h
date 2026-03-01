#pragma once

#include "ported_clickhouse/parsers/IParser.h"

namespace DB
{

struct SelectClausesLiteResult
{
    ASTPtr where_expression;
    ASTPtr group_by_expressions;
    ASTPtr having_expression;
    ASTPtr order_by_list;
    ASTPtr limit;
};

bool parseSelectClausesLite(IParser::Pos & pos, SelectClausesLiteResult & result, Expected & expected);

}
