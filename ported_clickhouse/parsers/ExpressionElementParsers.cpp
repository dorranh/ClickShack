#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

#include "ported_clickhouse/parsers/ASTIdentifier.h"

namespace DB
{

bool ParserIdentifier::parseImpl(Pos & pos, ASTPtr & node, Expected &)
{
    if (pos->type == TokenType::QuotedIdentifier)
    {
        const char * begin = pos->begin;
        const char * end = pos->end;

        if (end - begin <= 2)
            return false;

        // Simplified quoted identifier handling for this phase.
        if ((*begin == '`' || *begin == '"') && end[-1] == *begin)
            node = make_intrusive<ASTIdentifier>(String(begin + 1, end - 1));
        else
            node = make_intrusive<ASTIdentifier>(String(begin, end));

        ++pos;
        return true;
    }

    if (pos->type == TokenType::BareWord)
    {
        node = make_intrusive<ASTIdentifier>(String(pos->begin, pos->end));
        ++pos;
        return true;
    }

    if (allow_query_parameter && pos->type == TokenType::OpeningCurlyBrace)
    {
        // Query parameters are intentionally deferred in this phase.
        return false;
    }

    return false;
}

}
