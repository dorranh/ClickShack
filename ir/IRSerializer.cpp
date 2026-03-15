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
nlohmann::json IRSerializer::serializeExpr(const IAST * n) { return {{"type","Raw"},{"id",n->getID('_')},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeExprList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeTableExpr(const IAST *) { return {{"type","Raw"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeOrderByList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeWindowList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeLimitClause(const IAST *) { return {{"type","Limit"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeLimitByClause(const IAST *) { return {{"type","LimitBy"},{"span",span0()}}; }

} // namespace clickshack
