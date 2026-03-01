#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery_fwd.h"
#include "ported_clickhouse/parsers/ASTTableAliasLite.h"
#include "ported_clickhouse/parsers/ASTTableFunctionLite.h"
#include "ported_clickhouse/parsers/ASTTableIdentifierLite.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTTableExprLite : public IAST
{
public:
    enum class Kind : UInt8
    {
        TableIdentifier,
        TableFunction,
        Subquery,
    };

    Kind kind = Kind::TableIdentifier;
    ASTTableIdentifierLite * table_identifier = nullptr;
    ASTTableFunctionLite * table_function = nullptr;
    IAST * subquery = nullptr;
    ASTTableAliasLite * alias = nullptr;

    String getID(char delim) const override
    {
        String kind_name = "table";
        if (kind == Kind::TableFunction)
            kind_name = "function";
        else if (kind == Kind::Subquery)
            kind_name = "subquery";
        return "TableExprLite" + String(1, delim) + kind_name;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTTableExprLite>();
        res->kind = kind;
        if (table_identifier)
            res->set(res->table_identifier, table_identifier->clone());
        if (table_function)
            res->set(res->table_function, table_function->clone());
        if (subquery)
            res->set(res->subquery, subquery->clone());
        if (alias)
            res->set(res->alias, alias->clone());
        return res;
    }
};

}
