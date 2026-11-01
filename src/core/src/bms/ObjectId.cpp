#include "b2b/core/bms/ObjectId.hpp"

#include <stdexcept>

namespace b2b::core {

namespace {

constexpr std::string_view kDigits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

std::optional<int> digitValue(char c, IdBase base) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'z') {
        // Why not 36進数でも別の値にする: 36進数の BMS は大文字と小文字を区別しないため、小文字は大文字と同じ値として読む
        return base == IdBase::Base62 ? c - 'a' + 36 : c - 'a' + 10;
    }
    return std::nullopt;
}

}  // namespace

std::string formatObjectId(int value, IdBase base) {
    if (value < 1 || value > maxObjectId(base)) {
        throw std::out_of_range("object id out of range: " + std::to_string(value));
    }
    const int radix = static_cast<int>(base);
    return std::string{kDigits[static_cast<std::size_t>(value / radix)], kDigits[static_cast<std::size_t>(value % radix)]};
}

std::optional<int> parseObjectId(std::string_view text, IdBase base) {
    if (text.size() != 2) {
        return std::nullopt;
    }
    const auto high = digitValue(text[0], base);
    const auto low = digitValue(text[1], base);
    if (!high || !low) {
        return std::nullopt;
    }
    return *high * static_cast<int>(base) + *low;
}

}  // namespace b2b::core
