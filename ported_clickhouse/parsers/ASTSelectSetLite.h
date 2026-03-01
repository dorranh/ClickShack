#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

#include <vector>

namespace DB
{

class ASTSelectSetLite : public IAST
{
public:
    std::vector<String> union_modes;

    String getID(char delim) const override
    {
        return "SelectSetLite" + String(1, delim) + "union";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectSetLite>();
        res->union_modes = union_modes;
        for (const auto & child : children)
            res->children.push_back(child ? child->clone() : nullptr);
        return res;
    }
};

}
