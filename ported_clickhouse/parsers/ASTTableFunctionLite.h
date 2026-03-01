#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTTableFunctionLite : public IAST
{
public:
    String name;
    ASTExpressionList * arguments = nullptr;

    String getID(char delim) const override
    {
        return "TableFunctionLite" + String(1, delim) + name;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTTableFunctionLite>();
        res->name = name;
        if (arguments)
            res->set(res->arguments, arguments->clone());
        return res;
    }
};

}
