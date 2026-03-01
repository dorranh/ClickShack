#pragma once

#include "ported_clickhouse/base/types.h"
#include "ported_clickhouse/common/Exception.h"
#include "ported_clickhouse/parsers/IAST_fwd.h"

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
}

class IAST
{
public:
    ASTs children;

    virtual ~IAST() = default;

    virtual String getID(char delim = '_') const = 0;
    virtual ASTPtr clone() const = 0;

    template <typename T>
    void set(T *& field, const ASTPtr & child)
    {
        if (!child)
            return;

        T * cast = dynamic_cast<T *>(child.get());
        if (!cast)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Could not cast AST subtree");

        children.push_back(child);
        field = cast;
    }

    template <typename T>
    T * as()
    {
        return dynamic_cast<T *>(this);
    }

    template <typename T>
    const T * as() const
    {
        return dynamic_cast<const T *>(this);
    }
};

}
