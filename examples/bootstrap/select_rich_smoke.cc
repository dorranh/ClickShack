#include <iostream>
#include <string>
#include <vector>

#include "ported_clickhouse/parsers/ASTJoinLite.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/ASTTableExprLite.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"

namespace
{
std::string joinCsv(const std::vector<std::string> & values)
{
    if (values.empty())
        return "none";

    std::string out;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i)
            out += ",";
        out += values[i];
    }
    return out;
}

std::string joinTypeToString(DB::ASTJoinLite::JoinType type)
{
    switch (type)
    {
        case DB::ASTJoinLite::JoinType::Inner:
            return "INNER";
        case DB::ASTJoinLite::JoinType::Left:
            return "LEFT";
        case DB::ASTJoinLite::JoinType::Right:
            return "RIGHT";
        case DB::ASTJoinLite::JoinType::Full:
            return "FULL";
        case DB::ASTJoinLite::JoinType::Cross:
            return "CROSS";
    }
    return "UNKNOWN";
}

void summarizeSource(const DB::IAST * node, std::vector<std::string> & join_types, std::vector<std::string> & source_kinds)
{
    if (!node)
        return;

    if (const auto * join = node->as<DB::ASTJoinLite>())
    {
        join_types.push_back(joinTypeToString(join->join_type));
        summarizeSource(join->left, join_types, source_kinds);
        summarizeSource(join->right, join_types, source_kinds);
        return;
    }

    if (const auto * source = node->as<DB::ASTTableExprLite>())
    {
        switch (source->kind)
        {
            case DB::ASTTableExprLite::Kind::TableIdentifier:
                source_kinds.push_back("table");
                break;
            case DB::ASTTableExprLite::Kind::TableFunction:
                source_kinds.push_back("function");
                break;
            case DB::ASTTableExprLite::Kind::Subquery:
                source_kinds.push_back("subquery");
                break;
        }
        return;
    }

    source_kinds.push_back("unknown");
}
}

int main(int argc, char ** argv)
{
    std::string query = "SELECT a FROM t";
    if (argc > 1)
        query = argv[1];

    DB::Tokens tokens(query.data(), query.data() + query.size(), 0, true);
    DB::IParser::Pos pos(tokens, 4000, 4000);
    DB::Expected expected;
    DB::ASTPtr node;
    DB::ParserSelectRichQuery parser;

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

    auto * select = node ? node->as<DB::ASTSelectRichQuery>() : nullptr;
    if (!select || !select->expressions || !select->from_source)
    {
        std::cerr << "missing select rich ast" << std::endl;
        return 3;
    }

    std::vector<std::string> join_types;
    std::vector<std::string> source_kinds;
    summarizeSource(select->from_source, join_types, source_kinds);

    std::cout << "projections=" << select->expressions->children.size()
              << " joins=" << join_types.size()
              << " join_types=" << joinCsv(join_types)
              << " source_kinds=" << joinCsv(source_kinds)
              << " where=" << (select->where_expression ? 1 : 0)
              << " group_by=" << (select->group_by_expressions ? 1 : 0)
              << " order_by=" << (select->order_by_list ? 1 : 0)
              << " limit=" << (select->limit ? 1 : 0)
              << " offset=" << (select->limit && select->limit->offset_present ? 1 : 0);

    if (select->limit)
    {
        std::cout << " limit_value=" << select->limit->limit;
        if (select->limit->offset_present)
            std::cout << " offset_value=" << select->limit->offset;
    }

    std::cout << std::endl;
    return 0;
}
