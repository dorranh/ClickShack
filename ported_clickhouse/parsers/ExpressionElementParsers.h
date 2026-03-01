#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

namespace DB
{

class ParserIdentifier : public IParserBase
{
public:
    explicit ParserIdentifier(bool allow_query_parameter_ = false, Highlight highlight_type_ = Highlight::identifier)
        : allow_query_parameter(allow_query_parameter_)
        , highlight_type(highlight_type_)
    {
    }

    Highlight highlight() const override { return highlight_type; }

protected:
    const char * getName() const override { return "identifier"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;

private:
    bool allow_query_parameter;
    Highlight highlight_type;
};

}
