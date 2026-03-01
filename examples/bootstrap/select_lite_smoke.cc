#include <iostream>
#include <string>

#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/ASTSelectLiteQuery.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserSelectLiteQuery.h"

namespace
{
std::string describeFirstExpression(const DB::ASTPtr & expr)
{
    if (!expr)
        return "none";

    if (auto * id = expr->as<DB::ASTIdentifier>())
        return "identifier:" + id->name();

    if (auto * lit = expr->as<DB::ASTLiteral>())
    {
        if (lit->kind == DB::ASTLiteral::Kind::Number)
            return "literal:number";
        return "literal:string";
    }

    if (auto * fn = expr->as<DB::ASTFunction>())
        return "function:" + fn->name;

    return "unknown";
}
}

int main(int argc, char ** argv)
{
    std::string query = "SELECT a, 1, f(x)";
    if (argc > 1)
        query = argv[1];

    DB::Tokens tokens(query.data(), query.data() + query.size(), 0, true);
    DB::IParser::Pos pos(tokens, 1000, 1000);
    DB::Expected expected;
    DB::ASTPtr node;
    DB::ParserSelectLiteQuery parser;

    if (!parser.parse(pos, node, expected))
    {
        std::cerr << "parse failed" << std::endl;
        return 1;
    }

    if (!pos->isEnd())
    {
        std::cerr << "trailing tokens" << std::endl;
        return 2;
    }

    auto * select = node ? node->as<DB::ASTSelectLiteQuery>() : nullptr;
    if (!select || !select->expressions)
    {
        std::cerr << "no expressions ast" << std::endl;
        return 3;
    }

    const auto & expressions = select->expressions->children;
    if (expressions.empty())
    {
        std::cerr << "empty expression list" << std::endl;
        return 4;
    }

    std::cout << "expressions=" << expressions.size()
              << " first=" << describeFirstExpression(expressions.front()) << std::endl;
    return 0;
}
