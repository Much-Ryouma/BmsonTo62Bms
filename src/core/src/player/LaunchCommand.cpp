#include "b2b/core/player/LaunchCommand.hpp"

namespace b2b::core {

namespace {

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
}

}  // namespace

std::vector<std::string> splitCommandLine(const std::string& commandLine) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool hasToken = false;
    for (const char character : commandLine) {
        if (character == '"') {
            quoted = !quoted;
            hasToken = true;
        } else if ((character == ' ' || character == '\t') && !quoted) {
            if (hasToken) {
                tokens.push_back(current);
                current.clear();
                hasToken = false;
            }
        } else {
            current.push_back(character);
            hasToken = true;
        }
    }
    if (hasToken) {
        tokens.push_back(current);
    }
    return tokens;
}

// Why not 置き換えてから空白で区切る: 曲のフォルダ名に空白が入っていると、置き換えたパスが途中で切れて
// 別の引数になってしまうため、先に区切ってから各引数の中だけを置き換える
LaunchCommand buildLaunchCommand(const std::string& commandTemplate, const std::string& executable,
                                 const std::string& file) {
    std::vector<std::string> tokens = splitCommandLine(commandTemplate);
    for (auto& token : tokens) {
        replaceAll(token, "{exe}", executable);
        replaceAll(token, "{file}", file);
    }
    LaunchCommand command;
    if (!tokens.empty()) {
        command.program = tokens.front();
        command.arguments.assign(tokens.begin() + 1, tokens.end());
    }
    return command;
}

}  // namespace b2b::core
