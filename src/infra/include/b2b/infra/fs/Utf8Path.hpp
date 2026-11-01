#pragma once

#include <filesystem>
#include <string>

namespace b2b::infra {

[[nodiscard]] std::filesystem::path pathFromUtf8(const std::string& utf8);

[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);

void writeTextFileAtomically(const std::filesystem::path& path, const std::string& content);

[[nodiscard]] std::filesystem::path executableDirectory();

}  // namespace b2b::infra
