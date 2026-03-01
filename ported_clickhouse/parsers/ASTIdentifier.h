#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/parsers/ASTIdentifier_fwd.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTIdentifier : public IAST
{
public:
    explicit ASTIdentifier(const String & short_name);

    String getID(char delim) const override { return "Identifier" + String(1, delim) + name_; }
    ASTPtr clone() const override;

    const String & name() const { return name_; }

private:
    String name_;
};

}
