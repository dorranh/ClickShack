#pragma once

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"
#include "ported_clickhouse/parsers/IAST.h"

namespace DB
{

class ASTSelectRichQuery : public IAST
{
public:
    ASTExpressionList * with_expressions = nullptr;
    bool with_recursive = false;
    bool distinct = false;
    bool select_all = false;
    ASTExpressionList * distinct_on_expressions = nullptr;
    bool top_present = false;
    String top_count;
    bool top_with_ties = false;
    ASTExpressionList * expressions = nullptr;
    IAST * from_source = nullptr;
    bool from_final = false;
    IAST * sample_size = nullptr;
    IAST * sample_offset = nullptr;
    ASTExpressionList * array_join_expressions = nullptr;
    bool array_join_is_left = false;
    IAST * prewhere_expression = nullptr;
    IAST * where_expression = nullptr;
    ASTExpressionList * group_by_expressions = nullptr;
    bool group_by_all = false;
    bool group_by_with_rollup = false;
    bool group_by_with_cube = false;
    bool group_by_with_totals = false;
    IAST * having_expression = nullptr;
    ASTWindowListLite * window_list = nullptr;
    IAST * qualify_expression = nullptr;
    ASTOrderByListLite * order_by_list = nullptr;
    ASTLimitLite * limit = nullptr;
    ASTLimitByLite * limit_by = nullptr;
    ASTExpressionList * settings = nullptr;
    bool has_format = false;
    String format_name;

    String getID(char delim) const override
    {
        return "SelectRichQuery" + String(1, delim) + "core";
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTSelectRichQuery>();
        res->with_recursive = with_recursive;
        res->distinct = distinct;
        res->select_all = select_all;
        res->top_present = top_present;
        res->top_count = top_count;
        res->top_with_ties = top_with_ties;
        if (with_expressions)
            res->set(res->with_expressions, with_expressions->clone());
        if (distinct_on_expressions)
            res->set(res->distinct_on_expressions, distinct_on_expressions->clone());
        if (expressions)
            res->set(res->expressions, expressions->clone());
        if (from_source)
            res->set(res->from_source, from_source->clone());
        res->from_final = from_final;
        if (sample_size)
            res->set(res->sample_size, sample_size->clone());
        if (sample_offset)
            res->set(res->sample_offset, sample_offset->clone());
        if (array_join_expressions)
            res->set(res->array_join_expressions, array_join_expressions->clone());
        res->array_join_is_left = array_join_is_left;
        if (prewhere_expression)
            res->set(res->prewhere_expression, prewhere_expression->clone());
        if (where_expression)
            res->set(res->where_expression, where_expression->clone());
        if (group_by_expressions)
            res->set(res->group_by_expressions, group_by_expressions->clone());
        res->group_by_all = group_by_all;
        res->group_by_with_rollup = group_by_with_rollup;
        res->group_by_with_cube = group_by_with_cube;
        res->group_by_with_totals = group_by_with_totals;
        if (having_expression)
            res->set(res->having_expression, having_expression->clone());
        if (window_list)
            res->set(res->window_list, window_list->clone());
        if (qualify_expression)
            res->set(res->qualify_expression, qualify_expression->clone());
        if (order_by_list)
            res->set(res->order_by_list, order_by_list->clone());
        if (limit)
            res->set(res->limit, limit->clone());
        if (limit_by)
            res->set(res->limit_by, limit_by->clone());
        if (settings)
            res->set(res->settings, settings->clone());
        res->has_format = has_format;
        res->format_name = format_name;
        return res;
    }
};

}
