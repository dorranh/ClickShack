#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/IAST.h"

#include <vector>

namespace DB
{

class ASTSelectSetLite : public IAST
{
public:
    std::vector<String> union_modes;
    std::vector<String> set_ops;
    ASTExpressionList * settings = nullptr;
    bool has_format = false;
    String format_name;

    String getID(char delim) const override
    {
        return "SelectSetLite" + String(1, delim) + "union";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectSetLite>();
        res->union_modes = union_modes;
        res->set_ops = set_ops;
        if (settings)
            res->set(res->settings, settings->clone());
        res->has_format = has_format;
        res->format_name = format_name;
        for (const auto & child : children)
            res->children.push_back(child ? child->clone() : nullptr);
        return res;
    }
};

}
