#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"

#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/ASTSelectSetLite.h"
#include "ported_clickhouse/parsers/CommonParsers.h"
#include "ported_clickhouse/parsers/ExpressionElementParsers.h"
#include "ported_clickhouse/parsers/ExpressionOpsLiteParsers.h"
#include "ported_clickhouse/parsers/SelectClausesLiteParsers.h"
#include "ported_clickhouse/parsers/TableSourcesLiteParsers.h"

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

bool isWithAliasStop(TokenIterator pos)
{
    return isKeyword(pos, "SELECT") || isKeyword(pos, "FROM") || isKeyword(pos, "WHERE") || isKeyword(pos, "GROUP")
        || isKeyword(pos, "HAVING") || isKeyword(pos, "ORDER") || isKeyword(pos, "LIMIT") || isKeyword(pos, "OFFSET")
        || isKeyword(pos, "UNION") || isKeyword(pos, "BY") || pos->type == TokenType::Comma;
}

bool parseSelectRichCore(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_with(Keyword::WITH);
    ParserKeyword s_select(Keyword::SELECT);
    ParserKeyword s_distinct(Keyword::DISTINCT);
    ParserKeyword s_from(Keyword::FROM);
    ParserKeyword s_final(Keyword::FINAL);
    ParserKeyword s_sample(Keyword::SAMPLE);
    ParserKeyword s_offset(Keyword::OFFSET);
    ParserKeyword s_array(Keyword::ARRAY);
    ParserKeyword s_join(Keyword::JOIN);
    ParserKeyword s_left(Keyword::LEFT);
    ParserKeyword s_as(Keyword::AS);

    ASTPtr with_expressions;
    if (s_with.ignore(pos, expected))
    {
        ParserExpressionOpsLite expr_p;
        ParserIdentifier alias_p(/*allow_query_parameter*/ true);
        ParserToken comma(TokenType::Comma);

        auto list = make_intrusive<ASTExpressionList>();
        while (true)
        {
            ASTPtr expr;
            if (!expr_p.parse(pos, expr, expected))
                return false;

            // Accept optional WITH alias syntax and ignore alias payload in parser-only bridge.
            if (s_as.ignore(pos, expected))
            {
                ASTPtr ignore_alias;
                if (!alias_p.parse(pos, ignore_alias, expected))
                    return false;
            }
            else if (!isWithAliasStop(pos))
            {
                IParser::Pos alias_pos = pos;
                ASTPtr bare_alias;
                if (alias_p.parse(alias_pos, bare_alias, expected))
                    pos = alias_pos;
            }

            list->children.push_back(expr);
            if (!comma.ignore(pos, expected))
                break;
        }
        with_expressions = list;
    }

    if (!s_select.ignore(pos, expected))
        return false;

    bool distinct = s_distinct.ignore(pos, expected);

    ParserExpressionListOpsLite projection_p;
    ASTPtr projections;
    if (!projection_p.parse(pos, projections, expected))
        return false;

    ASTPtr source;
    bool from_final = false;
    ASTPtr sample_size;
    ASTPtr sample_offset;
    ASTPtr array_join_expressions;
    bool array_join_is_left = false;
    if (s_from.ignore(pos, expected))
    {
        ParserTableSourceLite source_p;
        if (!source_p.parse(pos, source, expected))
            return false;

        from_final = s_final.ignore(pos, expected);

        if (s_sample.ignore(pos, expected))
        {
            ParserExpressionOpsLite expr_p;
            if (!expr_p.parse(pos, sample_size, expected))
                return false;

            IParser::Pos offset_pos = pos;
            if (s_offset.ignore(offset_pos, expected))
            {
                ASTPtr offset_expr;
                if (!expr_p.parse(offset_pos, offset_expr, expected))
                    return false;
                sample_offset = offset_expr;
                pos = offset_pos;
            }
        }

        IParser::Pos array_pos = pos;
        bool is_left = false;
        if (s_left.ignore(array_pos, expected))
            is_left = true;
        if (s_array.ignore(array_pos, expected))
        {
            if (!s_join.ignore(array_pos, expected))
                return false;
            ParserExpressionListOpsLite expr_list_p;
            ASTPtr exprs;
            if (!expr_list_p.parse(array_pos, exprs, expected))
                return false;
            array_join_expressions = exprs;
            array_join_is_left = is_left;
            pos = array_pos;
        }
    }

    SelectClausesLiteResult clauses;
    if (!parseSelectClausesLite(pos, clauses, expected))
        return false;

    auto query = make_intrusive<ASTSelectRichQuery>();
    query->distinct = distinct;
    if (with_expressions)
        query->set(query->with_expressions, with_expressions);
    query->set(query->expressions, projections);
    if (source)
        query->set(query->from_source, source);
    query->from_final = from_final;
    if (sample_size)
        query->set(query->sample_size, sample_size);
    if (sample_offset)
        query->set(query->sample_offset, sample_offset);
    if (array_join_expressions)
        query->set(query->array_join_expressions, array_join_expressions);
    query->array_join_is_left = array_join_is_left;
    if (clauses.prewhere_expression)
        query->set(query->prewhere_expression, clauses.prewhere_expression);
    if (clauses.where_expression)
        query->set(query->where_expression, clauses.where_expression);
    if (clauses.group_by_expressions)
        query->set(query->group_by_expressions, clauses.group_by_expressions);
    if (clauses.having_expression)
        query->set(query->having_expression, clauses.having_expression);
    if (clauses.window_list)
        query->set(query->window_list, clauses.window_list);
    if (clauses.qualify_expression)
        query->set(query->qualify_expression, clauses.qualify_expression);
    if (clauses.order_by_list)
        query->set(query->order_by_list, clauses.order_by_list);
    if (clauses.limit)
        query->set(query->limit, clauses.limit);
    if (clauses.limit_by)
        query->set(query->limit_by, clauses.limit_by);

    node = query;
    return true;
}

}

bool parseNestedSelectRichQuery(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserSelectRichQuery nested;
    return nested.parse(pos, node, expected);
}

bool ParserSelectRichQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ASTPtr first_query;
    if (!parseSelectRichCore(pos, first_query, expected))
        return false;

    ParserKeyword s_union(Keyword::UNION);
    ParserKeyword s_all(Keyword::ALL);
    ParserKeyword s_distinct(Keyword::DISTINCT);

    ASTPtr current = first_query;
    auto union_set = make_intrusive<ASTSelectSetLite>();
    bool has_union = false;

    while (true)
    {
        Pos union_pos = pos;
        if (!s_union.ignore(union_pos, expected))
            break;

        String mode = "DISTINCT";
        if (s_all.ignore(union_pos, expected))
            mode = "ALL";
        else
            s_distinct.ignore(union_pos, expected);

        ASTPtr next_query;
        if (!parseSelectRichCore(union_pos, next_query, expected))
            return false;

        if (!has_union)
        {
            union_set->children.push_back(first_query);
            has_union = true;
        }

        union_set->union_modes.push_back(mode);
        union_set->children.push_back(next_query);
        pos = union_pos;
    }

    node = has_union ? ASTPtr(union_set) : current;
    return true;
}

}
