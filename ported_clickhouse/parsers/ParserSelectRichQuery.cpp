#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"

#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionOpsLiteParsers.h"
#include "ported_clickhouse/parsers/SelectClausesLiteParsers.h"
#include "ported_clickhouse/parsers/TableSourcesLiteParsers.h"

namespace DB
{

bool parseNestedSelectRichQuery(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserSelectRichQuery nested;
    return nested.parse(pos, node, expected);
}

bool ParserSelectRichQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_select(Keyword::SELECT);
    ParserKeyword s_from(Keyword::FROM);

    if (!s_select.ignore(pos, expected))
        return false;

    ParserExpressionListOpsLite projection_p;
    ASTPtr projections;
    if (!projection_p.parse(pos, projections, expected))
        return false;

    if (!s_from.ignore(pos, expected))
        return false;

    ParserTableSourceLite source_p;
    ASTPtr source;
    if (!source_p.parse(pos, source, expected))
        return false;

    SelectClausesLiteResult clauses;
    if (!parseSelectClausesLite(pos, clauses, expected))
        return false;

    auto query = make_intrusive<ASTSelectRichQuery>();
    query->set(query->expressions, projections);
    query->set(query->from_source, source);
    if (clauses.where_expression)
        query->set(query->where_expression, clauses.where_expression);
    if (clauses.group_by_expressions)
        query->set(query->group_by_expressions, clauses.group_by_expressions);
    if (clauses.order_by_list)
        query->set(query->order_by_list, clauses.order_by_list);
    if (clauses.limit)
        query->set(query->limit, clauses.limit);

    node = query;
    return true;
}

}
