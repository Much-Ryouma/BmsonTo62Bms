#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace b2b::core {

inline constexpr std::size_t kMaxFileNameLength = 64;

[[nodiscard]] std::size_t codePointCount(std::string_view utf8);

[[nodiscard]] std::string truncateCodePoints(std::string_view utf8, std::size_t limit);

[[nodiscard]] std::string fileStemOf(std::string_view path);

[[nodiscard]] std::string sanitizeFileName(std::string_view name);

[[nodiscard]] std::string finishFolderName(std::string_view rendered);

struct SoundNames {
    std::vector<std::string> paths;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

[[nodiscard]] SoundNames finishSoundNames(const std::vector<std::string>& renderedStems, const std::string& folder,
                                          const std::string& extension);

}  // namespace b2b::core
