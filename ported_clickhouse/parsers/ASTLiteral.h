#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

#include <utility>

namespace DB
{

class ASTLiteral : public IAST
{
public:
    enum class Kind : UInt8
    {
        Number,
        String,
    };

    ASTLiteral(Kind kind_, String value_)
        : kind(kind_)
        , value(std::move(value_))
    {
    }

    Kind kind;
    String value;

    String getID(char delim) const override
    {
        return "Literal" + String(1, delim) + value;
    }

    ASTPtr clone() const override
    {
        return make_intrusive<ASTLiteral>(kind, value);
    }
};

}
