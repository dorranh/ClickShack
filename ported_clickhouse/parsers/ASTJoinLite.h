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
        Paste,
    };

    enum class JoinLocality : UInt8
    {
        Unspecified,
        Global,
        Local,
    };

    JoinType join_type = JoinType::Inner;
    JoinLocality join_locality = JoinLocality::Unspecified;
    bool is_global = false;
    bool is_local = false;
    String strictness;
    IAST * left = nullptr;
    IAST * right = nullptr;
    IAST * on_expression = nullptr;
    IAST * using_columns = nullptr;

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
        else if (join_type == JoinType::Paste)
            type = "PASTE";
        return "JoinLite" + String(1, delim) + type;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTJoinLite>();
        res->join_type = join_type;
        res->join_locality = join_locality;
        res->is_global = is_global;
        res->is_local = is_local;
        res->strictness = strictness;
        if (left)
            res->set(res->left, left->clone());
        if (right)
            res->set(res->right, right->clone());
        if (on_expression)
            res->set(res->on_expression, on_expression->clone());
        if (using_columns)
            res->set(res->using_columns, using_columns->clone());
        return res;
    }
};

}
