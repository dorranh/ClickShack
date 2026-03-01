#pragma once

#include <memory>
#include <vector>

namespace DB
{

class IAST;

using ASTPtr = std::shared_ptr<IAST>;
using ASTs = std::vector<ASTPtr>;

template <typename T, typename ... Args>
constexpr std::shared_ptr<T> make_intrusive(Args && ... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

}
