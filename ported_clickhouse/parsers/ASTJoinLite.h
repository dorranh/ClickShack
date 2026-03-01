#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTJoinLite : public IAST
{
public:
    enum class JoinType : UInt8
    {
        Inner,
        Left,
        Right,
        Full,
        Cross,
    };

    JoinType join_type = JoinType::Inner;
    IAST * left = nullptr;
    IAST * right = nullptr;
    IAST * on_expression = nullptr;

    String getID(char delim) const override
    {
        String type = "INNER";
        if (join_type == JoinType::Left)
            type = "LEFT";
        else if (join_type == JoinType::Right)
            type = "RIGHT";
        else if (join_type == JoinType::Full)
            type = "FULL";
        else if (join_type == JoinType::Cross)
            type = "CROSS";
        return "JoinLite" + String(1, delim) + type;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTJoinLite>();
        res->join_type = join_type;
        if (left)
            res->set(res->left, left->clone());
        if (right)
            res->set(res->right, right->clone());
        if (on_expression)
            res->set(res->on_expression, on_expression->clone());
        return res;
    }
};

}
