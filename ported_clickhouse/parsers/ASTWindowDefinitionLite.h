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
    bool is_reference = false;

    String getID(char delim) const override
    {
        return "WindowDefinitionLite" + String(1, delim) + (is_reference ? "ref:" : "def:") + name + "|" + body;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTWindowDefinitionLite>();
        res->name = name;
        res->body = body;
        res->is_reference = is_reference;
        return res;
    }
};

}
