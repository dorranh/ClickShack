#include "ported_clickhouse/parsers/SelectClausesLiteParsers.h"

#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
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

}

bool parseSelectClausesLite(IParser::Pos & pos, SelectClausesLiteResult & result, Expected & expected)
{
    ParserKeyword s_where(Keyword::WHERE);
    ParserKeyword s_group(Keyword::GROUP);
    ParserKeyword s_by(Keyword::BY);
    ParserKeyword s_order(Keyword::ORDER);
    ParserKeyword s_limit(Keyword::LIMIT);
    ParserKeyword s_offset(Keyword::OFFSET);

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

    if (s_order.ignore(pos, expected))
    {
        if (!s_by.ignore(pos, expected))
            return false;
        if (!parseOrderByList(pos, result.order_by_list, expected))
            return false;
    }

    if (s_limit.ignore(pos, expected))
    {
        auto limit = make_intrusive<ASTLimitLite>();
        if (!parseNumberToken(pos, limit->limit))
            return false;

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
