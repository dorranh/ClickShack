#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTLimitByLite : public IAST
{
public:
    String limit;
    IAST * limit_expression = nullptr;
    bool offset_present = false;
    String offset;
    IAST * offset_expression = nullptr;
    ASTExpressionList * by_expressions = nullptr;
    bool by_all = false;

    String getID(char delim) const override
    {
        return "LimitByLite" + String(1, delim) + limit
            + (offset_present ? (String("+") + offset) : String())
            + (by_all ? String("+all") : String());
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTLimitByLite>();
        res->limit = limit;
        if (limit_expression)
            res->set(res->limit_expression, limit_expression->clone());
        res->offset_present = offset_present;
        res->offset = offset;
        if (offset_expression)
            res->set(res->offset_expression, offset_expression->clone());
        res->by_all = by_all;
        if (by_expressions)
            res->set(res->by_expressions, by_expressions->clone());
        return res;
    }
};

}
