#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace b2b::core {

class PatternError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

enum class PatternScope { Folder, Slice };

struct NameVariables {
    std::string song;
    std::string source;
    std::string id;
    int lane = 0;
    int measure = 0;
    int index = 0;
};

class NamePattern {
public:
    [[nodiscard]] static NamePattern parse(std::string_view text, PatternScope scope);

    [[nodiscard]] std::string render(const NameVariables& variables) const;

private:
    enum class Variable { Song, Source, Id, Lane, Measure, Index };

    struct Token {
        bool literal = true;
        std::string text;
        Variable variable = Variable::Song;
        bool zeroPad = false;
        int width = 0;
        int precision = -1;
    };

    std::vector<Token> tokens_;
};

}  // namespace b2b::core
