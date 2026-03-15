#pragma once

#include "nlohmann/json.hpp"
#include "ported_clickhouse/parsers/IAST.h"

namespace clickshack
{

class IRSerializer
{
public:
    // Entry point. Accepts ASTSelectRichQuery* or ASTSelectSetLite* at root.
    // Returns envelope: {"ir_version": "1", "query": {...}}
    static nlohmann::json serializeQuery(const DB::IAST * ast);

private:
    static nlohmann::json serializeNode(const DB::IAST * node);
    static nlohmann::json serializeSelect(const DB::IAST * node);
    static nlohmann::json serializeUnion(const DB::IAST * node);
    static nlohmann::json serializeExpr(const DB::IAST * node);
    static nlohmann::json serializeExprList(const DB::IAST * node);
    static nlohmann::json serializeTableExpr(const DB::IAST * node);
    static nlohmann::json serializeOrderByList(const DB::IAST * node);
    static nlohmann::json serializeWindowList(const DB::IAST * node);
    static nlohmann::json serializeLimitClause(const DB::IAST * node);
    static nlohmann::json serializeLimitByClause(const DB::IAST * node);
};

} // namespace clickshack
