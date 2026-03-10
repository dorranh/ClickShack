#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cctype>

#include "ported_clickhouse/parsers/ASTJoinLite.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/ASTSelectSetLite.h"
#include "ported_clickhouse/parsers/ASTTableExprLite.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"

namespace
{
struct FixtureEntry
{
    std::string id;
    std::string path;
    std::string priority;
    std::string expected_result_kind;
    std::string expected_canonical_summary;
    std::string expected_failure_class;
};

struct SummaryResult
{
    int code = 1;
    std::string classification;
    std::string summary;
};

constexpr int kRepeatRuns = 3;

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

std::string summarizeLimitExpr(const DB::IAST * node)
{
    if (!node)
        return "";
    if (const auto * literal = node->as<DB::ASTLiteral>())
        return literal->value;
    if (const auto * identifier = node->as<DB::ASTIdentifier>())
        return identifier->name();
    return node->getID('|');
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
        case DB::ASTJoinLite::JoinType::Paste:
            return "PASTE";
    }
    return "UNKNOWN";
}

std::string joinLocalityToString(DB::ASTJoinLite::JoinLocality locality)
{
    switch (locality)
    {
        case DB::ASTJoinLite::JoinLocality::Unspecified:
            return "UNSPECIFIED";
        case DB::ASTJoinLite::JoinLocality::Global:
            return "GLOBAL";
        case DB::ASTJoinLite::JoinLocality::Local:
            return "LOCAL";
    }
    return "UNKNOWN";
}

void summarizeSource(
    const DB::IAST * node,
    std::vector<std::string> & join_types,
    std::vector<std::string> & join_modes,
    std::vector<std::string> & join_localities,
    std::vector<std::string> & source_kinds)
{
    if (!node)
        return;

    if (const auto * join = node->as<DB::ASTJoinLite>())
    {
        join_types.push_back(joinTypeToString(join->join_type));
        std::string mode = join->strictness.empty() ? "none" : join->strictness;
        if (join->is_global)
            mode = "GLOBAL-" + mode;
        join_modes.push_back(mode);
        join_localities.push_back(joinLocalityToString(join->join_locality));
        summarizeSource(join->left, join_types, join_modes, join_localities, source_kinds);
        summarizeSource(join->right, join_types, join_modes, join_localities, source_kinds);
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

void summarizeExpression(
    const DB::IAST * node,
    size_t & simple_case_count,
    size_t & over_count,
    size_t & over_named_count,
    size_t & over_inline_count)
{
    if (!node)
        return;

    if (const auto * fn = node->as<DB::ASTFunction>())
    {
        if (fn->name == "caseWithExpr")
            ++simple_case_count;
        else if (fn->name == "over")
        {
            ++over_count;
            if (fn->arguments && fn->arguments->children.size() >= 2)
            {
                const auto * named = fn->arguments->children[1] ? fn->arguments->children[1]->as<DB::ASTIdentifier>() : nullptr;
                if (named)
                    ++over_named_count;
                else
                    ++over_inline_count;
            }
        }
    }

    for (const auto & child : node->children)
        summarizeExpression(child.get(), simple_case_count, over_count, over_named_count, over_inline_count);
}

std::string readFile(const std::string & path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return {};
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool isAbsolutePath(const std::string & path)
{
    if (path.empty())
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return true;
    return path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
}

std::string unescapeJsonString(const std::string & value)
{
    std::string out;
    out.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 1 < value.size())
        {
            ++i;
            switch (value[i])
            {
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '"':
                    out.push_back('"');
                    break;
                default:
                    out.push_back(value[i]);
                    break;
            }
            continue;
        }
        out.push_back(value[i]);
    }

    return out;
}

std::optional<std::string> extractJsonStringField(const std::string & object, const std::string & key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    std::smatch match;
    if (!std::regex_search(object, match, pattern))
        return std::nullopt;
    return unescapeJsonString(match[1].str());
}

std::string requireJsonStringField(
    const std::string & object,
    const std::string & key,
    const std::string & manifest_path)
{
    const auto value = extractJsonStringField(object, key);
    if (!value)
    {
        std::cerr << "manifest parse error: missing string field '" << key << "' in " << manifest_path << std::endl;
        return {};
    }
    return *value;
}

std::string extractFixturesArray(const std::string & manifest_text)
{
    const size_t fixtures_key = manifest_text.find("\"fixtures\"");
    if (fixtures_key == std::string::npos)
        return {};

    const size_t open_bracket = manifest_text.find('[', fixtures_key);
    if (open_bracket == std::string::npos)
        return {};

    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (size_t i = open_bracket; i < manifest_text.size(); ++i)
    {
        const char c = manifest_text[i];
        if (in_string)
        {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in_string = false;
            continue;
        }

        if (c == '"')
        {
            in_string = true;
            continue;
        }
        if (c == '[')
            ++depth;
        else if (c == ']')
        {
            --depth;
            if (depth == 0)
                return manifest_text.substr(open_bracket + 1, i - open_bracket - 1);
        }
    }

    return {};
}

std::vector<std::string> extractFixtureObjects(const std::string & fixtures_array)
{
    std::vector<std::string> objects;
    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    size_t object_start = 0;

    for (size_t i = 0; i < fixtures_array.size(); ++i)
    {
        const char c = fixtures_array[i];
        if (in_string)
        {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in_string = false;
            continue;
        }

        if (c == '"')
        {
            in_string = true;
            continue;
        }
        if (c == '{')
        {
            if (depth == 0)
                object_start = i;
            ++depth;
            continue;
        }
        if (c == '}')
        {
            --depth;
            if (depth == 0)
                objects.push_back(fixtures_array.substr(object_start, i - object_start + 1));
        }
    }

    return objects;
}

std::vector<FixtureEntry> loadManifestFixtures(const std::string & manifest_path)
{
    const std::string text = readFile(manifest_path);
    if (text.empty())
    {
        std::cerr << "manifest parse error: empty or unreadable manifest: " << manifest_path << std::endl;
        return {};
    }

    const std::string fixtures_array = extractFixturesArray(text);
    if (fixtures_array.empty())
    {
        std::cerr << "manifest parse error: fixtures array not found in " << manifest_path << std::endl;
        return {};
    }

    std::vector<FixtureEntry> fixtures;
    for (const auto & object : extractFixtureObjects(fixtures_array))
    {
        FixtureEntry fixture;
        fixture.id = requireJsonStringField(object, "id", manifest_path);
        fixture.path = requireJsonStringField(object, "path", manifest_path);
        fixture.priority = requireJsonStringField(object, "priority", manifest_path);
        fixture.expected_result_kind = requireJsonStringField(object, "expected_result_kind", manifest_path);
        if (fixture.id.empty() || fixture.path.empty() || fixture.priority.empty() || fixture.expected_result_kind.empty())
            return {};

        if (const auto expected = extractJsonStringField(object, "expected_canonical_summary"))
            fixture.expected_canonical_summary = *expected;
        if (const auto failure = extractJsonStringField(object, "expected_failure_class"))
            fixture.expected_failure_class = *failure;
        fixtures.push_back(std::move(fixture));
    }

    std::sort(fixtures.begin(), fixtures.end(), [](const FixtureEntry & lhs, const FixtureEntry & rhs) {
        if (lhs.priority != rhs.priority)
            return lhs.priority < rhs.priority;
        return lhs.id < rhs.id;
    });
    return fixtures;
}

std::string parentDirectory(const std::string & path)
{
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string joinPath(const std::string & base, const std::string & relative)
{
    if (relative.empty())
        return base;
    if (!relative.empty() && (relative[0] == '/' || relative[0] == '\\'))
        return relative;
    if (base.empty() || base == ".")
        return relative;
    if (base.back() == '/' || base.back() == '\\')
        return base + relative;
    return base + "/" + relative;
}

std::string resolveInputPath(const std::string & path)
{
    if (path.empty() || isAbsolutePath(path))
        return path;

    std::ifstream local(path);
    if (local.good())
        return path;

    const char * workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY");
    if (!workspace || std::string(workspace).empty())
        return path;

    const std::string candidate = joinPath(workspace, path);
    std::ifstream workspace_file(candidate);
    if (workspace_file.good())
        return candidate;
    return path;
}

SummaryResult summarizeQuery(const std::string & query)
{
    DB::Tokens tokens(query.data(), query.data() + query.size(), 0, true);
    DB::IParser::Pos pos(tokens, 4000, 4000);
    DB::Expected expected;
    DB::ASTPtr node;
    DB::ParserSelectRichQuery parser;

    if (!parser.parse(pos, node, expected))
        return {.code = 1, .classification = "parse_failed"};

    if (!pos->isEnd())
        return {.code = 2, .classification = "trailing_tokens"};

    int union_count = 0;
    std::vector<std::string> set_ops;
    size_t settings_count = 0;
    bool has_format = false;
    const DB::ASTSelectRichQuery * select = nullptr;

    if (auto * set = node ? node->as<DB::ASTSelectSetLite>() : nullptr)
    {
        union_count = static_cast<int>(set->union_modes.size());
        settings_count = set->settings ? set->settings->children.size() : 0;
        has_format = set->has_format;
        if (!set->set_ops.empty())
        {
            for (const auto & op : set->set_ops)
                set_ops.push_back(op);
        }
        else
        {
            for (size_t i = 0; i < set->union_modes.size(); ++i)
                set_ops.push_back("UNION");
        }
        if (!set->children.empty())
            select = set->children.front() ? set->children.front()->as<DB::ASTSelectRichQuery>() : nullptr;
    }
    else
    {
        select = node ? node->as<DB::ASTSelectRichQuery>() : nullptr;
        if (select)
        {
            settings_count = select->settings ? select->settings->children.size() : 0;
            has_format = select->has_format;
        }
    }

    if (!select || !select->expressions)
        return {.code = 3, .classification = "missing_select_ast"};

    std::vector<std::string> join_types;
    std::vector<std::string> join_modes;
    std::vector<std::string> join_localities;
    std::vector<std::string> source_kinds;
    summarizeSource(select->from_source, join_types, join_modes, join_localities, source_kinds);
    bool has_explicit_locality = false;
    for (const auto & locality : join_localities)
    {
        if (locality != "UNSPECIFIED")
        {
            has_explicit_locality = true;
            break;
        }
    }
    size_t distinct_on_count = 0;
    size_t order_nulls = 0;
    size_t order_collate = 0;
    size_t order_fill = 0;
    size_t order_interpolate = 0;
    size_t simple_case_count = 0;
    size_t over_count = 0;
    size_t over_named_count = 0;
    size_t over_inline_count = 0;
    if (select->distinct_on_expressions)
        distinct_on_count = select->distinct_on_expressions->children.size();
    if (select->order_by_list)
    {
        for (const auto & child : select->order_by_list->children)
        {
            const auto * elem = child ? child->as<DB::ASTOrderByElementLite>() : nullptr;
            if (!elem)
                continue;
            if (elem->nulls_order != DB::ASTOrderByElementLite::NullsOrder::Unspecified)
                ++order_nulls;
            if (!elem->collate_name.empty())
                ++order_collate;
            if (elem->with_fill)
                ++order_fill;
            if (elem->interpolate)
                ++order_interpolate;
        }
    }
    for (const auto & projection : select->expressions->children)
        summarizeExpression(projection.get(), simple_case_count, over_count, over_named_count, over_inline_count);

    std::ostringstream out;
    out << "projections=" << select->expressions->children.size()
        << " unions=" << union_count
        << " set_ops=" << joinCsv(set_ops)
        << " settings=" << settings_count
        << " format=" << (has_format ? 1 : 0)
        << " joins=" << join_types.size()
        << " join_types=" << joinCsv(join_types)
        << " join_modes=" << joinCsv(join_modes)
        << " source_kinds=" << joinCsv(source_kinds)
        << " with=" << (select->with_expressions ? 1 : 0)
        << " with_recursive=" << (select->with_recursive ? 1 : 0)
        << " distinct=" << (select->distinct ? 1 : 0)
        << " select_all=" << (select->select_all ? 1 : 0)
        << " distinct_on=" << distinct_on_count
        << " top=" << (select->top_present ? 1 : 0)
        << " top_ties=" << (select->top_with_ties ? 1 : 0)
        << " final=" << (select->from_final ? 1 : 0)
        << " sample=" << (select->sample_size ? 1 : 0)
        << " sample_offset=" << (select->sample_offset ? 1 : 0)
        << " array_join=" << (select->array_join_expressions ? 1 : 0)
        << " array_join_left=" << (select->array_join_is_left ? 1 : 0)
        << " prewhere=" << (select->prewhere_expression ? 1 : 0)
        << " where=" << (select->where_expression ? 1 : 0)
        << " group_by=" << (select->group_by_expressions ? 1 : 0)
        << " group_all=" << (select->group_by_all ? 1 : 0)
        << " group_rollup=" << (select->group_by_with_rollup ? 1 : 0)
        << " group_cube=" << (select->group_by_with_cube ? 1 : 0)
        << " group_totals=" << (select->group_by_with_totals ? 1 : 0)
        << " having=" << (select->having_expression ? 1 : 0)
        << " windows=" << (select->window_list ? select->window_list->children.size() : 0)
        << " qualify=" << (select->qualify_expression ? 1 : 0)
        << " order_by=" << (select->order_by_list ? 1 : 0)
        << " order_nulls=" << order_nulls
        << " order_collate=" << order_collate
        << " order_fill=" << order_fill
        << " order_interpolate=" << order_interpolate
        << " limit=" << (select->limit ? 1 : 0)
        << " limit_by=" << (select->limit_by ? 1 : 0)
        << " limit_by_offset=" << (select->limit_by && select->limit_by->offset_present ? 1 : 0)
        << " limit_ties=" << (select->limit && select->limit->with_ties ? 1 : 0)
        << " limit_by_all=" << (select->limit_by && select->limit_by->by_all ? 1 : 0)
        << " offset=" << (select->limit && select->limit->offset_present ? 1 : 0);

    if (has_explicit_locality)
        out << " join_localities=" << joinCsv(join_localities);

    if (select->limit)
    {
        out << " limit_value=" << select->limit->limit;
        const std::string limit_expr = summarizeLimitExpr(select->limit->limit_expression);
        if (!limit_expr.empty())
            out << " limit_expr=" << limit_expr;
        if (select->limit->offset_present)
        {
            out << " offset_value=" << select->limit->offset;
            const std::string offset_expr = summarizeLimitExpr(select->limit->offset_expression);
            if (!offset_expr.empty())
                out << " offset_expr=" << offset_expr;
        }
    }
    if (select->top_present)
        out << " top_value=" << select->top_count;
    if (select->limit_by)
    {
        out << " limit_by_value=" << select->limit_by->limit;
        const std::string limit_by_expr = summarizeLimitExpr(select->limit_by->limit_expression);
        if (!limit_by_expr.empty())
            out << " limit_by_expr=" << limit_by_expr;
        if (select->limit_by->offset_present)
        {
            out << " limit_by_offset_value=" << select->limit_by->offset;
            const std::string limit_by_offset_expr = summarizeLimitExpr(select->limit_by->offset_expression);
            if (!limit_by_offset_expr.empty())
                out << " limit_by_offset_expr=" << limit_by_offset_expr;
        }
    }
    if (simple_case_count > 0 || over_count > 0)
    {
        out << " simple_case=" << simple_case_count
            << " over=" << over_count
            << " over_named=" << over_named_count
            << " over_inline=" << over_inline_count;
    }
    if (select->grouping_sets_expressions)
        out << " grouping_sets=1";
    if (select->order_by_all)
        out << " order_by_all=1";

    return {.code = 0, .classification = "ok", .summary = out.str()};
}

std::vector<FixtureEntry> selectFixturesForMode(const std::vector<FixtureEntry> & fixtures, const std::string & mode)
{
    std::vector<FixtureEntry> selected;
    for (const auto & fixture : fixtures)
    {
        if (mode == "quick" && fixture.expected_result_kind == "success" && fixture.priority == "P0")
            selected.push_back(fixture);
        else if ((mode == "supported" || mode == "repeat") && fixture.expected_result_kind == "success")
            selected.push_back(fixture);
        else if (mode == "excluded" && fixture.expected_result_kind == "excluded")
            selected.push_back(fixture);
    }
    return selected;
}

int runManifestMode(const std::string & manifest_path, const std::string & mode)
{
    if (mode != "quick" && mode != "supported" && mode != "excluded" && mode != "repeat")
    {
        std::cerr << "unknown mode: " << mode << std::endl;
        return 64;
    }

    const std::string resolved_manifest = resolveInputPath(manifest_path);
    const std::vector<FixtureEntry> fixtures = loadManifestFixtures(resolved_manifest);
    if (fixtures.empty())
    {
        std::cerr << "manifest has no fixtures: " << manifest_path << std::endl;
        return 65;
    }

    const std::string manifest_dir = parentDirectory(resolved_manifest);
    const auto selected = selectFixturesForMode(fixtures, mode);
    if (selected.empty())
    {
        std::cerr << "no fixtures selected for mode: " << mode << std::endl;
        return 66;
    }

    int count = 0;
    for (const auto & fixture : selected)
    {
        const std::string fixture_path = resolveInputPath(joinPath(manifest_dir, fixture.path));
        const std::string query = readFile(fixture_path);
        if (query.empty())
        {
            std::cerr << "classification=io_error fixture=" << fixture.id << " detail=empty_or_unreadable_query_file" << std::endl;
            return 65;
        }

        if (mode == "excluded")
        {
            const auto result = summarizeQuery(query);
            if (result.code == 0)
            {
                std::cerr << "excluded fixture unexpectedly succeeded: " << fixture.id << std::endl;
                return 1;
            }
            if (!fixture.expected_failure_class.empty() && fixture.expected_failure_class != result.classification)
            {
                std::cerr << "excluded fixture class mismatch for " << fixture.id << ": expected "
                          << fixture.expected_failure_class << " got " << result.classification << std::endl;
                return 1;
            }
            ++count;
            continue;
        }

        const auto first = summarizeQuery(query);
        if (first.code != 0)
        {
            std::cerr << "fixture failed: " << fixture.id << " classification=" << first.classification << std::endl;
            return first.code;
        }

        if (first.summary != fixture.expected_canonical_summary)
        {
            std::cerr << "summary mismatch for " << fixture.id << std::endl;
            std::cerr << "expected: " << fixture.expected_canonical_summary << std::endl;
            std::cerr << "actual:   " << first.summary << std::endl;
            return 1;
        }

        if (mode == "repeat")
        {
            for (int i = 1; i < kRepeatRuns; ++i)
            {
                const auto rerun = summarizeQuery(query);
                if (rerun.code != 0)
                {
                    std::cerr << "repeat failed for " << fixture.id << " classification=" << rerun.classification << std::endl;
                    return rerun.code;
                }
                if (rerun.summary != first.summary)
                {
                    std::cerr << "repeat mismatch for " << fixture.id << std::endl;
                    return 1;
                }
            }
        }

        ++count;
    }

    if (mode == "repeat")
        std::cout << "repeat ok: " << count << " fixtures runs=" << kRepeatRuns << std::endl;
    else
        std::cout << mode << " ok: " << count << " fixtures" << std::endl;
    return 0;
}
}

int main(int argc, char ** argv)
{
    std::string query;
    std::string query_file;
    std::string manifest;
    std::string mode;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--query" && i + 1 < argc)
        {
            query = argv[++i];
        }
        else if (arg == "--query-file" && i + 1 < argc)
        {
            query_file = argv[++i];
        }
        else if (arg == "--manifest" && i + 1 < argc)
        {
            manifest = argv[++i];
        }
        else if (arg == "--mode" && i + 1 < argc)
        {
            mode = argv[++i];
        }
        else if (arg == "--help")
        {
            std::cout << "usage: parser_workload_summary [--query <sql>] [--query-file <path>] [--manifest <path> --mode <supported|excluded|quick|repeat>]" << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "unknown argument: " << arg << std::endl;
            return 64;
        }
    }

    if (!manifest.empty())
    {
        if (mode.empty())
        {
            std::cerr << "--mode is required when --manifest is provided" << std::endl;
            return 64;
        }
        if (!query.empty() || !query_file.empty())
        {
            std::cerr << "--query/--query-file cannot be combined with --manifest" << std::endl;
            return 64;
        }
        return runManifestMode(manifest, mode);
    }

    if (!mode.empty())
    {
        std::cerr << "--manifest is required when --mode is provided" << std::endl;
        return 64;
    }

    if (!query_file.empty())
    {
        query = readFile(query_file);
        if (query.empty())
            query = readFile(resolveInputPath(query_file));
        if (query.empty())
        {
            std::cerr << "classification=io_error detail=empty_or_unreadable_query_file" << std::endl;
            return 65;
        }
    }

    if (query.empty())
        query = "SELECT a FROM t";

    const auto result = summarizeQuery(query);
    if (result.code != 0)
    {
        std::cerr << "classification=" << result.classification << std::endl;
        return result.code;
    }

    std::cout << result.summary << std::endl;
    return 0;
}
