#include "ported_clickhouse/parsers/ASTIdentifier.h"

#include "ported_clickhouse/common/ErrorCodes.h"

namespace DB
{

ASTIdentifier::ASTIdentifier(const String & short_name)
    : name_(short_name)
{
}

ASTPtr ASTIdentifier::clone() const
{
    return make_intrusive<ASTIdentifier>(name_);
}

String getIdentifierName(const IAST * ast)
{
    String res;
    if (tryGetIdentifierNameInto(ast, res))
        return res;
    throw Exception(ErrorCodes::UNEXPECTED_AST_STRUCTURE, "AST node is not an identifier");
}

std::optional<String> tryGetIdentifierName(const IAST * ast)
{
    String res;
    if (tryGetIdentifierNameInto(ast, res))
        return res;
    return {};
}

bool tryGetIdentifierNameInto(const IAST * ast, String & name)
{
    if (!ast)
        return false;
    if (const auto * id = dynamic_cast<const ASTIdentifier *>(ast))
    {
        name = id->name();
        return true;
    }
    return false;
}

void setIdentifierSpecial(ASTPtr &)
{
    // no-op in pruned port
}

}
