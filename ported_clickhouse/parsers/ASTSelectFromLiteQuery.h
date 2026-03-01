#pragma once

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTTableIdentifierLite.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTSelectFromLiteQuery : public IAST
{
public:
    ASTExpressionList * expressions = nullptr;
    ASTTableIdentifierLite * from_table = nullptr;

    String getID(char delim) const override
    {
        return "SelectFromLiteQuery" + String(1, delim) + "from";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectFromLiteQuery>();
        if (expressions)
            res->set(res->expressions, expressions->clone());
        if (from_table)
            res->set(res->from_table, from_table->clone());
        return res;
    }
};

}
