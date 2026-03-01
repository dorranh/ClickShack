#include "ported_clickhouse/parsers/ASTExpressionList.h"

namespace DB
{

ASTPtr ASTExpressionList::clone() const
{
    auto clone_ast = make_intrusive<ASTExpressionList>(separator);
    for (const auto & child : children)
        clone_ast->children.push_back(child ? child->clone() : nullptr);
    return clone_ast;
}

}
