#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTLimitLite : public IAST
{
public:
    String limit;
    IAST * limit_expression = nullptr;
    bool offset_present = false;
    String offset;
    IAST * offset_expression = nullptr;
    bool with_ties = false;

    String getID(char delim) const override
    {
        return "LimitLite" + String(1, delim) + limit
            + (offset_present ? (String("+") + offset) : String())
            + (with_ties ? String("+ties") : String());
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTLimitLite>();
        res->limit = limit;
        if (limit_expression)
            res->set(res->limit_expression, limit_expression->clone());
        res->offset_present = offset_present;
        res->offset = offset;
        if (offset_expression)
            res->set(res->offset_expression, offset_expression->clone());
        res->with_ties = with_ties;
        return res;
    }
};

}
