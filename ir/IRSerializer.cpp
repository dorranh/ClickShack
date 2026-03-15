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

// Stubs — replaced in Tasks 9 and 11
nlohmann::json IRSerializer::serializeSelect(const IAST *) { return {{"type","Select"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeUnion(const IAST *)  { return {{"type","SelectUnion"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeExpr(const IAST * n) { return {{"type","Raw"},{"id",n->getID('_')},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeExprList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeTableExpr(const IAST *) { return {{"type","Raw"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeOrderByList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeWindowList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeLimitClause(const IAST *) { return {{"type","Limit"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeLimitByClause(const IAST *) { return {{"type","LimitBy"},{"span",span0()}}; }

} // namespace clickshack
