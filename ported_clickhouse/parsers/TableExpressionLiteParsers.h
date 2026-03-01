#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserTableIdentifierLite : public IParserBase
{
protected:
    const char * getName() const override { return "table identifier lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
