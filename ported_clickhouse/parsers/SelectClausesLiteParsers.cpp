#include "ported_clickhouse/parsers/SelectClausesLiteParsers.h"

#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTWindowDefinitionLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"
#include "ported_clickhouse/parsers/ExpressionOpsLiteParsers.h"

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

bool parseNumberToken(IParser::Pos & pos, String & out)
{
    if (pos->type != TokenType::Number)
        return false;
    out.assign(pos->begin, pos->end);
    ++pos;
    return true;
}

bool parseOrderByList(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserExpressionOpsLite expr_p;
    ParserToken comma(TokenType::Comma);

    auto list = make_intrusive<ASTOrderByListLite>();

    while (true)
    {
        ASTPtr expr;
        if (!expr_p.parse(pos, expr, expected))
            return false;

        auto elem = make_intrusive<ASTOrderByElementLite>();
        elem->set(elem->expression, expr);

        if (isKeyword(pos, "ASC"))
        {
            elem->direction = ASTOrderByElementLite::Direction::Asc;
            ++pos;
        }
        else if (isKeyword(pos, "DESC"))
        {
            elem->direction = ASTOrderByElementLite::Direction::Desc;
            ++pos;
        }

        list->children.push_back(elem);

        if (!comma.ignore(pos, expected))
            break;
    }

    node = list;
    return true;
}

bool parseWindowBody(IParser::Pos & pos, String & body, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    if (!open.ignore(pos, expected))
        return false;

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
                body.assign(body_begin, body_end);
                return true;
            }
        }
        ++pos;
    }

    return false;
}

bool parseWindowList(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserKeyword s_as(Keyword::AS);
    ParserToken comma(TokenType::Comma);

    auto list = make_intrusive<ASTWindowListLite>();

    while (true)
    {
        ASTPtr name_ast;
        if (!identifier_p.parse(pos, name_ast, expected))
            return false;
        auto * name_id = name_ast ? name_ast->as<ASTIdentifier>() : nullptr;
        if (!name_id)
            return false;

        if (!s_as.ignore(pos, expected))
            return false;

        String body;
        if (!parseWindowBody(pos, body, expected))
            return false;

        auto window_def = make_intrusive<ASTWindowDefinitionLite>();
        window_def->name = name_id->name();
        window_def->body = body;
        list->children.push_back(window_def);

        if (!comma.ignore(pos, expected))
            break;
    }

    node = list;
    return true;
}

}

bool parseSelectClausesLite(IParser::Pos & pos, SelectClausesLiteResult & result, Expected & expected)
{
    ParserKeyword s_where(Keyword::WHERE);
    ParserKeyword s_group(Keyword::GROUP);
    ParserKeyword s_by(Keyword::BY);
    ParserKeyword s_having(Keyword::HAVING);
    ParserKeyword s_window(Keyword::WINDOW);
    ParserKeyword s_order(Keyword::ORDER);
    ParserKeyword s_limit(Keyword::LIMIT);
    ParserKeyword s_offset(Keyword::OFFSET);
    ParserKeyword s_qualify(Keyword::QUALIFY);

    if (s_where.ignore(pos, expected))
    {
        ParserExpressionOpsLite expr_p;
        if (!expr_p.parse(pos, result.where_expression, expected))
            return false;
    }

    if (s_group.ignore(pos, expected))
    {
        if (!s_by.ignore(pos, expected))
            return false;
        ParserExpressionListOpsLite expr_list_p;
        if (!expr_list_p.parse(pos, result.group_by_expressions, expected))
            return false;
    }

    if (s_having.ignore(pos, expected))
    {
        ParserExpressionOpsLite expr_p;
        if (!expr_p.parse(pos, result.having_expression, expected))
            return false;
    }

    if (s_window.ignore(pos, expected))
    {
        if (!parseWindowList(pos, result.window_list, expected))
            return false;
    }

    if (s_qualify.ignore(pos, expected))
    {
        ParserExpressionOpsLite expr_p;
        if (!expr_p.parse(pos, result.qualify_expression, expected))
            return false;
    }

    if (s_order.ignore(pos, expected))
    {
        if (!s_by.ignore(pos, expected))
            return false;
        if (!parseOrderByList(pos, result.order_by_list, expected))
            return false;
    }

    if (s_limit.ignore(pos, expected))
    {
        String limit_value;
        if (!parseNumberToken(pos, limit_value))
            return false;

        IParser::Pos by_pos = pos;
        if (s_by.ignore(by_pos, expected))
        {
            ASTPtr by_exprs;
            ParserExpressionListOpsLite expr_list_p;
            if (!expr_list_p.parse(by_pos, by_exprs, expected))
                return false;

            auto limit_by = make_intrusive<ASTLimitByLite>();
            limit_by->limit = limit_value;
            limit_by->set(limit_by->by_expressions, by_exprs);
            result.limit_by = limit_by;
            pos = by_pos;
            return true;
        }

        auto limit = make_intrusive<ASTLimitLite>();
        limit->limit = limit_value;

        IParser::Pos offset_pos = pos;
        if (s_offset.ignore(offset_pos, expected))
        {
            String offset_value;
            if (!parseNumberToken(offset_pos, offset_value))
                return false;
            limit->offset_present = true;
            limit->offset = offset_value;
            pos = offset_pos;
        }

        result.limit = limit;
    }

    return true;
}

}
