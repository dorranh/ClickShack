#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTExpressionOpsLite : public IAST
{
public:
    String op;
    bool is_unary = false;
    IAST * left = nullptr;
    IAST * right = nullptr;

    String getID(char delim) const override
    {
        return "ExpressionOpsLite" + String(1, delim) + op;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTExpressionOpsLite>();
        res->op = op;
        res->is_unary = is_unary;
        if (left)
            res->set(res->left, left->clone());
        if (right)
            res->set(res->right, right->clone());
        return res;
    }
};

}
