#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTWindowDefinitionLite : public IAST
{
public:
    String name;
    String body;

    String getID(char delim) const override
    {
        return "WindowDefinitionLite" + String(1, delim) + name;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTWindowDefinitionLite>();
        res->name = name;
        res->body = body;
        return res;
    }
};

}
