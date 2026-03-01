#pragma once

#include "ported_clickhouse/parsers/IParserBase.h"

#include <string_view>

namespace DB
{

enum class Keyword : UInt8
{
    USE,
    DATABASE,
    SELECT,
    FROM,
    WHERE,
    PREWHERE,
    GROUP,
    BY,
    ORDER,
    LIMIT,
    OFFSET,
    AS,
    ON,
    AND,
    OR,
    NOT,
    INNER,
    LEFT,
    RIGHT,
    FULL,
    CROSS,
    JOIN,
    ASC,
    DESC,
    WITH,
    DISTINCT,
    HAVING,
    UNION,
    ALL,
    USING,
    QUALIFY,
    WINDOW,
    ARRAY,
    SAMPLE,
    FINAL,
    NULLS,
    FIRST,
    LAST,
    COLLATE,
    GLOBAL,
    OUTER,
    ANY,
    SEMI,
    ANTI,
    ASOF,
    TIES,
    INTERSECT,
    EXCEPT,
    RECURSIVE,
    FETCH,
    NEXT,
    ROW,
    ROWS,
    ONLY,
    SETTINGS,
    FORMAT,
    ROLLUP,
    CUBE,
    TOTALS,
    ESCAPE,
    FILL,
    TO,
    STEP,
    STALENESS,
    INTERPOLATE,
};

std::string_view toStringView(Keyword type);

class ParserKeyword : public IParserBase
{
public:
    explicit ParserKeyword(std::string_view s_) : s(s_) {}
    explicit ParserKeyword(Keyword keyword);

protected:
    const char * getName() const override { return "keyword"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;

private:
    std::string_view s;
};

class ParserToken : public IParserBase
{
public:
    explicit ParserToken(TokenType token_type_) : token_type(token_type_) {}

protected:
    const char * getName() const override { return "token"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;

private:
    TokenType token_type;
};

class ParserNothing : public IParserBase
{
protected:
    const char * getName() const override { return "nothing"; }
    bool parseImpl(Pos &, ASTPtr &, Expected &) override { return true; }
};

}
