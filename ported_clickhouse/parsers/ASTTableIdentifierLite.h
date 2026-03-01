#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTTableIdentifierLite : public IAST
{
public:
    String database;
    String table;

    String getID(char delim) const override
    {
        if (database.empty())
            return "TableIdentifierLite" + String(1, delim) + table;
        return "TableIdentifierLite" + String(1, delim) + database + "." + table;
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTTableIdentifierLite>();
        res->database = database;
        res->table = table;
        return res;
    }
};

}
