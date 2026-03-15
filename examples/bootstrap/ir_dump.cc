#include "ir/IRSerializer.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"

#include <iostream>
#include <sstream>
#include <string>

int main()
{
    std::ostringstream buf;
    buf << std::cin.rdbuf();
    const std::string sql = buf.str();

    if (sql.empty())
    {
        std::cerr << "ir_dump: empty input\n";
        return 1;
    }

    DB::Tokens tokens(sql.data(), sql.data() + sql.size(), 0, true);
    DB::IParser::Pos pos(tokens, 4000, 4000);
    DB::Expected expected;
    DB::ASTPtr ast;
    DB::ParserSelectRichQuery parser;

    if (!parser.parse(pos, ast, expected))
    {
        std::cerr << "ir_dump: parse error\n";
        return 1;
    }

    try
    {
        auto ir = clickshack::IRSerializer::serializeQuery(ast.get());
        std::cout << ir.dump(2) << "\n";
    }
    catch (const std::exception & e)
    {
        std::cerr << "ir_dump: serialization error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
