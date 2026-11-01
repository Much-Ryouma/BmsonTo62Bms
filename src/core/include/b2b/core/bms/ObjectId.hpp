#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace b2b::core {

enum class IdBase { Base36 = 36, Base62 = 62 };

[[nodiscard]] constexpr int maxObjectId(IdBase base) noexcept {
    return static_cast<int>(base) * static_cast<int>(base) - 1;
}

[[nodiscard]] std::string formatObjectId(int value, IdBase base);

[[nodiscard]] std::optional<int> parseObjectId(std::string_view text, IdBase base);

}  // namespace b2b::core
