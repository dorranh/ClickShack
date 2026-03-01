#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTTableAliasLite : public IAST
{
public:
    String alias;

    String getID(char delim) const override
    {
        return "TableAliasLite" + String(1, delim) + alias;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTTableAliasLite>();
        res->alias = alias;
        return res;
    }
};

}
