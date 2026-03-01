#include "ported_clickhouse/parsers/ASTFunction.h"

namespace DB
{

ASTPtr ASTFunction::clone() const
{
    auto res = make_intrusive<ASTFunction>();
    res->name = name;
    if (arguments)
        res->set(res->arguments, arguments->clone());
    return res;
}

}
