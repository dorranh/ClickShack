#pragma once

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTSelectRichQuery : public IAST
{
public:
    ASTExpressionList * with_expressions = nullptr;
    bool distinct = false;
    ASTExpressionList * expressions = nullptr;
    IAST * from_source = nullptr;
    IAST * where_expression = nullptr;
    ASTExpressionList * group_by_expressions = nullptr;
    IAST * having_expression = nullptr;
    ASTWindowListLite * window_list = nullptr;
    IAST * qualify_expression = nullptr;
    ASTOrderByListLite * order_by_list = nullptr;
    ASTLimitLite * limit = nullptr;
    ASTLimitByLite * limit_by = nullptr;

    String getID(char delim) const override
    {
        return "SelectRichQuery" + String(1, delim) + "core";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectRichQuery>();
        res->distinct = distinct;
        if (with_expressions)
            res->set(res->with_expressions, with_expressions->clone());
        if (expressions)
            res->set(res->expressions, expressions->clone());
        if (from_source)
            res->set(res->from_source, from_source->clone());
        if (where_expression)
            res->set(res->where_expression, where_expression->clone());
        if (group_by_expressions)
            res->set(res->group_by_expressions, group_by_expressions->clone());
        if (having_expression)
            res->set(res->having_expression, having_expression->clone());
        if (window_list)
            res->set(res->window_list, window_list->clone());
        if (qualify_expression)
            res->set(res->qualify_expression, qualify_expression->clone());
        if (order_by_list)
            res->set(res->order_by_list, order_by_list->clone());
        if (limit)
            res->set(res->limit, limit->clone());
        if (limit_by)
            res->set(res->limit_by, limit_by->clone());
        return res;
    }
};

}
