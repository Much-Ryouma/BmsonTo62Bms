#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "b2b/core/bmson/Bmson.hpp"

namespace b2b::infra {

class BmsonFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct ParsedBmson {
    core::bmson::Chart chart;
    std::vector<std::string> warnings;
};

[[nodiscard]] ParsedBmson parseBmson(const std::string& text);

}  // namespace b2b::infra
