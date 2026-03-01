#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserSelectLiteQuery : public IParserBase
{
protected:
    const char * getName() const override { return "SELECT lite query"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
