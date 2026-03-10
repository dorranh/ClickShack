#include "ported_clickhouse/parsers/SelectClausesLiteParsers.h"

#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTWindowDefinitionLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
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

bool parseLimitExpression(IParser::Pos & pos, ASTPtr & expr, String & fallback, Expected & expected)
{
    ParserExpressionOpsLite expression_p;
    if (!expression_p.parse(pos, expr, expected))
        return false;

    if (const auto * literal = expr ? expr->as<ASTLiteral>() : nullptr)
        fallback = literal->value;
    else
        fallback.clear();
    return true;
}

bool parseGroupingSetsList(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserToken comma(TokenType::Comma);
    ParserExpressionListOpsLite expr_list_p;

    if (!open.ignore(pos, expected))
        return false;

    auto groups = make_intrusive<ASTExpressionList>();
    while (true)
    {
        ASTPtr group_exprs;
        if (!open.ignore(pos, expected))
            return false;
        if (!expr_list_p.parse(pos, group_exprs, expected))
            return false;
        if (!close.ignore(pos, expected))
            return false;
        groups->children.push_back(group_exprs);

        if (!comma.ignore(pos, expected))
            break;
    }

    if (!close.ignore(pos, expected))
        return false;

    node = groups;
    return true;
}

bool parseOrderByList(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserExpressionOpsLite expr_p;
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserToken comma(TokenType::Comma);
    ParserKeyword s_with(Keyword::WITH);
    ParserKeyword s_fill(Keyword::FILL);
    ParserKeyword s_from(Keyword::FROM);
    ParserKeyword s_to(Keyword::TO);
    ParserKeyword s_step(Keyword::STEP);
    ParserKeyword s_staleness(Keyword::STALENESS);
    ParserKeyword s_interpolate(Keyword::INTERPOLATE);
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);

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

        // Parser-only acceptance for NULLS FIRST/LAST modifiers.
        if (isKeyword(pos, "NULLS"))
        {
            ++pos;
            if (isKeyword(pos, "FIRST") || isKeyword(pos, "LAST"))
            {
                elem->nulls_order = isKeyword(pos, "FIRST")
                    ? ASTOrderByElementLite::NullsOrder::First
                    : ASTOrderByElementLite::NullsOrder::Last;
                ++pos;
            }
            else
                return false;
        }

        // Parser-only acceptance for COLLATE <name>; payload is intentionally ignored.
        if (isKeyword(pos, "COLLATE"))
        {
            ++pos;
            ASTPtr collate_name;
            if (!identifier_p.parse(pos, collate_name, expected))
                return false;
            if (const auto * id = collate_name ? collate_name->as<ASTIdentifier>() : nullptr)
                elem->collate_name = id->name();
            else
                return false;
        }

        // Parser-only acceptance for WITH FILL [FROM expr] [TO expr] [STEP expr] [STALENESS expr].
        IParser::Pos fill_pos = pos;
        if (s_with.ignore(fill_pos, expected))
        {
            if (!s_fill.ignore(fill_pos, expected))
                return false;
            elem->with_fill = true;

            if (s_from.ignore(fill_pos, expected))
            {
                ASTPtr fill_from;
                if (!expr_p.parse(fill_pos, fill_from, expected))
                    return false;
                elem->set(elem->fill_from, fill_from);
            }
            if (s_to.ignore(fill_pos, expected))
            {
                ASTPtr fill_to;
                if (!expr_p.parse(fill_pos, fill_to, expected))
                    return false;
                elem->set(elem->fill_to, fill_to);
            }
            if (s_step.ignore(fill_pos, expected))
            {
                ASTPtr fill_step;
                if (!expr_p.parse(fill_pos, fill_step, expected))
                    return false;
                elem->set(elem->fill_step, fill_step);
            }
            if (s_staleness.ignore(fill_pos, expected))
            {
                ASTPtr fill_staleness;
                if (!expr_p.parse(fill_pos, fill_staleness, expected))
                    return false;
                elem->set(elem->fill_staleness, fill_staleness);
            }

            pos = fill_pos;
        }

        // Parser-only acceptance for optional INTERPOLATE clause after WITH FILL.
        IParser::Pos interp_pos = pos;
        if (s_interpolate.ignore(interp_pos, expected))
        {
            elem->interpolate = true;
            if (open.ignore(interp_pos, expected))
            {
                if (!close.ignore(interp_pos, expected))
                {
                    auto exprs = make_intrusive<ASTExpressionList>();
                    while (true)
                    {
                        ASTPtr interp_expr;
                        if (!expr_p.parse(interp_pos, interp_expr, expected))
                            return false;
                        exprs->children.push_back(interp_expr);
                        ParserToken comma2(TokenType::Comma);
                        if (!comma2.ignore(interp_pos, expected))
                            break;
                    }
                    if (!close.ignore(interp_pos, expected))
                        return false;
                    elem->set(elem->interpolate_expressions, exprs);
                }
            }
            pos = interp_pos;
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
    ParserKeyword s_prewhere(Keyword::PREWHERE);
    ParserKeyword s_where(Keyword::WHERE);
    ParserKeyword s_group(Keyword::GROUP);
    ParserKeyword s_by(Keyword::BY);
    ParserKeyword s_having(Keyword::HAVING);
    ParserKeyword s_window(Keyword::WINDOW);
    ParserKeyword s_order(Keyword::ORDER);
    ParserKeyword s_limit(Keyword::LIMIT);
    ParserKeyword s_offset(Keyword::OFFSET);
    ParserKeyword s_qualify(Keyword::QUALIFY);
    ParserKeyword s_with(Keyword::WITH);
    ParserKeyword s_ties(Keyword::TIES);
    ParserKeyword s_all(Keyword::ALL);
    ParserKeyword s_rollup(Keyword::ROLLUP);
    ParserKeyword s_cube(Keyword::CUBE);
    ParserKeyword s_totals(Keyword::TOTALS);
    ParserKeyword s_fetch(Keyword::FETCH);
    ParserKeyword s_first(Keyword::FIRST);
    ParserKeyword s_next(Keyword::NEXT);
    ParserKeyword s_row(Keyword::ROW);
    ParserKeyword s_rows(Keyword::ROWS);
    ParserKeyword s_only(Keyword::ONLY);

    if (s_prewhere.ignore(pos, expected))
    {
        ParserExpressionOpsLite expr_p;
        if (!expr_p.parse(pos, result.prewhere_expression, expected))
            return false;
    }

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
        if (s_all.ignore(pos, expected))
        {
            result.group_by_all = true;
        }
        else if (isKeyword(pos, "GROUPING"))
        {
            ++pos;
            if (!isKeyword(pos, "SETS"))
                return false;
            ++pos;
            if (!parseGroupingSetsList(pos, result.grouping_sets_expressions, expected))
                return false;
        }
        else
        {
            ParserExpressionListOpsLite expr_list_p;
            if (!expr_list_p.parse(pos, result.group_by_expressions, expected))
                return false;
        }

        while (true)
        {
            IParser::Pos mod_pos = pos;
            if (!s_with.ignore(mod_pos, expected))
                break;

            if (s_rollup.ignore(mod_pos, expected))
                result.group_by_with_rollup = true;
            else if (s_cube.ignore(mod_pos, expected))
                result.group_by_with_cube = true;
            else if (s_totals.ignore(mod_pos, expected))
                result.group_by_with_totals = true;
            else
                return false;

            pos = mod_pos;
        }
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
        IParser::Pos all_pos = pos;
        if (s_all.ignore(all_pos, expected))
        {
            result.order_by_all = true;
            pos = all_pos;
        }
        else
        {
            if (!parseOrderByList(pos, result.order_by_list, expected))
                return false;
        }
    }

    if (s_limit.ignore(pos, expected))
    {
        ParserToken comma(TokenType::Comma);
        ASTPtr limit_expr;
        String limit_value;
        if (!parseLimitExpression(pos, limit_expr, limit_value, expected))
            return false;

        // LIMIT count OFFSET offset BY ...
        IParser::Pos offset_by_pos = pos;
        if (s_offset.ignore(offset_by_pos, expected))
        {
            String offset_value;
            ASTPtr offset_expr;
            if (!parseLimitExpression(offset_by_pos, offset_expr, offset_value, expected))
                return false;

            if (s_by.ignore(offset_by_pos, expected))
            {
                auto limit_by = make_intrusive<ASTLimitByLite>();
                limit_by->limit = limit_value;
                limit_by->set(limit_by->limit_expression, limit_expr);
                limit_by->offset_present = true;
                limit_by->offset = offset_value;
                limit_by->set(limit_by->offset_expression, offset_expr);

                if (s_all.ignore(offset_by_pos, expected))
                {
                    limit_by->by_all = true;
                }
                else
                {
                    ASTPtr by_exprs;
                    ParserExpressionListOpsLite expr_list_p;
                    if (!expr_list_p.parse(offset_by_pos, by_exprs, expected))
                        return false;
                    limit_by->set(limit_by->by_expressions, by_exprs);
                }
                result.limit_by = limit_by;
                pos = offset_by_pos;
                return true;
            }
        }

        // LIMIT offset,count BY ...
        IParser::Pos comma_by_pos = pos;
        if (comma.ignore(comma_by_pos, expected))
        {
            String count_value;
            ASTPtr count_expr;
            if (!parseLimitExpression(comma_by_pos, count_expr, count_value, expected))
                return false;

            if (s_by.ignore(comma_by_pos, expected))
            {
                auto limit_by = make_intrusive<ASTLimitByLite>();
                limit_by->offset_present = true;
                limit_by->offset = limit_value;
                limit_by->set(limit_by->offset_expression, limit_expr);
                limit_by->limit = count_value;
                limit_by->set(limit_by->limit_expression, count_expr);

                if (s_all.ignore(comma_by_pos, expected))
                {
                    limit_by->by_all = true;
                }
                else
                {
                    ASTPtr by_exprs;
                    ParserExpressionListOpsLite expr_list_p;
                    if (!expr_list_p.parse(comma_by_pos, by_exprs, expected))
                        return false;
                    limit_by->set(limit_by->by_expressions, by_exprs);
                }
                result.limit_by = limit_by;
                pos = comma_by_pos;
                return true;
            }
        }

        IParser::Pos by_pos = pos;
        if (s_by.ignore(by_pos, expected))
        {
            auto limit_by = make_intrusive<ASTLimitByLite>();
            limit_by->limit = limit_value;
            limit_by->set(limit_by->limit_expression, limit_expr);

            if (s_all.ignore(by_pos, expected))
            {
                limit_by->by_all = true;
            }
            else
            {
                ASTPtr by_exprs;
                ParserExpressionListOpsLite expr_list_p;
                if (!expr_list_p.parse(by_pos, by_exprs, expected))
                    return false;
                limit_by->set(limit_by->by_expressions, by_exprs);
            }
            result.limit_by = limit_by;
            pos = by_pos;
            return true;
        }

        auto limit = make_intrusive<ASTLimitLite>();
        limit->limit = limit_value;
        limit->set(limit->limit_expression, limit_expr);

        IParser::Pos comma_pos = pos;
        if (comma.ignore(comma_pos, expected))
        {
            String second_value;
            ASTPtr second_expr;
            if (!parseLimitExpression(comma_pos, second_expr, second_value, expected))
                return false;
            limit->offset_present = true;
            limit->offset = limit_value;
            limit->set(limit->offset_expression, limit_expr);
            limit->limit = second_value;
            limit->set(limit->limit_expression, second_expr);
            pos = comma_pos;
        }
        else
        {
            IParser::Pos offset_pos = pos;
            if (s_offset.ignore(offset_pos, expected))
            {
                String offset_value;
                ASTPtr offset_expr;
                if (!parseLimitExpression(offset_pos, offset_expr, offset_value, expected))
                    return false;
                limit->offset_present = true;
                limit->offset = offset_value;
                limit->set(limit->offset_expression, offset_expr);
                pos = offset_pos;
            }
        }

        IParser::Pos ties_pos = pos;
        if (s_with.ignore(ties_pos, expected))
        {
            if (!s_ties.ignore(ties_pos, expected))
                return false;
            limit->with_ties = true;
            pos = ties_pos;
        }

        result.limit = limit;
    }
    else
    {
        // OFFSET <n> [ROW|ROWS] FETCH {FIRST|NEXT} <n> {ROW|ROWS} {ONLY|WITH TIES}
        IParser::Pos fetch_pos = pos;
        auto limit = make_intrusive<ASTLimitLite>();
        bool saw_fetch = false;

        if (s_offset.ignore(fetch_pos, expected))
        {
            String offset_value;
            ASTPtr offset_expr;
            if (!parseLimitExpression(fetch_pos, offset_expr, offset_value, expected))
                return false;
            limit->offset_present = true;
            limit->offset = offset_value;
            limit->set(limit->offset_expression, offset_expr);
            s_row.ignore(fetch_pos, expected) || s_rows.ignore(fetch_pos, expected);
        }

        if (s_fetch.ignore(fetch_pos, expected))
        {
            if (!s_first.ignore(fetch_pos, expected))
                s_next.ignore(fetch_pos, expected);

            String fetch_count;
            ASTPtr fetch_expr;
            if (!parseLimitExpression(fetch_pos, fetch_expr, fetch_count, expected))
                return false;
            limit->limit = fetch_count;
            limit->set(limit->limit_expression, fetch_expr);
            saw_fetch = true;

            if (!s_row.ignore(fetch_pos, expected))
                s_rows.ignore(fetch_pos, expected);

            if (s_only.ignore(fetch_pos, expected))
            {
                // no-op
            }
            else if (s_with.ignore(fetch_pos, expected))
            {
                if (!s_ties.ignore(fetch_pos, expected))
                    return false;
                limit->with_ties = true;
            }
            else
            {
                return false;
            }
        }

        if (saw_fetch)
        {
            pos = fetch_pos;
            result.limit = limit;
        }
    }

    return true;
}

}
