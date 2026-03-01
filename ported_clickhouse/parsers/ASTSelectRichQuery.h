#pragma once

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTSelectRichQuery : public IAST
{
public:
    ASTExpressionList * expressions = nullptr;
    IAST * from_source = nullptr;
    IAST * where_expression = nullptr;
    ASTExpressionList * group_by_expressions = nullptr;
    ASTOrderByListLite * order_by_list = nullptr;
    ASTLimitLite * limit = nullptr;

    String getID(char delim) const override
    {
        return "SelectRichQuery" + String(1, delim) + "core";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectRichQuery>();
        if (expressions)
            res->set(res->expressions, expressions->clone());
        if (from_source)
            res->set(res->from_source, from_source->clone());
        if (where_expression)
            res->set(res->where_expression, where_expression->clone());
        if (group_by_expressions)
            res->set(res->group_by_expressions, group_by_expressions->clone());
        if (order_by_list)
            res->set(res->order_by_list, order_by_list->clone());
        if (limit)
            res->set(res->limit, limit->clone());
        return res;
    }
};

}
