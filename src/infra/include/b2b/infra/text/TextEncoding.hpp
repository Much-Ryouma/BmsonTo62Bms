#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace b2b::infra {

enum class TextEncoding { ShiftJis, Utf8 };

struct EncodedText {
    std::string bytes;
    std::vector<std::string> unrepresentable;
};

[[nodiscard]] EncodedText encodeText(std::string_view utf8, TextEncoding encoding);

[[nodiscard]] std::string_view encodingName(TextEncoding encoding);

struct ReplacedText {
    std::string text;
    int replaced = 0;
};

[[nodiscard]] ReplacedText replaceUnrepresentable(std::string_view utf8, TextEncoding encoding, char replacement);

}  // namespace b2b::infra
