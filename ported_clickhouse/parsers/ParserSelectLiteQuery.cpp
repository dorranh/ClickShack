#include "ported_clickhouse/parsers/ParserSelectLiteQuery.h"

#include "ported_clickhouse/parsers/ASTSelectLiteQuery.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionLiteParsers.h"

namespace DB
{

bool ParserSelectLiteQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_select(Keyword::SELECT);
    ParserExpressionListLite expressions_p;

    if (!s_select.ignore(pos, expected))
        return false;

    ASTPtr expressions;
    if (!expressions_p.parse(pos, expressions, expected))
        return false;

    auto query = make_intrusive<ASTSelectLiteQuery>();
    query->set(query->expressions, expressions);
    node = query;
    return true;
}

}
