#include "ported_clickhouse/parsers/ExpressionLiteParsers.h"

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

#include <strings.h>

namespace DB
{

namespace
{
bool isKeywordLikeFrom(TokenIterator pos)
{
    if (pos->type != TokenType::BareWord)
        return false;

    const char from_kw[] = "FROM";
    if (pos->size() != sizeof(from_kw) - 1)
        return false;
    return strncasecmp(pos->begin, from_kw, sizeof(from_kw) - 1) == 0;
}

String parseQuotedLiteral(TokenIterator pos)
{
    if (pos->size() < 2)
        return {};

    const char * begin = pos->begin;
    const char * end = pos->end;
    if ((*begin == '\'' && end[-1] == '\'') || (*begin == '"' && end[-1] == '"'))
        return String(begin + 1, end - 1);

    return String(begin, end);
}
}

bool ParserNumberLite::parseImpl(Pos & pos, ASTPtr & node, Expected &)
{
    if (pos->type != TokenType::Number)
        return false;

    node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::Number, String(pos->begin, pos->end));
    ++pos;
    return true;
}

bool ParserStringLiteralLite::parseImpl(Pos & pos, ASTPtr & node, Expected &)
{
    if (pos->type != TokenType::StringLiteral)
        return false;

    node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::String, parseQuotedLiteral(pos));
    ++pos;
    return true;
}

bool ParserLiteralLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserNumberLite number_p;
    if (number_p.parse(pos, node, expected))
        return true;

    ParserStringLiteralLite string_p;
    return string_p.parse(pos, node, expected);
}

bool ParserFunctionCallLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true, Highlight::function);
    ParserToken open_p(TokenType::OpeningRoundBracket);
    ParserToken close_p(TokenType::ClosingRoundBracket);
    ParserExpressionListLite args_p;

    ASTPtr function_name_ast;
    if (!identifier_p.parse(pos, function_name_ast, expected))
        return false;

    if (!open_p.ignore(pos, expected))
        return false;

    ASTPtr arguments_ast;
    if (!close_p.ignore(pos, expected))
    {
        if (!args_p.parse(pos, arguments_ast, expected))
            return false;

        if (!close_p.ignore(pos, expected))
            return false;
    }

    auto * identifier = function_name_ast ? function_name_ast->as<ASTIdentifier>() : nullptr;
    if (!identifier)
        return false;

    auto function = make_intrusive<ASTFunction>();
    function->name = identifier->name();
    if (arguments_ast)
        function->set(function->arguments, arguments_ast);
    node = function;
    return true;
}

bool ParserExpressionLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    if (isKeywordLikeFrom(pos))
        return false;

    ParserFunctionCallLite function_p;
    if (function_p.parse(pos, node, expected))
        return true;

    ParserLiteralLite literal_p;
    if (literal_p.parse(pos, node, expected))
        return true;

    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    return identifier_p.parse(pos, node, expected);
}

bool ParserExpressionListLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserExpressionLite expr_p;
    ParserToken comma_p(TokenType::Comma);

    auto list = make_intrusive<ASTExpressionList>();

    ASTPtr first_expr;
    if (!expr_p.parse(pos, first_expr, expected))
        return false;
    list->children.push_back(first_expr);

    while (comma_p.ignore(pos, expected))
    {
        ASTPtr expr;
        if (!expr_p.parse(pos, expr, expected))
            return false;
        list->children.push_back(expr);
    }

    node = list;
    return true;
}

}
