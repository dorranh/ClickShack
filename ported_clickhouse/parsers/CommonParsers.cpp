#include "ported_clickhouse/parsers/CommonParsers.h"

#include "ported_clickhouse/base/find_symbols.h"

#include <strings.h>

namespace DB
{

std::string_view toStringView(Keyword type)
{
    switch (type)
    {
        case Keyword::USE:
            return "USE";
        case Keyword::DATABASE:
            return "DATABASE";
        case Keyword::SELECT:
            return "SELECT";
        case Keyword::FROM:
            return "FROM";
        case Keyword::WHERE:
            return "WHERE";
        case Keyword::PREWHERE:
            return "PREWHERE";
        case Keyword::GROUP:
            return "GROUP";
        case Keyword::BY:
            return "BY";
        case Keyword::ORDER:
            return "ORDER";
        case Keyword::LIMIT:
            return "LIMIT";
        case Keyword::OFFSET:
            return "OFFSET";
        case Keyword::AS:
            return "AS";
        case Keyword::ON:
            return "ON";
        case Keyword::AND:
            return "AND";
        case Keyword::OR:
            return "OR";
        case Keyword::NOT:
            return "NOT";
        case Keyword::INNER:
            return "INNER";
        case Keyword::LEFT:
            return "LEFT";
        case Keyword::RIGHT:
            return "RIGHT";
        case Keyword::FULL:
            return "FULL";
        case Keyword::CROSS:
            return "CROSS";
        case Keyword::JOIN:
            return "JOIN";
        case Keyword::ASC:
            return "ASC";
        case Keyword::DESC:
            return "DESC";
        case Keyword::WITH:
            return "WITH";
        case Keyword::DISTINCT:
            return "DISTINCT";
        case Keyword::HAVING:
            return "HAVING";
        case Keyword::UNION:
            return "UNION";
        case Keyword::ALL:
            return "ALL";
        case Keyword::USING:
            return "USING";
        case Keyword::QUALIFY:
            return "QUALIFY";
        case Keyword::WINDOW:
            return "WINDOW";
        case Keyword::ARRAY:
            return "ARRAY";
        case Keyword::SAMPLE:
            return "SAMPLE";
        case Keyword::FINAL:
            return "FINAL";
        case Keyword::NULLS:
            return "NULLS";
        case Keyword::FIRST:
            return "FIRST";
        case Keyword::LAST:
            return "LAST";
        case Keyword::COLLATE:
            return "COLLATE";
    }
    return "";
}

ParserKeyword::ParserKeyword(Keyword keyword)
    : s(toStringView(keyword))
{
}

bool ParserKeyword::parseImpl(Pos & pos, ASTPtr &, Expected & expected)
{
    if (pos->type != TokenType::BareWord)
        return false;

    const char * current_word = s.data();

    while (true)
    {
        expected.add(pos, current_word);

        if (pos->type != TokenType::BareWord)
            return false;

        const char * const next_whitespace = find_first_symbols<' ', '\0'>(current_word, s.data() + s.size());
        const size_t word_length = static_cast<size_t>(next_whitespace - current_word);

        if (word_length != pos->size())
            return false;

        if (0 != strncasecmp(pos->begin, current_word, word_length))
            return false;

        ++pos;

        if (!*next_whitespace)
            break;

        current_word = next_whitespace + 1;
    }

    return true;
}

bool ParserToken::parseImpl(Pos & pos, ASTPtr &, Expected & expected)
{
    expected.add(pos, "token");
    if (pos->type != token_type)
        return false;
    ++pos;
    return true;
}

}
