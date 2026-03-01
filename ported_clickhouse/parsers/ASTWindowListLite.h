#pragma once

#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTWindowListLite : public IAST
{
public:
    String getID(char) const override { return "WindowListLite"; }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTWindowListLite>();
        for (const auto & child : children)
            res->children.push_back(child ? child->clone() : nullptr);
        return res;
    }
};

}
