#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserExpressionOpsLite : public IParserBase
{
protected:
    const char * getName() const override { return "expression ops lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserExpressionListOpsLite : public IParserBase
{
protected:
    const char * getName() const override { return "expression list ops lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserProjectionListOpsLite : public IParserBase
{
protected:
    const char * getName() const override { return "projection list ops lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
