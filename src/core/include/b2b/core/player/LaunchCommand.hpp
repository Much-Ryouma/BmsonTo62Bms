#pragma once

#include <string>
#include <vector>

namespace b2b::core {

inline constexpr const char* kDefaultLaunchTemplate = R"("{exe}" "{file}")";

struct LaunchCommand {
    std::string program;
    std::vector<std::string> arguments;

    friend bool operator==(const LaunchCommand&, const LaunchCommand&) = default;
};

[[nodiscard]] std::vector<std::string> splitCommandLine(const std::string& commandLine);

[[nodiscard]] LaunchCommand buildLaunchCommand(const std::string& commandTemplate, const std::string& executable,
                                               const std::string& file);

}  // namespace b2b::core
