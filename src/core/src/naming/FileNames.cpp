#include "b2b/core/naming/FileNames.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>

namespace b2b::core {

namespace {

constexpr std::string_view kForbidden = "\\/:*?\"<>|";
constexpr std::array<std::string_view, 22> kReservedNames = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
    "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

bool isContinuationByte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

std::string upperAscii(std::string_view text) {
    std::string upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
    return upper;
}

std::string lowerAscii(std::string_view text) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return lower;
}

void trimTrailingDotsAndSpaces(std::string& name) {
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) {
        name.pop_back();
    }
}

}  // namespace

std::size_t codePointCount(std::string_view utf8) {
    return static_cast<std::size_t>(std::count_if(utf8.begin(), utf8.end(), [](char c) { return !isContinuationByte(c); }));
}

std::string truncateCodePoints(std::string_view utf8, std::size_t limit) {
    std::size_t count = 0;
    for (std::size_t at = 0; at < utf8.size(); ++at) {
        if (!isContinuationByte(utf8[at])) {
            if (count == limit) {
                return std::string(utf8.substr(0, at));
            }
            ++count;
        }
    }
    return std::string(utf8);
}

std::string fileStemOf(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string_view::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return std::string(name);
}

std::string sanitizeFileName(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        const bool control = static_cast<unsigned char>(c) < 0x20;
        result.push_back(control || kForbidden.find(c) != std::string_view::npos ? '_' : c);
    }
    trimTrailingDotsAndSpaces(result);
    const std::string upper = upperAscii(result);
    // Why not そのまま使う: Windows では CON や NUL などはデバイス名として予約されていて、ファイルを作れないため
    if (std::find(kReservedNames.begin(), kReservedNames.end(), upper) != kReservedNames.end()) {
        result.push_back('_');
    }
    return result;
}

std::string finishFolderName(std::string_view rendered) {
    std::string folder = truncateCodePoints(sanitizeFileName(rendered), kMaxFileNameLength);
    trimTrailingDotsAndSpaces(folder);
    return folder;
}

SoundNames finishSoundNames(const std::vector<std::string>& renderedStems, const std::string& folder,
                            const std::string& extension) {
    SoundNames names;
    std::set<std::string> taken;
    int truncated = 0;
    int renamed = 0;
    std::string truncatedExample;
    std::string renamedExample;

    for (const auto& rendered : renderedStems) {
        std::string stem = sanitizeFileName(rendered);
        if (codePointCount(stem) > kMaxFileNameLength) {
            stem = truncateCodePoints(stem, kMaxFileNameLength);
            trimTrailingDotsAndSpaces(stem);
            if (truncated++ == 0) {
                truncatedExample = stem;
            }
        }
        if (stem.empty()) {
            names.errors.push_back("ファイル名が空になるスライスがあります。名前のパターンを見直してください");
            names.paths.emplace_back();
            continue;
        }
        // Why not 大文字と小文字を区別して比べる: Windows のファイル名は区別しないため、Aa と aA が同じファイルになってしまう
        std::string unique = stem;
        for (int suffix = 2; taken.contains(lowerAscii(unique)); ++suffix) {
            unique = stem + "_" + std::to_string(suffix);
        }
        if (unique != stem && renamed++ == 0) {
            renamedExample = stem + " → " + unique;
        }
        taken.insert(lowerAscii(unique));
        const std::string file = unique + "." + extension;
        names.paths.push_back(folder.empty() ? file : folder + "/" + file);
    }

    if (truncated > 0) {
        names.warnings.push_back("ファイル名が " + std::to_string(kMaxFileNameLength) + " 文字を超えたため、" +
                                 std::to_string(truncated) + " 個を切り詰めました（例: " + truncatedExample + "）");
    }
    if (renamed > 0) {
        names.warnings.push_back("ファイル名がぶつかったため、" + std::to_string(renamed) + " 個に _2 などを付けました（例: " +
                                 renamedExample + "）");
    }
    return names;
}

}  // namespace b2b::core
