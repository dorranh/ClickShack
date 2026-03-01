#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTOrderByElementLite : public IAST
{
public:
    enum class Direction : UInt8
    {
        Asc,
        Desc,
    };

    IAST * expression = nullptr;
    Direction direction = Direction::Asc;

    String getID(char delim) const override
    {
        return "OrderByElementLite" + String(1, delim) + (direction == Direction::Asc ? "ASC" : "DESC");
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTOrderByElementLite>();
        res->direction = direction;
        if (expression)
            res->set(res->expression, expression->clone());
        return res;
    }
};

}
