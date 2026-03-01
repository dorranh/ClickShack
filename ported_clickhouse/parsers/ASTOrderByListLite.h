#pragma once

#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTOrderByListLite : public IAST
{
public:
    String getID(char) const override { return "OrderByListLite"; }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTOrderByListLite>();
        for (const auto & child : children)
            res->children.push_back(child ? child->clone() : nullptr);
        return res;
    }
};

}
