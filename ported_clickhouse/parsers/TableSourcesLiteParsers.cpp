#include "ported_clickhouse/parsers/TableSourcesLiteParsers.h"

#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTJoinLite.h"
#include "ported_clickhouse/parsers/ASTTableAliasLite.h"
#include "ported_clickhouse/parsers/ASTTableExprLite.h"
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

bool parseAlias(IParser::Pos & pos, ASTPtr & alias, Expected & expected)
{
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserKeyword s_as(Keyword::AS);

    if (s_as.ignore(pos, expected))
    {
        ASTPtr id;
        if (!identifier_p.parse(pos, id, expected))
            return false;
        auto * alias_id = id ? id->as<ASTIdentifier>() : nullptr;
        if (!alias_id)
            return false;
        auto alias_ast = make_intrusive<ASTTableAliasLite>();
        alias_ast->alias = alias_id->name();
        alias = alias_ast;
        return true;
    }

    if (isKeyword(pos, "WHERE") || isKeyword(pos, "PREWHERE") || isKeyword(pos, "GROUP") || isKeyword(pos, "ORDER") || isKeyword(pos, "LIMIT")
        || isKeyword(pos, "INNER") || isKeyword(pos, "LEFT") || isKeyword(pos, "RIGHT") || isKeyword(pos, "FULL")
        || isKeyword(pos, "CROSS") || isKeyword(pos, "JOIN") || isKeyword(pos, "ON") || isKeyword(pos, "OFFSET")
        || isKeyword(pos, "USING") || isKeyword(pos, "QUALIFY")
        || isKeyword(pos, "WINDOW") || isKeyword(pos, "ARRAY") || isKeyword(pos, "SAMPLE")
        || isKeyword(pos, "FINAL") || isKeyword(pos, "GLOBAL") || isKeyword(pos, "OUTER")
        || isKeyword(pos, "ANY") || isKeyword(pos, "SEMI") || isKeyword(pos, "ANTI") || isKeyword(pos, "ASOF")
        || isKeyword(pos, "FETCH") || isKeyword(pos, "ROW") || isKeyword(pos, "ROWS") || isKeyword(pos, "ONLY")
        || isKeyword(pos, "INTERSECT") || isKeyword(pos, "EXCEPT")
        || isKeyword(pos, "SETTINGS") || isKeyword(pos, "FORMAT")
        || isKeyword(pos, "ROLLUP") || isKeyword(pos, "CUBE") || isKeyword(pos, "TOTALS")
        || isKeyword(pos, "UNION") || isKeyword(pos, "HAVING") || isKeyword(pos, "WITH") || isKeyword(pos, "SELECT")
        || isKeyword(pos, "DISTINCT") || isKeyword(pos, "ALL"))
        return true;

    ASTPtr id;
    IParser::Pos alias_pos = pos;
    if (!identifier_p.parse(alias_pos, id, expected))
        return true;

    auto * alias_id = id ? id->as<ASTIdentifier>() : nullptr;
    if (!alias_id)
        return true;

    auto alias_ast = make_intrusive<ASTTableAliasLite>();
    alias_ast->alias = alias_id->name();
    alias = alias_ast;
    pos = alias_pos;
    return true;
}

bool parseTableAtom(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserToken dot(TokenType::Dot);
    ParserToken fn_open(TokenType::OpeningRoundBracket);
    ParserToken fn_close(TokenType::ClosingRoundBracket);
    ParserToken comma(TokenType::Comma);
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);

    if (open.ignore(pos, expected))
    {
        ASTPtr subquery;
        if (!parseNestedSelectRichQuery(pos, subquery, expected))
            return false;
        if (!close.ignore(pos, expected))
            return false;

        auto expr = make_intrusive<ASTTableExprLite>();
        expr->kind = ASTTableExprLite::Kind::Subquery;
        expr->set(expr->subquery, subquery);

        ASTPtr alias;
        if (!parseAlias(pos, alias, expected))
            return false;
        if (alias)
            expr->set(expr->alias, alias);

        node = expr;
        return true;
    }

    ASTPtr first;
    if (!identifier_p.parse(pos, first, expected))
        return false;
    auto * first_id = first ? first->as<ASTIdentifier>() : nullptr;
    if (!first_id)
        return false;

    String compound = first_id->name();
    Strings parts;
    parts.push_back(first_id->name());

    while (dot.ignore(pos, expected))
    {
        ASTPtr part;
        if (!identifier_p.parse(pos, part, expected))
            return false;
        auto * id = part ? part->as<ASTIdentifier>() : nullptr;
        if (!id)
            return false;
        parts.push_back(id->name());
        compound += ".";
        compound += id->name();
    }

    if (fn_open.ignore(pos, expected))
    {
        auto fn = make_intrusive<ASTTableFunctionLite>();
        fn->name = compound;

        ASTPtr args_node;
        if (!fn_close.ignore(pos, expected))
        {
            auto args = make_intrusive<ASTExpressionList>();
            ParserExpressionOpsLite expr_p;

            ASTPtr first_arg;
            if (!expr_p.parse(pos, first_arg, expected))
                return false;
            args->children.push_back(first_arg);

            while (comma.ignore(pos, expected))
            {
                ASTPtr arg;
                if (!expr_p.parse(pos, arg, expected))
                    return false;
                args->children.push_back(arg);
            }

            if (!fn_close.ignore(pos, expected))
                return false;
            args_node = args;
        }

        if (args_node)
            fn->set(fn->arguments, args_node);

        auto expr = make_intrusive<ASTTableExprLite>();
        expr->kind = ASTTableExprLite::Kind::TableFunction;
        expr->set(expr->table_function, fn);

        ASTPtr alias;
        if (!parseAlias(pos, alias, expected))
            return false;
        if (alias)
            expr->set(expr->alias, alias);

        node = expr;
        return true;
    }

    auto table_id = make_intrusive<ASTTableIdentifierLite>();
    if (parts.size() == 1)
    {
        table_id->table = parts[0];
    }
    else
    {
        table_id->table = parts.back();
        table_id->database = parts[0];
        for (size_t i = 1; i + 1 < parts.size(); ++i)
            table_id->database += "." + parts[i];
    }

    auto expr = make_intrusive<ASTTableExprLite>();
    expr->kind = ASTTableExprLite::Kind::TableIdentifier;
    expr->set(expr->table_identifier, table_id);

    ASTPtr alias;
    if (!parseAlias(pos, alias, expected))
        return false;
    if (alias)
        expr->set(expr->alias, alias);

    node = expr;
    return true;
}

bool parseJoinOperator(IParser::Pos & pos, ASTJoinLite::JoinType & join_type, bool & is_global, String & strictness)
{
    is_global = false;
    strictness.clear();

    if (isKeyword(pos, "GLOBAL"))
    {
        is_global = true;
        ++pos;
    }

    auto tryParseStrictness = [&](IParser::Pos & it) -> bool
    {
        if (isKeyword(it, "ANY"))
        {
            strictness = "ANY";
            ++it;
            return true;
        }
        if (isKeyword(it, "ALL"))
        {
            strictness = "ALL";
            ++it;
            return true;
        }
        if (isKeyword(it, "SEMI"))
        {
            strictness = "SEMI";
            ++it;
            return true;
        }
        if (isKeyword(it, "ANTI"))
        {
            strictness = "ANTI";
            ++it;
            return true;
        }
        if (isKeyword(it, "ASOF"))
        {
            strictness = "ASOF";
            ++it;
            return true;
        }
        return false;
    };

    tryParseStrictness(pos);

    if (isKeyword(pos, "INNER"))
    {
        ++pos;
        tryParseStrictness(pos);
        if (!isKeyword(pos, "JOIN"))
            return false;
        ++pos;
        join_type = ASTJoinLite::JoinType::Inner;
        return true;
    }

    if (isKeyword(pos, "LEFT"))
    {
        ++pos;
        if (isKeyword(pos, "OUTER"))
            ++pos;
        tryParseStrictness(pos);
        if (!isKeyword(pos, "JOIN"))
            return false;
        ++pos;
        join_type = ASTJoinLite::JoinType::Left;
        return true;
    }

    if (isKeyword(pos, "RIGHT"))
    {
        ++pos;
        if (isKeyword(pos, "OUTER"))
            ++pos;
        tryParseStrictness(pos);
        if (!isKeyword(pos, "JOIN"))
            return false;
        ++pos;
        join_type = ASTJoinLite::JoinType::Right;
        return true;
    }

    if (isKeyword(pos, "FULL"))
    {
        ++pos;
        if (isKeyword(pos, "OUTER"))
            ++pos;
        tryParseStrictness(pos);
        if (!isKeyword(pos, "JOIN"))
            return false;
        ++pos;
        join_type = ASTJoinLite::JoinType::Full;
        return true;
    }

    if (isKeyword(pos, "CROSS"))
    {
        ++pos;
        if (!strictness.empty())
            return false;
        if (!isKeyword(pos, "JOIN"))
            return false;
        ++pos;
        join_type = ASTJoinLite::JoinType::Cross;
        return true;
    }

    if (isKeyword(pos, "JOIN"))
    {
        ++pos;
        if (strictness.empty())
            strictness = "ALL";
        join_type = ASTJoinLite::JoinType::Inner;
        return true;
    }

    return false;
}

bool parseUsingColumns(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserToken comma(TokenType::Comma);
    ParserIdentifier identifier_p(/*allow_query_parameter*/ true);
    ParserToken dot_p(TokenType::Dot);

    bool has_parens = open.ignore(pos, expected);

    auto list = make_intrusive<ASTExpressionList>();

    while (true)
    {
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

        list->children.push_back(make_intrusive<ASTIdentifier>(name));

        if (!comma.ignore(pos, expected))
            break;
    }

    if (has_parens && !close.ignore(pos, expected))
        return false;

    node = list;
    return true;
}

}

bool ParserTableSourceLite::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ASTPtr left;
    if (!parseTableAtom(pos, left, expected))
        return false;

    while (true)
    {
        Pos join_pos = pos;
        ASTJoinLite::JoinType join_type = ASTJoinLite::JoinType::Inner;
        bool is_global = false;
        String strictness;
        if (!parseJoinOperator(join_pos, join_type, is_global, strictness))
            break;

        ASTPtr right;
        if (!parseTableAtom(join_pos, right, expected))
            return false;

        ASTPtr on_expr;
        ASTPtr using_columns;
        if (join_type == ASTJoinLite::JoinType::Cross)
        {
            if (isKeyword(join_pos, "ON") || isKeyword(join_pos, "USING"))
                return false;
        }
        else
        {
            if (isKeyword(join_pos, "ON"))
            {
                ++join_pos;
                ParserExpressionOpsLite expr_p;
                if (!expr_p.parse(join_pos, on_expr, expected))
                    return false;
            }
            else if (isKeyword(join_pos, "USING"))
            {
                ++join_pos;
                if (!parseUsingColumns(join_pos, using_columns, expected))
                    return false;
            }
            else
            {
                return false;
            }
        }

        auto join = make_intrusive<ASTJoinLite>();
        join->join_type = join_type;
        join->is_global = is_global;
        join->strictness = strictness;
        join->set(join->left, left);
        join->set(join->right, right);
        if (on_expr)
            join->set(join->on_expression, on_expr);
        if (using_columns)
            join->set(join->using_columns, using_columns);

        left = join;
        pos = join_pos;
    }

    node = left;
    return true;
}

}
