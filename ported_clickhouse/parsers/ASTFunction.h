#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTFunction : public IAST
{
public:
    String name;
    ASTExpressionList * arguments = nullptr;

    String getID(char delim) const override { return "Function" + String(1, delim) + name; }
    ASTPtr clone() const override;
};

}
