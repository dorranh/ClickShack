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
    ASTExpressionList * by_expressions = nullptr;

    String getID(char delim) const override
    {
        return "LimitByLite" + String(1, delim) + limit;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTLimitByLite>();
        res->limit = limit;
        if (by_expressions)
            res->set(res->by_expressions, by_expressions->clone());
        return res;
    }
};

}
