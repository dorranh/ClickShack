#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserSelectFromLiteQuery : public IParserBase
{
protected:
    const char * getName() const override { return "SELECT FROM lite query"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
