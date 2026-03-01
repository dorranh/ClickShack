#include <iostream>
#include <string>

#include "ported_clickhouse/parsers/ASTIdentifier_fwd.h"
#include "ported_clickhouse/parsers/ASTUseQuery.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserUseQuery.h"

int main(int argc, char ** argv)
{
    std::string query = "USE mydb";
    if (argc > 1)
        query = argv[1];

    DB::Tokens tokens(query.data(), query.data() + query.size(), 0, true);
    DB::IParser::Pos pos(tokens, 1000, 1000);
    DB::Expected expected;
    DB::ASTPtr node;
    DB::ParserUseQuery parser;

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

    auto * use = node ? node->as<DB::ASTUseQuery>() : nullptr;
    if (!use || !use->database)
    {
        std::cerr << "no database ast" << std::endl;
        return 3;
    }

    std::string db;
    if (!DB::tryGetIdentifierNameInto(use->database, db))
    {
        std::cerr << "database extraction failed" << std::endl;
        return 4;
    }

    std::cout << db << std::endl;
    return 0;
}
