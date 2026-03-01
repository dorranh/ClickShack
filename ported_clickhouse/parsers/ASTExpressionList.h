#pragma once

#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTExpressionList : public IAST
{
public:
    explicit ASTExpressionList(char separator_ = ',')
        : separator(separator_)
    {
    }

    char separator = ',';

    String getID(char) const override { return "ExpressionList"; }
    ASTPtr clone() const override;
};

}
