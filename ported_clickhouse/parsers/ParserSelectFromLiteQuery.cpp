#include "ported_clickhouse/parsers/ParserSelectFromLiteQuery.h"

#include "ported_clickhouse/parsers/ASTSelectFromLiteQuery.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionLiteParsers.h"
#include "ported_clickhouse/parsers/TableExpressionLiteParsers.h"

namespace DB
{

bool ParserSelectFromLiteQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_select(Keyword::SELECT);
    ParserKeyword s_from(Keyword::FROM);
    ParserExpressionListLite expressions_p;
    ParserTableIdentifierLite table_p;

    if (!s_select.ignore(pos, expected))
        return false;

    ASTPtr expressions;
    if (!expressions_p.parse(pos, expressions, expected))
        return false;

    if (!s_from.ignore(pos, expected))
        return false;

    ASTPtr from_table;
    if (!table_p.parse(pos, from_table, expected))
        return false;

    auto query = make_intrusive<ASTSelectFromLiteQuery>();
    query->set(query->expressions, expressions);
    query->set(query->from_table, from_table);
    node = query;
    return true;
}

}
