#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserUseQuery : public IParserBase
{
protected:
    const char * getName() const override { return "USE query"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
