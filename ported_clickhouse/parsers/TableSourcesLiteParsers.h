#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserTableSourceLite : public IParserBase
{
protected:
    const char * getName() const override { return "table source lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

bool parseNestedSelectRichQuery(IParser::Pos & pos, ASTPtr & node, Expected & expected);

}
