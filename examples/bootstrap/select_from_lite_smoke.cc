#include <iostream>
#include <string>

#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/ASTSelectFromLiteQuery.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserSelectFromLiteQuery.h"

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
    std::string query = "SELECT a, 1, f(x) FROM mydb.mytable";
    if (argc > 1)
        query = argv[1];

    DB::Tokens tokens(query.data(), query.data() + query.size(), 0, true);
    DB::IParser::Pos pos(tokens, 1000, 1000);
    DB::Expected expected;
    DB::ASTPtr node;
    DB::ParserSelectFromLiteQuery parser;

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

    auto * select = node ? node->as<DB::ASTSelectFromLiteQuery>() : nullptr;
    if (!select || !select->expressions || !select->from_table)
    {
        std::cerr << "missing select-from ast" << std::endl;
        return 3;
    }

    const auto & expressions = select->expressions->children;
    if (expressions.empty())
    {
        std::cerr << "empty expression list" << std::endl;
        return 4;
    }

    std::cout << "expressions=" << expressions.size()
              << " table=" << select->from_table->table
              << " database=" << (select->from_table->database.empty() ? "<none>" : select->from_table->database)
              << " first=" << describeFirstExpression(expressions.front())
              << std::endl;
    return 0;
}
