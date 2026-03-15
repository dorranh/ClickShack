#include "ir/IRSerializer.h"

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTExpressionOpsLite.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTJoinLite.h"
#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/ASTSelectSetLite.h"
#include "ported_clickhouse/parsers/ASTTableAliasLite.h"
#include "ported_clickhouse/parsers/ASTTableExprLite.h"
#include "ported_clickhouse/parsers/ASTTableFunctionLite.h"
#include "ported_clickhouse/parsers/ASTTableIdentifierLite.h"
#include "ported_clickhouse/parsers/ASTWindowDefinitionLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"

#include <stdexcept>

namespace clickshack
{

using json = nlohmann::json;
using namespace DB;

// v1: spans are placeholders — AST does not carry source positions yet
static json span0() { return {{"start", 0}, {"end", 0}}; }

nlohmann::json IRSerializer::serializeQuery(const IAST * ast)
{
    if (!ast) throw std::runtime_error("IRSerializer: null AST");
    return {{"ir_version", "1"}, {"query", serializeNode(ast)}};
}

nlohmann::json IRSerializer::serializeNode(const IAST * node)
{
    if (!node) throw std::runtime_error("IRSerializer: null node");
    if (node->as<ASTSelectRichQuery>()) return serializeSelect(node);
    if (node->as<ASTSelectSetLite>())   return serializeUnion(node);
    return serializeExpr(node);
}

nlohmann::json IRSerializer::serializeSelect(const IAST * node)
{
    const auto * q = node->as<ASTSelectRichQuery>();
    json j;

    if (q->distinct)                j["distinct"]    = true;
    if (q->select_all)              j["all"]         = true;
    if (q->distinct_on_expressions) j["distinct_on"] = serializeExprList(q->distinct_on_expressions);
    if (q->top_present)             j["top"]         = {{"count", q->top_count}, {"with_ties", q->top_with_ties}};

    j["projections"] = q->expressions ? serializeExprList(q->expressions) : json::array();

    if (q->from_source) {
        json from = serializeTableExpr(q->from_source);
        if (q->from_final)    from["final"]         = true;
        if (q->sample_size)   from["sample"]        = serializeExpr(q->sample_size);
        if (q->sample_offset) from["sample_offset"] = serializeExpr(q->sample_offset);
        j["from"] = std::move(from);
    }
    if (q->array_join_expressions)
        j["array_join"] = {{"left", q->array_join_is_left},
                           {"exprs", serializeExprList(q->array_join_expressions)}};
    if (q->prewhere_expression) j["prewhere"] = serializeExpr(q->prewhere_expression);
    if (q->where_expression)    j["where"]    = serializeExpr(q->where_expression);

    if (q->group_by_all) {
        j["group_by"] = {{"all", true}};
    } else if (q->grouping_sets_expressions) {
        j["group_by"] = {{"grouping_sets", serializeExprList(q->grouping_sets_expressions)}};
    } else if (q->group_by_expressions) {
        json gb = {{"exprs", serializeExprList(q->group_by_expressions)}};
        if (q->group_by_with_rollup) gb["rollup"] = true;
        if (q->group_by_with_cube)   gb["cube"]   = true;
        if (q->group_by_with_totals) gb["totals"]  = true;
        j["group_by"] = std::move(gb);
    }

    if (q->having_expression)  j["having"]  = serializeExpr(q->having_expression);
    if (q->window_list)        j["window"]  = serializeWindowList(q->window_list);
    if (q->qualify_expression) j["qualify"] = serializeExpr(q->qualify_expression);

    if (q->order_by_all) {
        j["order_by"] = {{"all", true}};
    } else if (q->order_by_list) {
        j["order_by"] = serializeOrderByList(q->order_by_list);
    }

    if (q->limit)    j["limit"]    = serializeLimitClause(q->limit);
    if (q->limit_by) j["limit_by"] = serializeLimitByClause(q->limit_by);
    if (q->settings) j["settings"] = serializeExprList(q->settings);
    if (q->has_format) j["format"] = q->format_name;

    j["type"] = "Select";
    j["span"] = span0();

    // If WITH clause present, wrap Select in a With root
    if (q->with_expressions) {
        json with_node;
        with_node["type"]      = "With";
        with_node["span"]      = span0();
        with_node["recursive"] = q->with_recursive;

        json ctes = json::array();
        for (const auto & child : q->with_expressions->children) {
            if (!child) continue;
            const auto * fn = child->as<ASTFunction>();
            if (fn && fn->name == "with_subquery" && fn->arguments
                && fn->arguments->children.size() == 2)
            {
                const auto * alias_id = fn->arguments->children[0]
                    ? fn->arguments->children[0]->as<ASTIdentifier>()
                    : nullptr;
                json cte;
                cte["name"]  = alias_id ? alias_id->name() : "";
                cte["query"] = serializeNode(fn->arguments->children[1].get());
                ctes.push_back(std::move(cte));
            }
            else
            {
                ctes.push_back({{"expr", serializeExpr(child.get())}});
            }
        }
        with_node["ctes"] = std::move(ctes);
        with_node["body"] = std::move(j);
        return with_node;
    }

    return j;
}

nlohmann::json IRSerializer::serializeUnion(const IAST * node)
{
    const auto * u = node->as<ASTSelectSetLite>();
    if (u->children.empty())
        throw std::runtime_error("IRSerializer: SelectSetLite with no children");

    json result = serializeNode(u->children[0].get());
    for (size_t i = 0; i < u->set_ops.size() && i + 1 < u->children.size(); ++i) {
        json un;
        un["type"]       = "SelectUnion";
        un["span"]       = span0();
        un["op"]         = u->set_ops[i];
        un["quantifier"] = (i < u->union_modes.size()) ? u->union_modes[i] : "";
        un["left"]       = std::move(result);
        un["right"]      = serializeNode(u->children[i + 1].get());
        result = std::move(un);
    }
    return result;
}
nlohmann::json IRSerializer::serializeExprList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children)
        if (child) arr.push_back(serializeExpr(child.get()));
    return arr;
}

nlohmann::json IRSerializer::serializeExpr(const IAST * node)
{
    if (!node) return nullptr;

    // Literal
    if (auto * lit = node->as<ASTLiteral>()) {
        switch (lit->kind) {
            case ASTLiteral::Kind::Number:
                return {{"type","Literal"},{"kind","number"},{"value",lit->value},{"span",span0()}};
            case ASTLiteral::Kind::String:
                return {{"type","Literal"},{"kind","string"},{"value",lit->value},{"span",span0()}};
            case ASTLiteral::Kind::Null:
                return {{"type","Literal"},{"kind","null"},{"span",span0()}};
        }
    }

    // Identifier (includes "*" for STAR)
    if (auto * ident = node->as<ASTIdentifier>()) {
        const auto & n = ident->name();
        if (n == "*")
            return {{"type","Star"},{"span",span0()}};
        // table.* pattern stored as "table.*"
        auto dot = n.rfind('.');
        if (dot != std::string::npos && dot + 1 < n.size() && n[dot + 1] == '*')
            return {{"type","TableStar"},{"table",n.substr(0, dot)},{"span",span0()}};
        // qualified column: table.column
        if (dot != std::string::npos)
            return {{"type","Column"},{"table",n.substr(0,dot)},{"name",n.substr(dot+1)},{"span",span0()}};
        return {{"type","Column"},{"name",n},{"span",span0()}};
    }

    // ASTFunction covers: regular calls, CAST, CASE, IN, BETWEEN, LIKE, subquery, array, tuple, etc.
    if (auto * fn = node->as<ASTFunction>()) {
        const auto & nm = fn->name;

        // Subquery wrapper: subquery(select_node)
        if (nm == "subquery") {
            if (fn->arguments && !fn->arguments->children.empty())
                return {{"type","Subquery"},{"query",serializeNode(fn->arguments->children[0].get())},{"span",span0()}};
            return {{"type","Subquery"},{"query",nullptr},{"span",span0()}};
        }

        // Array literal: array(e1, e2, ...)
        if (nm == "array") {
            json elems = json::array();
            if (fn->arguments)
                for (const auto & ch : fn->arguments->children)
                    if (ch) elems.push_back(serializeExpr(ch.get()));
            return {{"type","Array"},{"elements",std::move(elems)},{"span",span0()}};
        }

        // Tuple literal: tuple(e1, e2, ...)
        if (nm == "tuple") {
            json elems = json::array();
            if (fn->arguments)
                for (const auto & ch : fn->arguments->children)
                    if (ch) elems.push_back(serializeExpr(ch.get()));
            return {{"type","Tuple"},{"elements",std::move(elems)},{"span",span0()}};
        }

        // CAST(expr, type_identifier)
        if (nm == "CAST") {
            json cast_expr = nullptr;
            std::string cast_type;
            if (fn->arguments && fn->arguments->children.size() >= 1)
                cast_expr = serializeExpr(fn->arguments->children[0].get());
            if (fn->arguments && fn->arguments->children.size() >= 2) {
                auto * type_id = fn->arguments->children[1]
                    ? fn->arguments->children[1]->as<ASTIdentifier>() : nullptr;
                cast_type = type_id ? type_id->name() : fn->arguments->children[1]->getID('_');
            }
            return {{"type","Cast"},{"expr",cast_expr},{"cast_type",cast_type},{"span",span0()}};
        }

        // IS NULL / IS NOT NULL -> isNull / isNotNull
        if (nm == "isNull" || nm == "isNotNull") {
            json inner = nullptr;
            if (fn->arguments && !fn->arguments->children.empty())
                inner = serializeExpr(fn->arguments->children[0].get());
            return {{"type","IsNull"},{"negated",nm == "isNotNull"},{"expr",inner},{"span",span0()}};
        }

        // IN / NOT IN -> in / notIn
        if (nm == "in" || nm == "notIn") {
            json expr_j = nullptr;
            json list_j = json::array();
            if (fn->arguments && fn->arguments->children.size() >= 1)
                expr_j = serializeExpr(fn->arguments->children[0].get());
            if (fn->arguments && fn->arguments->children.size() >= 2 && fn->arguments->children[1]) {
                // The second argument may be an ASTExpressionList or a subquery wrapper
                auto * ch = fn->arguments->children[1].get();
                if (auto * lst = ch->as<ASTExpressionList>())
                    list_j = serializeExprList(lst);
                else
                    // subquery in IN clause — wrap as single-element array for consistency
                    list_j = json::array({serializeExpr(ch)});
            }
            return {{"type","In"},{"negated",nm == "notIn"},{"expr",expr_j},{"list",list_j},{"span",span0()}};
        }

        // BETWEEN / NOT BETWEEN -> between / notBetween
        if (nm == "between" || nm == "notBetween") {
            json expr_j = nullptr, low_j = nullptr, high_j = nullptr;
            if (fn->arguments) {
                auto & ch = fn->arguments->children;
                if (ch.size() >= 1) expr_j = serializeExpr(ch[0].get());
                if (ch.size() >= 2) low_j  = serializeExpr(ch[1].get());
                if (ch.size() >= 3) high_j = serializeExpr(ch[2].get());
            }
            return {{"type","Between"},{"negated",nm == "notBetween"},
                    {"expr",expr_j},{"low",low_j},{"high",high_j},{"span",span0()}};
        }

        // LIKE / NOT LIKE / ILIKE / NOT ILIKE
        if (nm == "like" || nm == "notLike" || nm == "iLike" || nm == "notILike"
            || nm == "likeEscape" || nm == "notLikeEscape"
            || nm == "iLikeEscape" || nm == "notILikeEscape") {
            bool neg   = nm.find("not") == 0 || nm.find("Not") != std::string::npos;
            bool ilike = nm.find("iLike") != std::string::npos || nm.find("ILike") != std::string::npos;
            json expr_j = nullptr, pattern_j = nullptr;
            if (fn->arguments) {
                auto & ch = fn->arguments->children;
                if (ch.size() >= 1) expr_j    = serializeExpr(ch[0].get());
                if (ch.size() >= 2) pattern_j = serializeExpr(ch[1].get());
            }
            return {{"type","Like"},{"negated",neg},{"ilike",ilike},
                    {"expr",expr_j},{"pattern",pattern_j},{"span",span0()}};
        }

        // CASE: case(when, then, ..., else?) or caseWithExpr(subject, when, then, ..., else?)
        if (nm == "case" || nm == "caseWithExpr") {
            json args_j = json::array();
            if (fn->arguments)
                for (const auto & ch : fn->arguments->children)
                    if (ch) args_j.push_back(serializeExpr(ch.get()));
            return {{"type","Function"},{"name","CASE"},{"args",std::move(args_j)},{"span",span0()}};
        }

        // Lambda: lambda(params_list, body)  — produced by -> operator in some dialects.
        // In this parser -> is a binary op on ASTExpressionOpsLite, so handled below.

        // over(fn, window_spec)
        if (nm == "over") {
            json fn_j = nullptr, win_j = nullptr;
            if (fn->arguments) {
                auto & ch = fn->arguments->children;
                if (ch.size() >= 1) fn_j  = serializeExpr(ch[0].get());
                if (ch.size() >= 2) win_j = serializeExpr(ch[1].get());
            }
            return {{"type","Over"},{"function",fn_j},{"window",win_j},{"span",span0()}};
        }

        // alias(expr, name_identifier)
        if (nm == "alias") {
            json expr_j = nullptr;
            std::string alias_name;
            if (fn->arguments) {
                auto & ch = fn->arguments->children;
                if (ch.size() >= 1) expr_j = serializeExpr(ch[0].get());
                if (ch.size() >= 2 && ch[1]) {
                    auto * aid = ch[1]->as<ASTIdentifier>();
                    alias_name = aid ? aid->name() : ch[1]->getID('_');
                }
            }
            return {{"type","Alias"},{"expr",expr_j},{"alias",alias_name},{"span",span0()}};
        }

        // with_subquery(name_identifier, query) — used in CTE with_expressions
        if (nm == "with_subquery") {
            // Handled in serializeSelect CTE loop; if encountered here fall through to generic Function.
        }

        // Generic function call
        json args_j = json::array();
        if (fn->arguments)
            for (const auto & ch : fn->arguments->children)
                if (ch) args_j.push_back(serializeExpr(ch.get()));
        return {{"type","Function"},{"name",nm},{"args",std::move(args_j)},{"span",span0()}};
    }

    // ASTExpressionOpsLite — binary/unary arithmetic, comparison, logical, ::, ->
    if (auto * ops = node->as<ASTExpressionOpsLite>()) {
        if (ops->is_unary)
            return {{"type","UnaryOp"},{"op",ops->op},
                    {"expr",serializeExpr(ops->right)},{"span",span0()}};

        // :: (double colon cast) — right side is typically an identifier (type name)
        if (ops->op == "::") {
            std::string cast_type;
            if (ops->right) {
                if (auto * tid = ops->right->as<ASTIdentifier>())
                    cast_type = tid->name();
                else
                    cast_type = ops->right->getID('_');
            }
            return {{"type","Cast"},{"expr",serializeExpr(ops->left)},
                    {"cast_type",cast_type},{"span",span0()}};
        }

        // -> (lambda)
        if (ops->op == "->")
            return {{"type","Lambda"},
                    {"params",ops->left ? serializeExprList(ops->left) : json::array()},
                    {"body",serializeExpr(ops->right)},{"span",span0()}};

        // Default: binary operator
        return {{"type","BinaryOp"},{"op",ops->op},
                {"left",serializeExpr(ops->left)},{"right",serializeExpr(ops->right)},
                {"span",span0()}};
    }

    // Bare ASTExpressionList (e.g. when a list appears as an expression)
    if (auto * list = node->as<ASTExpressionList>()) {
        json arr = json::array();
        for (const auto & ch : list->children)
            if (ch) arr.push_back(serializeExpr(ch.get()));
        return arr;
    }

    // Inline subquery (ASTSelectRichQuery or ASTSelectSetLite used as an expression)
    if (node->as<ASTSelectRichQuery>() || node->as<ASTSelectSetLite>())
        return {{"type","Subquery"},{"query",serializeNode(node)},{"span",span0()}};

    // Fallback
    return {{"type","Raw"},{"id",node->getID('_')},{"span",span0()}};
}
nlohmann::json IRSerializer::serializeTableExpr(const IAST * node)
{
    if (!node) return nullptr;

    if (auto * join = node->as<ASTJoinLite>()) {
        static const char * types[]      = {"INNER","LEFT","RIGHT","FULL","CROSS","PASTE"};
        static const char * localities[] = {"","GLOBAL","LOCAL"};
        json j = {{"type","Join"},{"span",span0()}};
        j["kind"] = types[static_cast<int>(join->join_type)];
        if (join->join_locality != ASTJoinLite::JoinLocality::Unspecified)
            j["locality"] = localities[static_cast<int>(join->join_locality)];
        if (!join->strictness.empty()) j["strictness"] = join->strictness;
        j["left"]  = serializeTableExpr(join->left);
        j["right"] = serializeTableExpr(join->right);
        if (join->on_expression)  j["on"]    = serializeExpr(join->on_expression);
        if (join->using_columns)  j["using"] = serializeExprList(join->using_columns);
        return j;
    }

    if (auto * te = node->as<ASTTableExprLite>()) {
        json j;
        switch (te->kind) {
            case ASTTableExprLite::Kind::TableIdentifier:
                j = {{"type","Table"},{"span",span0()}};
                if (te->table_identifier) {
                    j["name"] = te->table_identifier->table;
                    if (!te->table_identifier->database.empty())
                        j["database"] = te->table_identifier->database;
                }
                break;
            case ASTTableExprLite::Kind::TableFunction:
                j = {{"type","TableFunction"},{"span",span0()}};
                if (te->table_function) {
                    j["function"] = {
                        {"type","Function"},
                        {"name", te->table_function->name},
                        {"args", te->table_function->arguments
                            ? serializeExprList(static_cast<IAST *>(te->table_function->arguments))
                            : json::array()},
                        {"span", span0()},
                    };
                }
                break;
            case ASTTableExprLite::Kind::Subquery:
                j = {{"type","SubquerySource"},{"span",span0()}};
                if (te->subquery) j["query"] = serializeNode(te->subquery);
                break;
        }
        if (te->alias) j["alias"] = te->alias->alias;
        return j;
    }

    return {{"type","Raw"},{"id",node->getID('_')},{"span",span0()}};
}

nlohmann::json IRSerializer::serializeOrderByList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children) {
        auto * elem = child ? child->as<ASTOrderByElementLite>() : nullptr;
        if (!elem) continue;
        json e = {{"span",span0()}};
        e["expr"]      = serializeExpr(elem->expression);
        e["direction"] = (elem->direction == ASTOrderByElementLite::Direction::Asc) ? "ASC" : "DESC";
        if (elem->nulls_order == ASTOrderByElementLite::NullsOrder::First) e["nulls"] = "FIRST";
        if (elem->nulls_order == ASTOrderByElementLite::NullsOrder::Last)  e["nulls"] = "LAST";
        if (!elem->collate_name.empty()) e["collate"] = elem->collate_name;
        if (elem->with_fill) {
            json fill;
            if (elem->fill_from)      fill["from"]      = serializeExpr(elem->fill_from);
            if (elem->fill_to)        fill["to"]        = serializeExpr(elem->fill_to);
            if (elem->fill_step)      fill["step"]      = serializeExpr(elem->fill_step);
            if (elem->fill_staleness) fill["staleness"] = serializeExpr(elem->fill_staleness);
            e["with_fill"] = std::move(fill);
        }
        if (elem->interpolate && elem->interpolate_expressions)
            e["interpolate"] = serializeExprList(elem->interpolate_expressions);
        arr.push_back(std::move(e));
    }
    return arr;
}

nlohmann::json IRSerializer::serializeWindowList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children) {
        auto * wd = child ? child->as<ASTWindowDefinitionLite>() : nullptr;
        if (!wd) continue;
        json w = {{"span",span0()},{"is_reference",wd->is_reference},{"body",wd->body}};
        if (!wd->name.empty()) w["name"] = wd->name;
        arr.push_back(std::move(w));
    }
    return arr;
}

nlohmann::json IRSerializer::serializeLimitClause(const IAST * node)
{
    const auto * lim = node ? node->as<ASTLimitLite>() : nullptr;
    if (!lim) return nullptr;
    json j = {{"type","Limit"},{"span",span0()}};
    if (lim->limit_expression)
        j["count"] = serializeExpr(lim->limit_expression);
    else if (!lim->limit.empty())
        j["count"] = {{"type","Literal"},{"kind","number"},{"value",lim->limit},{"span",span0()}};
    if (lim->offset_present) {
        if (lim->offset_expression)
            j["offset"] = serializeExpr(lim->offset_expression);
        else if (!lim->offset.empty())
            j["offset"] = {{"type","Literal"},{"kind","number"},{"value",lim->offset},{"span",span0()}};
    }
    if (lim->with_ties) j["with_ties"] = true;
    return j;
}

nlohmann::json IRSerializer::serializeLimitByClause(const IAST * node)
{
    const auto * lb = node ? node->as<ASTLimitByLite>() : nullptr;
    if (!lb) return nullptr;
    json j = {{"type","LimitBy"},{"span",span0()}};
    if (lb->limit_expression)
        j["count"] = serializeExpr(lb->limit_expression);
    else if (!lb->limit.empty())
        j["count"] = {{"type","Literal"},{"kind","number"},{"value",lb->limit},{"span",span0()}};
    if (lb->offset_present) {
        if (lb->offset_expression)
            j["offset"] = serializeExpr(lb->offset_expression);
        else if (!lb->offset.empty())
            j["offset"] = {{"type","Literal"},{"kind","number"},{"value",lb->offset},{"span",span0()}};
    }
    if (lb->by_all)
        j["by"] = {{"type","Star"},{"span",span0()}};
    else if (lb->by_expressions)
        j["by"] = serializeExprList(lb->by_expressions);
    return j;
}

} // namespace clickshack
