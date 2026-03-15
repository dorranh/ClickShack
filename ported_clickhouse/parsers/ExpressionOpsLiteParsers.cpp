#include "ported_clickhouse/parsers/ExpressionOpsLiteParsers.h"

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTExpressionOpsLite.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"

#include <strings.h>

namespace DB
{

bool parseNestedSelectRichQuery(IParser::Pos & pos, ASTPtr & node, Expected & expected);

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
        || isKeyword(pos, "DESC") || isKeyword(pos, "HAVING") || isKeyword(pos, "UNION") || isKeyword(pos, "ALL")
        || isKeyword(pos, "WITH") || isKeyword(pos, "DISTINCT") || isKeyword(pos, "WHEN") || isKeyword(pos, "THEN")
        || isKeyword(pos, "ELSE") || isKeyword(pos, "END") || isKeyword(pos, "WINDOW") || isKeyword(pos, "QUALIFY")
        || isKeyword(pos, "USING") || isKeyword(pos, "PREWHERE") || isKeyword(pos, "ARRAY")
        || isKeyword(pos, "SAMPLE") || isKeyword(pos, "FINAL") || isKeyword(pos, "FETCH")
        || isKeyword(pos, "ROW") || isKeyword(pos, "ROWS") || isKeyword(pos, "ONLY")
        || isKeyword(pos, "INTERSECT") || isKeyword(pos, "EXCEPT")
        || isKeyword(pos, "SETTINGS") || isKeyword(pos, "FORMAT")
        || isKeyword(pos, "ROLLUP") || isKeyword(pos, "CUBE") || isKeyword(pos, "TOTALS")
        || isKeyword(pos, "FILL") || isKeyword(pos, "TO") || isKeyword(pos, "STEP")
        || isKeyword(pos, "STALENESS") || isKeyword(pos, "INTERPOLATE");
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

ASTPtr makeFunctionNode(const String & name, const ASTs & args)
{
    auto fn = make_intrusive<ASTFunction>();
    fn->name = name;

    auto expr_list = make_intrusive<ASTExpressionList>();
    for (const auto & arg : args)
        expr_list->children.push_back(arg);

    fn->set(fn->arguments, expr_list);
    return fn;
}

bool parseExpressionImpl(IParser::Pos & pos, ASTPtr & node, Expected & expected, int min_precedence);
bool parseIdentifierPath(IParser::Pos & pos, String & out, Expected & expected);

bool parseOverClause(IParser::Pos & pos, ASTPtr & lhs, Expected & expected)
{
    if (!isKeyword(pos, "OVER"))
        return false;
    ++pos;

    ASTPtr window_spec;

    ParserToken open(TokenType::OpeningRoundBracket);
    if (open.ignore(pos, expected))
    {
        const char * body_begin = pos->begin;
        const char * body_end = body_begin;
        int depth = 1;

        while (!pos->isEnd())
        {
            if (pos->type == TokenType::OpeningRoundBracket)
                ++depth;
            else if (pos->type == TokenType::ClosingRoundBracket)
            {
                --depth;
                if (depth == 0)
                {
                    body_end = pos->begin;
                    ++pos;
                    window_spec = make_intrusive<ASTLiteral>(ASTLiteral::Kind::String, String(body_begin, body_end));
                    break;
                }
            }
            ++pos;
        }

        if (depth != 0)
            return false;
    }
    else
    {
        String window_name;
        if (!parseIdentifierPath(pos, window_name, expected))
            return false;
        window_spec = make_intrusive<ASTIdentifier>(window_name);
    }

    auto over_fn = make_intrusive<ASTFunction>();
    over_fn->name = "over";
    auto args = make_intrusive<ASTExpressionList>();
    args->children.push_back(lhs);
    args->children.push_back(window_spec);
    over_fn->set(over_fn->arguments, args);
    lhs = over_fn;
    return true;
}

bool parseIdentifierPath(IParser::Pos & pos, String & out, Expected & expected)
{
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserToken dot_p(TokenType::Dot);

    ASTPtr first;
    if (!identifier_p.parse(pos, first, expected))
        return false;

    auto * first_id = first ? first->as<ASTIdentifier>() : nullptr;
    if (!first_id)
        return false;

    out = first_id->name();

    while (dot_p.ignore(pos, expected))
    {
        if (pos->type == TokenType::Asterisk)
        {
            out += ".*";
            ++pos;
            return true;
        }

        ASTPtr part;
        if (!identifier_p.parse(pos, part, expected))
            return false;

        auto * id = part ? part->as<ASTIdentifier>() : nullptr;
        if (!id)
            return false;

        out += ".";
        out += id->name();
    }

    return true;
}

bool parseCastTypeUntilClosingParen(IParser::Pos & pos, String & type_name)
{
    if (pos->isEnd())
        return false;

    const char * begin = pos->begin;
    const char * end = begin;
    int depth = 0;

    while (!pos->isEnd())
    {
        if (pos->type == TokenType::OpeningRoundBracket)
            ++depth;
        else if (pos->type == TokenType::ClosingRoundBracket)
        {
            if (depth == 0)
                break;
            --depth;
        }

        end = pos->end;
        ++pos;
    }

    if (begin == end)
        return false;

    type_name.assign(begin, end);
    return true;
}

bool isProjectionAliasStop(TokenIterator pos)
{
    return pos->type == TokenType::Comma || isKeyword(pos, "FROM") || isKeyword(pos, "PREWHERE") || isKeyword(pos, "WHERE")
        || isKeyword(pos, "GROUP") || isKeyword(pos, "HAVING") || isKeyword(pos, "WINDOW") || isKeyword(pos, "QUALIFY")
        || isKeyword(pos, "ORDER") || isKeyword(pos, "LIMIT") || isKeyword(pos, "OFFSET") || isKeyword(pos, "UNION")
        || isKeyword(pos, "INTERSECT") || isKeyword(pos, "EXCEPT") || isKeyword(pos, "SETTINGS") || isKeyword(pos, "FORMAT");
}

ASTPtr wrapAlias(const ASTPtr & expr, const String & alias)
{
    auto fn = make_intrusive<ASTFunction>();
    fn->name = "alias";
    auto args = make_intrusive<ASTExpressionList>();
    args->children.push_back(expr);
    args->children.push_back(make_intrusive<ASTIdentifier>(alias));
    fn->set(fn->arguments, args);
    return fn;
}

bool parseExpressionListInParens(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserToken comma(TokenType::Comma);

    if (!open.ignore(pos, expected))
        return false;

    auto list = make_intrusive<ASTExpressionList>();

    ASTPtr first;
    if (!parseExpressionImpl(pos, first, expected, 1))
        return false;
    list->children.push_back(first);

    while (comma.ignore(pos, expected))
    {
        ASTPtr next;
        if (!parseExpressionImpl(pos, next, expected, 1))
            return false;
        list->children.push_back(next);
    }

    if (!close.ignore(pos, expected))
        return false;

    node = list;
    return true;
}

bool parseSubqueryInParens(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    IParser::Pos begin = pos;
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    if (!open.ignore(pos, expected))
        return false;
    if (!(isKeyword(pos, "SELECT") || isKeyword(pos, "WITH")))
    {
        pos = begin;
        return false;
    }

    ASTPtr subquery;
    if (!parseNestedSelectRichQuery(pos, subquery, expected))
    {
        pos = begin;
        return false;
    }
    if (!close.ignore(pos, expected))
    {
        pos = begin;
        return false;
    }

    ASTs args;
    args.push_back(subquery);
    node = makeFunctionNode("subquery", args);
    return true;
}

bool parsePrimary(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserToken open_square(TokenType::OpeningSquareBracket);
    ParserToken close_square(TokenType::ClosingSquareBracket);
    ParserToken comma(TokenType::Comma);

    if (isKeyword(pos, "CASE"))
    {
        ++pos;

        ASTs args;
        bool simple_case = false;

        if (!isKeyword(pos, "WHEN"))
        {
            ASTPtr case_expr;
            if (!parseExpressionImpl(pos, case_expr, expected, 1))
                return false;
            args.push_back(case_expr);
            simple_case = true;
        }

        while (isKeyword(pos, "WHEN"))
        {
            ++pos;

            ASTPtr when_expr;
            if (!parseExpressionImpl(pos, when_expr, expected, 1))
                return false;

            if (!isKeyword(pos, "THEN"))
                return false;
            ++pos;

            ASTPtr then_expr;
            if (!parseExpressionImpl(pos, then_expr, expected, 1))
                return false;

            args.push_back(when_expr);
            args.push_back(then_expr);
        }

        if (isKeyword(pos, "ELSE"))
        {
            ++pos;
            ASTPtr else_expr;
            if (!parseExpressionImpl(pos, else_expr, expected, 1))
                return false;
            args.push_back(else_expr);
        }

        if (!isKeyword(pos, "END"))
            return false;
        ++pos;

        node = makeFunctionNode(simple_case ? "caseWithExpr" : "case", args);
        return true;
    }

    if (isKeyword(pos, "EXISTS"))
    {
        ++pos;
        ASTPtr subquery_expr;
        if (!parseSubqueryInParens(pos, subquery_expr, expected))
            return false;
        ASTs args;
        args.push_back(subquery_expr);
        node = makeFunctionNode("exists", args);
        return true;
    }

    if (open.ignore(pos, expected))
    {
        if (isKeyword(pos, "SELECT") || isKeyword(pos, "WITH"))
        {
            ASTPtr subquery;
            if (!parseNestedSelectRichQuery(pos, subquery, expected))
                return false;
            if (!close.ignore(pos, expected))
                return false;
            ASTs args;
            args.push_back(subquery);
            node = makeFunctionNode("subquery", args);
            return true;
        }

        ASTPtr first;
        if (!parseExpressionImpl(pos, first, expected, 1))
            return false;

        if (comma.ignore(pos, expected))
        {
            ASTs tuple_args;
            tuple_args.push_back(first);

            while (true)
            {
                ASTPtr elem;
                if (!parseExpressionImpl(pos, elem, expected, 1))
                    return false;
                tuple_args.push_back(elem);

                if (!comma.ignore(pos, expected))
                    break;
            }

            if (!close.ignore(pos, expected))
                return false;

            node = makeFunctionNode("tuple", tuple_args);
            return true;
        }

        if (!close.ignore(pos, expected))
            return false;
        node = first;
        return true;
    }

    if (open_square.ignore(pos, expected))
    {
        ASTs args;

        if (!close_square.ignore(pos, expected))
        {
            while (true)
            {
                ASTPtr elem;
                if (!parseExpressionImpl(pos, elem, expected, 1))
                    return false;
                args.push_back(elem);

                if (!comma.ignore(pos, expected))
                    break;
            }

            if (!close_square.ignore(pos, expected))
                return false;
        }

        node = makeFunctionNode("array", args);
        return true;
    }

    if (pos->type == TokenType::Number)
    {
        IParser::Pos look = pos;
        ++look;
        if (!look->isEnd() && look->type == TokenType::Dot)
        {
            IParser::Pos frac = look;
            ++frac;
            if (!frac->isEnd() && frac->type == TokenType::Number)
            {
                String value(pos->begin, frac->end);
                node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::Number, value);
                ++frac;
                pos = frac;
                return true;
            }
        }

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

    if (isKeyword(pos, "NULL"))
    {
        node = make_intrusive<ASTLiteral>(ASTLiteral::Kind::Null, String{});
        ++pos;
        return true;
    }

    if (pos->type == TokenType::Asterisk)
    {
        node = make_intrusive<ASTIdentifier>("*");
        ++pos;
        return true;
    }

    if (isReservedExpressionBoundary(pos))
        return false;

    String name;
    if (!parseIdentifierPath(pos, name, expected))
        return false;

    ParserToken fn_open(TokenType::OpeningRoundBracket);
    ParserToken fn_close(TokenType::ClosingRoundBracket);

    if (fn_open.ignore(pos, expected))
    {
        // CAST(expr AS type) special-case.
        if (0 == strcasecmp(name.c_str(), "CAST"))
        {
            ASTPtr expr;
            if (!parseExpressionImpl(pos, expr, expected, 1))
                return false;

            if (!isKeyword(pos, "AS"))
                return false;
            ++pos;

            String type_name;
            if (!parseCastTypeUntilClosingParen(pos, type_name))
                return false;

            if (!fn_close.ignore(pos, expected))
                return false;

            ASTs args;
            args.push_back(expr);
            args.push_back(make_intrusive<ASTIdentifier>(type_name));
            node = makeFunctionNode("CAST", args);
            return true;
        }

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
        case TokenType::DoubleColon:
            op = "::";
            precedence = 7;
            return true;
        case TokenType::Concatenation:
            op = "||";
            precedence = 5;
            return true;
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
        case TokenType::Spaceship:
            op = "<=>";
            precedence = 4;
            return true;
        case TokenType::Caret:
            op = "^";
            precedence = 5;
            return true;
        case TokenType::PipeMark:
            op = "|";
            precedence = 5;
            return true;
        case TokenType::Arrow:
            op = "->";
            precedence = 1;
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

bool parseSpecialComparison(IParser::Pos & pos, ASTPtr & lhs, Expected & expected, int min_precedence)
{
    if (min_precedence > 4)
        return false;

    // IS [NOT] NULL
    {
        IParser::Pos look = pos;
        if (isKeyword(look, "IS"))
        {
            ++look;
            bool is_not = false;
            if (isKeyword(look, "NOT"))
            {
                is_not = true;
                ++look;
            }

            if (isKeyword(look, "DISTINCT"))
            {
                ++look;
                if (!isKeyword(look, "FROM"))
                    return false;
                ++look;

                ASTPtr rhs;
                if (!parseExpressionImpl(look, rhs, expected, 4))
                    return false;

                ASTs args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = makeFunctionNode(is_not ? "isNotDistinctFrom" : "isDistinctFrom", args);
                pos = look;
                return true;
            }

            if (!isKeyword(look, "NULL"))
                return false;
            ++look;

            ASTs args;
            args.push_back(lhs);
            lhs = makeFunctionNode(is_not ? "isNotNull" : "isNull", args);
            pos = look;
            return true;
        }
    }

    // [NOT] (IN|BETWEEN|LIKE|ILIKE)
    {
        IParser::Pos look = pos;
        bool not_prefix = false;
        if (isKeyword(look, "NOT"))
        {
            not_prefix = true;
            ++look;
        }

        if (isKeyword(look, "IN"))
        {
            ++look;
            ASTPtr list;
            if (!parseSubqueryInParens(look, list, expected))
            {
                if (!parseExpressionListInParens(look, list, expected))
                    return false;
            }

            ASTs args;
            args.push_back(lhs);
            args.push_back(list);
            lhs = makeFunctionNode(not_prefix ? "notIn" : "in", args);
            pos = look;
            return true;
        }

        if (isKeyword(look, "BETWEEN"))
        {
            ++look;
            ASTPtr low;
            if (!parseExpressionImpl(look, low, expected, 4))
                return false;
            if (!isKeyword(look, "AND"))
                return false;
            ++look;
            ASTPtr high;
            if (!parseExpressionImpl(look, high, expected, 4))
                return false;

            ASTs args;
            args.push_back(lhs);
            args.push_back(low);
            args.push_back(high);
            lhs = makeFunctionNode(not_prefix ? "notBetween" : "between", args);
            pos = look;
            return true;
        }

        if (isKeyword(look, "LIKE") || isKeyword(look, "ILIKE"))
        {
            bool ilike = isKeyword(look, "ILIKE");
            ++look;
            ASTPtr pattern;
            if (!parseExpressionImpl(look, pattern, expected, 4))
                return false;

            ASTPtr escape_expr;
            if (isKeyword(look, "ESCAPE"))
            {
                ++look;
                if (!parseExpressionImpl(look, escape_expr, expected, 4))
                    return false;
            }

            ASTs args;
            args.push_back(lhs);
            args.push_back(pattern);
            if (escape_expr)
                args.push_back(escape_expr);

            if (not_prefix)
                lhs = makeFunctionNode(ilike ? (escape_expr ? "notILikeEscape" : "notILike") : (escape_expr ? "notLikeEscape" : "notLike"), args);
            else
                lhs = makeFunctionNode(ilike ? (escape_expr ? "iLikeEscape" : "iLike") : (escape_expr ? "likeEscape" : "like"), args);

            pos = look;
            return true;
        }
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
        if (parseOverClause(pos, lhs, expected))
            continue;

        if (parseSpecialComparison(pos, lhs, expected, min_precedence))
            continue;

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

bool ParserProjectionListOpsLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserExpressionOpsLite expr_p;
    ParserIdentifier alias_p(/*allow_query_parameter*/ true);
    ParserKeyword s_as(Keyword::AS);
    ParserToken comma(TokenType::Comma);

    auto list = make_intrusive<ASTExpressionList>();

    while (true)
    {
        ASTPtr expr;
        if (!expr_p.parse(pos, expr, expected))
            return false;

        ASTPtr aliased = expr;
        if (s_as.ignore(pos, expected))
        {
            ASTPtr alias_ast;
            if (!alias_p.parse(pos, alias_ast, expected))
                return false;
            auto * alias_id = alias_ast ? alias_ast->as<ASTIdentifier>() : nullptr;
            if (!alias_id)
                return false;
            aliased = wrapAlias(expr, alias_id->name());
        }
        else if (!isProjectionAliasStop(pos))
        {
            IParser::Pos alias_pos = pos;
            ASTPtr alias_ast;
            if (alias_p.parse(alias_pos, alias_ast, expected))
            {
                auto * alias_id = alias_ast ? alias_ast->as<ASTIdentifier>() : nullptr;
                if (!alias_id)
                    return false;
                aliased = wrapAlias(expr, alias_id->name());
                pos = alias_pos;
            }
        }

        list->children.push_back(aliased);

        if (!comma.ignore(pos, expected))
            break;
    }

    node = list;
    return true;
}

}
