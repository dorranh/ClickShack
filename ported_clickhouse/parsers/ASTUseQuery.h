#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTIdentifier_fwd.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTUseQuery : public IAST
{
public:
    IAST * database = nullptr;

    String getDatabase() const
    {
        String name;
        tryGetIdentifierNameInto(database, name);
        return name;
    }

    String getID(char delim) const override { return "UseQuery" + String(1, delim) + getDatabase(); }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTUseQuery>();
        if (database)
            res->set(res->database, database->clone());
        return res;
    }
};

}
