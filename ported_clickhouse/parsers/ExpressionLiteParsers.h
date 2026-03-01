#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserNumberLite : public IParserBase
{
public:
    Highlight highlight() const override { return Highlight::number; }

protected:
    const char * getName() const override { return "number"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserStringLiteralLite : public IParserBase
{
public:
    Highlight highlight() const override { return Highlight::string; }

protected:
    const char * getName() const override { return "string literal"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserLiteralLite : public IParserBase
{
protected:
    const char * getName() const override { return "literal"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserFunctionCallLite : public IParserBase
{
protected:
    const char * getName() const override { return "function call"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserExpressionLite : public IParserBase
{
protected:
    const char * getName() const override { return "expression lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

class ParserExpressionListLite : public IParserBase
{
protected:
    const char * getName() const override { return "expression list lite"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;
};

}
