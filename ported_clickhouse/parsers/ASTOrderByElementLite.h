#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTExpressionList.h"
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
    enum class NullsOrder : UInt8
    {
        Unspecified,
        First,
        Last,
    };

    IAST * expression = nullptr;
    Direction direction = Direction::Asc;
    NullsOrder nulls_order = NullsOrder::Unspecified;
    String collate_name;
    bool with_fill = false;
    IAST * fill_from = nullptr;
    IAST * fill_to = nullptr;
    IAST * fill_step = nullptr;
    IAST * fill_staleness = nullptr;
    bool interpolate = false;
    ASTExpressionList * interpolate_expressions = nullptr;

    String getID(char delim) const override
    {
        return "OrderByElementLite" + String(1, delim) + (direction == Direction::Asc ? "ASC" : "DESC");
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTOrderByElementLite>();
        res->direction = direction;
        res->nulls_order = nulls_order;
        res->collate_name = collate_name;
        res->with_fill = with_fill;
        res->interpolate = interpolate;
        if (expression)
            res->set(res->expression, expression->clone());
        if (fill_from)
            res->set(res->fill_from, fill_from->clone());
        if (fill_to)
            res->set(res->fill_to, fill_to->clone());
        if (fill_step)
            res->set(res->fill_step, fill_step->clone());
        if (fill_staleness)
            res->set(res->fill_staleness, fill_staleness->clone());
        if (interpolate_expressions)
            res->set(res->interpolate_expressions, interpolate_expressions->clone());
        return res;
    }
};

}
