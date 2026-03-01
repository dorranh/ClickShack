#include "ported_clickhouse/parsers/ParserUseQuery.h"

#include "ported_clickhouse/parsers/ASTUseQuery.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

namespace DB
{

bool ParserUseQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_use(Keyword::USE);
    ParserKeyword s_database(Keyword::DATABASE);
    ParserIdentifier name_p(/*allow_query_parameter*/ true);

    if (!s_use.ignore(pos, expected))
        return false;

    ASTPtr database;
    Expected test_expected;

    Pos test_pos = pos;
    ASTPtr test_node;

    bool has_database_keyword_pattern =
        s_database.parse(test_pos, test_node, test_expected) &&
        name_p.parse(test_pos, test_node, test_expected);

    if (has_database_keyword_pattern)
    {
        s_database.ignore(pos, expected);
        if (!name_p.parse(pos, database, expected))
            return false;
    }
    else
    {
        if (!name_p.parse(pos, database, expected))
            return false;
    }

    auto query = make_intrusive<ASTUseQuery>();
    query->set(query->database, database);
    node = query;

    return true;
}

}
