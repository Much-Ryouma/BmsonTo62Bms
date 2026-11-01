#pragma once

#include <CLI/CLI.hpp>

namespace b2b::cli {

enum class ExitCode : int {
    Success = 0,
    ConversionError = 1,
    UsageError = 2,
};

[[nodiscard]] constexpr int toInt(ExitCode code) noexcept {
    return static_cast<int>(code);
}

struct CommandResult {
    int exitCode = toInt(ExitCode::Success);
};

void registerSlicesCommand(CLI::App& app, CommandResult& result);
void registerConvertCommand(CLI::App& app, CommandResult& result);

}  // namespace b2b::cli
