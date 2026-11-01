#pragma once

#include <string>
#include <vector>

namespace b2b::app {

enum class Severity { Error, Warning, Info };

struct Message {
    Severity severity = Severity::Info;
    std::string text;

    friend bool operator==(const Message&, const Message&) = default;
};

[[nodiscard]] bool containsError(const std::vector<Message>& messages);

}  // namespace b2b::app
