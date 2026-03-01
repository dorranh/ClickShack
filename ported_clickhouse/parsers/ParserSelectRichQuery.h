#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserSelectRichQuery : public IParserBase
{
protected:
    const char * getName() const override { return "SELECT rich query"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
