#include <openssl/sha.h>

#include "absl/strings/str_cat.h"
#include "double-conversion/double-conversion.h"
#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>
#include "re2/re2.h"

#include <boost/version.hpp>

namespace {

enum class ProbeState {
    kOk,
    kFail,
};

}  // namespace

int main() {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>("clickshack"), 10, hash);

    double value = 3.14159;
    double_conversion::DoubleToStringConverter converter(
        double_conversion::DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN,
        "inf",
        "nan",
        'e',
        -6,
        21,
        6,
        0);
    char buffer[64];
    double_conversion::StringBuilder builder(buffer, sizeof(buffer));
    converter.ToShortest(value, &builder);

    RE2 re("click.*");
    const bool matched = RE2::FullMatch("clickshack", re);

    auto status = ProbeState::kOk;
    const auto status_name = magic_enum::enum_name(status);

    auto message = absl::StrCat("probe=", matched ? "ok" : "fail", ";enum=", status_name);
    auto out = fmt::format("{};boost={};double={};sha0={}", message, BOOST_VERSION, builder.Finalize(), hash[0]);

    return out.empty() ? 1 : 0;
}
