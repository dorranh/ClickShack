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
