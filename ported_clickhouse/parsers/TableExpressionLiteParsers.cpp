#include "ported_clickhouse/parsers/TableExpressionLiteParsers.h"

#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTTableIdentifierLite.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

namespace DB
{

bool ParserTableIdentifierLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserToken dot_p(TokenType::Dot);

    ASTPtr left_name_ast;
    if (!identifier_p.parse(pos, left_name_ast, expected))
        return false;

    auto * left_name = left_name_ast ? left_name_ast->as<ASTIdentifier>() : nullptr;
    if (!left_name)
        return false;

    String database;
    String table = left_name->name();

    if (dot_p.ignore(pos, expected))
    {
        ASTPtr right_name_ast;
        if (!identifier_p.parse(pos, right_name_ast, expected))
            return false;

        auto * right_name = right_name_ast ? right_name_ast->as<ASTIdentifier>() : nullptr;
        if (!right_name)
            return false;

        database = table;
        table = right_name->name();
    }

    auto table_id = make_intrusive<ASTTableIdentifierLite>();
    table_id->database = database;
    table_id->table = table;
    node = table_id;
    return true;
}

}
