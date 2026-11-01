#include "b2b/infra/text/TextEncoding.hpp"

#include <optional>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace b2b::infra {

namespace {

constexpr char kReplacement = '?';
constexpr unsigned int kShiftJisCodePage = 932;

std::size_t sequenceLength(unsigned char lead) {
    if (lead < 0x80) { return 1; }
    if ((lead >> 5) == 0x6) { return 2; }
    if ((lead >> 4) == 0xE) { return 3; }
    if ((lead >> 3) == 0x1E) { return 4; }
    return 0;
}

bool isCompleteSequence(std::string_view text, std::size_t at, std::size_t length) {
    if (length == 0 || at + length > text.size()) {
        return false;
    }
    for (std::size_t index = 1; index < length; ++index) {
        if ((static_cast<unsigned char>(text[at + index]) >> 6) != 0x2) {
            return false;
        }
    }
    return true;
}

#if defined(_WIN32)

class ShiftJisConverter {
public:
    std::optional<std::string> convert(std::string_view character) const {
        wchar_t wide[4];
        const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, character.data(),
                                                   static_cast<int>(character.size()), wide, 4);
        if (wideLength <= 0) {
            return std::nullopt;
        }
        char narrow[8];
        BOOL usedDefault = FALSE;
        // Why not 既定のフラグで変換する: 表せない文字が似た文字に置き換えられ（ベストフィット）、警告を出せないため
        const int narrowLength = WideCharToMultiByte(kShiftJisCodePage, WC_NO_BEST_FIT_CHARS, wide, wideLength, narrow,
                                                     static_cast<int>(sizeof(narrow)), nullptr, &usedDefault);
        if (narrowLength <= 0 || usedDefault) {
            return std::nullopt;
        }
        return std::string(narrow, static_cast<std::size_t>(narrowLength));
    }
};

#else

class ShiftJisConverter {
public:
    ShiftJisConverter() : descriptor_(iconv_open("CP932", "UTF-8")) {
        if (descriptor_ == reinterpret_cast<iconv_t>(-1)) {
            throw std::runtime_error("Shift_JIS (CP932) への変換を準備できません");
        }
    }
    ~ShiftJisConverter() { iconv_close(descriptor_); }
    ShiftJisConverter(const ShiftJisConverter&) = delete;
    ShiftJisConverter& operator=(const ShiftJisConverter&) = delete;

    std::optional<std::string> convert(std::string_view character) const {
        iconv(descriptor_, nullptr, nullptr, nullptr, nullptr);
        std::string input(character);
        char* in = input.data();
        std::size_t inLeft = input.size();
        char narrow[8];
        char* out = narrow;
        std::size_t outLeft = sizeof(narrow);
        if (iconv(descriptor_, &in, &inLeft, &out, &outLeft) == static_cast<std::size_t>(-1) || inLeft != 0) {
            return std::nullopt;
        }
        return std::string(narrow, sizeof(narrow) - outLeft);
    }

private:
    iconv_t descriptor_;
};

#endif

EncodedText encodeShiftJis(std::string_view utf8) {
    const ShiftJisConverter converter;
    EncodedText encoded;
    encoded.bytes.reserve(utf8.size());
    for (std::size_t at = 0; at < utf8.size();) {
        const auto lead = static_cast<unsigned char>(utf8[at]);
        if (lead < 0x80) {
            encoded.bytes.push_back(static_cast<char>(lead));
            ++at;
            continue;
        }
        const std::size_t length = sequenceLength(lead);
        if (!isCompleteSequence(utf8, at, length)) {
            encoded.bytes.push_back(kReplacement);
            encoded.unrepresentable.emplace_back(1, static_cast<char>(lead));
            ++at;
            continue;
        }
        const std::string_view character = utf8.substr(at, length);
        if (const auto converted = converter.convert(character)) {
            encoded.bytes += *converted;
        } else {
            encoded.bytes.push_back(kReplacement);
            encoded.unrepresentable.emplace_back(character);
        }
        at += length;
    }
    return encoded;
}

}  // namespace

EncodedText encodeText(std::string_view utf8, TextEncoding encoding) {
    if (encoding == TextEncoding::Utf8) {
        return EncodedText{std::string(utf8), {}};
    }
    return encodeShiftJis(utf8);
}

ReplacedText replaceUnrepresentable(std::string_view utf8, TextEncoding encoding, char replacement) {
    if (encoding == TextEncoding::Utf8) {
        return ReplacedText{std::string(utf8), 0};
    }
    const ShiftJisConverter converter;
    ReplacedText result;
    for (std::size_t at = 0; at < utf8.size();) {
        const std::size_t length = sequenceLength(static_cast<unsigned char>(utf8[at]));
        if (length == 1) {
            result.text.push_back(utf8[at]);
            ++at;
            continue;
        }
        if (!isCompleteSequence(utf8, at, length)) {
            result.text.push_back(replacement);
            ++result.replaced;
            ++at;
            continue;
        }
        const std::string_view character = utf8.substr(at, length);
        if (converter.convert(character)) {
            result.text += character;
        } else {
            result.text.push_back(replacement);
            ++result.replaced;
        }
        at += length;
    }
    return result;
}

std::string_view encodingName(TextEncoding encoding) {
    return encoding == TextEncoding::Utf8 ? "UTF-8" : "Shift_JIS";
}

}  // namespace b2b::infra
