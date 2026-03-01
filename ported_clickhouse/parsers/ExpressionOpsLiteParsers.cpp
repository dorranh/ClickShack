#include "ported_clickhouse/parsers/ExpressionOpsLiteParsers.h"

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTExpressionOpsLite.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

#include <cctype>
#include <map>
#include <strings.h>

namespace DB
{

namespace
{
bool isKeyword(TokenIterator pos, const char * keyword)
{
    if (pos->type != TokenType::BareWord)
        return false;

    const size_t len = strlen(keyword);
    return pos->size() == len && strncasecmp(pos->begin, keyword, len) == 0;
}

bool isReservedExpressionBoundary(TokenIterator pos)
{
    return isKeyword(pos, "FROM") || isKeyword(pos, "WHERE") || isKeyword(pos, "GROUP") || isKeyword(pos, "ORDER")
        || isKeyword(pos, "LIMIT") || isKeyword(pos, "OFFSET") || isKeyword(pos, "BY") || isKeyword(pos, "JOIN")
        || isKeyword(pos, "ON") || isKeyword(pos, "AS") || isKeyword(pos, "INNER") || isKeyword(pos, "LEFT")
        || isKeyword(pos, "RIGHT") || isKeyword(pos, "FULL") || isKeyword(pos, "CROSS") || isKeyword(pos, "ASC")
        || isKeyword(pos, "DESC");
}

ASTPtr makeUnary(const String & op, const ASTPtr & rhs)
{
    auto node = make_intrusive<ASTExpressionOpsLite>();
    node->op = op;
    node->is_unary = true;
    node->set(node->right, rhs);
    return node;
}

ASTPtr makeBinary(const String & op, const ASTPtr & lhs, const ASTPtr & rhs)
{
    auto node = make_intrusive<ASTExpressionOpsLite>();
    node->op = op;
    node->is_unary = false;
    node->set(node->left, lhs);
    node->set(node->right, rhs);
    return node;
}

bool parseExpressionImpl(IParser::Pos & pos, ASTPtr & node, Expected & expected, int min_precedence);

bool parsePrimary(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);

    if (open.ignore(pos, expected))
    {
        if (!parseExpressionImpl(pos, node, expected, 1))
            return false;
        if (!close.ignore(pos, expected))
            return false;
        return true;
    }

    if (pos->type == TokenType::Number)
    {
        node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::Number, String(pos->begin, pos->end));
        ++pos;
        return true;
    }

    if (pos->type == TokenType::StringLiteral)
    {
        String value(pos->begin, pos->end);
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
            value = value.substr(1, value.size() - 2);
        node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::String, value);
        ++pos;
        return true;
    }

    if (isReservedExpressionBoundary(pos))
        return false;

    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserToken dot_p(TokenType::Dot);

    ASTPtr first;
    if (!identifier_p.parse(pos, first, expected))
        return false;

    auto * first_id = first ? first->as<ASTIdentifier>() : nullptr;
    if (!first_id)
        return false;

    String name = first_id->name();
    while (dot_p.ignore(pos, expected))
    {
        ASTPtr part;
        if (!identifier_p.parse(pos, part, expected))
            return false;
        auto * id = part ? part->as<ASTIdentifier>() : nullptr;
        if (!id)
            return false;
        name += ".";
        name += id->name();
    }

    ParserToken fn_open(TokenType::OpeningRoundBracket);
    ParserToken fn_close(TokenType::ClosingRoundBracket);
    ParserToken comma(TokenType::Comma);

    if (fn_open.ignore(pos, expected))
    {
        auto function = make_intrusive<ASTFunction>();
        function->name = name;

        ASTPtr args_node;
        if (!fn_close.ignore(pos, expected))
        {
            auto args = make_intrusive<ASTExpressionList>();

            ASTPtr arg;
            if (!parseExpressionImpl(pos, arg, expected, 1))
                return false;
            args->children.push_back(arg);

            while (comma.ignore(pos, expected))
            {
                ASTPtr next_arg;
                if (!parseExpressionImpl(pos, next_arg, expected, 1))
                    return false;
                args->children.push_back(next_arg);
            }

            if (!fn_close.ignore(pos, expected))
                return false;
            args_node = args;
        }

        if (args_node)
            function->set(function->arguments, args_node);
        node = function;
        return true;
    }

    node = make_intrusive<ASTIdentifier>(name);
    return true;
}

bool parseUnary(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    if (pos->type == TokenType::Plus)
    {
        ++pos;
        ASTPtr rhs;
        if (!parseUnary(pos, rhs, expected))
            return false;
        node = makeUnary("+", rhs);
        return true;
    }

    if (pos->type == TokenType::Minus)
    {
        ++pos;
        ASTPtr rhs;
        if (!parseUnary(pos, rhs, expected))
            return false;
        node = makeUnary("-", rhs);
        return true;
    }

    if (isKeyword(pos, "NOT"))
    {
        ++pos;
        ASTPtr rhs;
        if (!parseUnary(pos, rhs, expected))
            return false;
        node = makeUnary("NOT", rhs);
        return true;
    }

    return parsePrimary(pos, node, expected);
}

bool readBinaryOperator(IParser::Pos pos, String & op, int & precedence)
{
    precedence = 0;
    op.clear();

    switch (pos->type)
    {
        case TokenType::Asterisk:
            op = "*";
            precedence = 6;
            return true;
        case TokenType::Slash:
            op = "/";
            precedence = 6;
            return true;
        case TokenType::Percent:
            op = "%";
            precedence = 6;
            return true;
        case TokenType::Plus:
            op = "+";
            precedence = 5;
            return true;
        case TokenType::Minus:
            op = "-";
            precedence = 5;
            return true;
        case TokenType::Equals:
            op = "=";
            precedence = 4;
            return true;
        case TokenType::NotEquals:
            op = "!=";
            precedence = 4;
            return true;
        case TokenType::Less:
            op = "<";
            precedence = 4;
            return true;
        case TokenType::LessOrEquals:
            op = "<=";
            precedence = 4;
            return true;
        case TokenType::Greater:
            op = ">";
            precedence = 4;
            return true;
        case TokenType::GreaterOrEquals:
            op = ">=";
            precedence = 4;
            return true;
        default:
            break;
    }

    if (isKeyword(pos, "AND"))
    {
        op = "AND";
        precedence = 3;
        return true;
    }

    if (isKeyword(pos, "OR"))
    {
        op = "OR";
        precedence = 2;
        return true;
    }

    return false;
}

void consumeOperator(IParser::Pos & pos, const String & op)
{
    (void)op;
    ++pos;
}

bool parseExpressionImpl(IParser::Pos & pos, ASTPtr & node, Expected & expected, int min_precedence)
{
    ASTPtr lhs;
    if (!parseUnary(pos, lhs, expected))
        return false;

    while (true)
    {
        String op;
        int precedence = 0;
        if (!readBinaryOperator(pos, op, precedence) || precedence < min_precedence)
            break;

        consumeOperator(pos, op);

        ASTPtr rhs;
        if (!parseExpressionImpl(pos, rhs, expected, precedence + 1))
            return false;

        lhs = makeBinary(op, lhs, rhs);
    }

    node = lhs;
    return true;
}

}

bool ParserExpressionOpsLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return parseExpressionImpl(pos, node, expected, 1);
}

bool ParserExpressionListOpsLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserExpressionOpsLite expr_p;
    ParserToken comma(TokenType::Comma);

    auto list = make_intrusive<ASTExpressionList>();

    ASTPtr first;
    if (!expr_p.parse(pos, first, expected))
        return false;
    list->children.push_back(first);

    while (comma.ignore(pos, expected))
    {
        ASTPtr next;
        if (!expr_p.parse(pos, next, expected))
            return false;
        list->children.push_back(next);
    }

    node = list;
    return true;
}

}
