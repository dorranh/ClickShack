#pragma once

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTSelectLiteQuery : public IAST
{
public:
    ASTExpressionList * expressions = nullptr;

    String getID(char delim) const override
    {
        return "SelectLiteQuery" + String(1, delim) + "exprs";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectLiteQuery>();
        if (expressions)
            res->set(res->expressions, expressions->clone());
        return res;
    }
};

}
