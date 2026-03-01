#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTLimitLite : public IAST
{
public:
    String limit;
    bool offset_present = false;
    String offset;
    bool with_ties = false;

    String getID(char delim) const override
    {
        return "LimitLite" + String(1, delim) + limit
            + (offset_present ? (String("+") + offset) : String())
            + (with_ties ? String("+ties") : String());
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTLimitLite>();
        res->limit = limit;
        res->offset_present = offset_present;
        res->offset = offset;
        res->with_ties = with_ties;
        return res;
    }
};

}
