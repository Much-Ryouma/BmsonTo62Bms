#include "b2b/core/naming/NamePattern.hpp"

#include <algorithm>
#include <cctype>
#include <map>

#include "b2b/core/naming/FileNames.hpp"

namespace b2b::core {

namespace {

constexpr int kMaxWidth = 32;

bool isAllDigits(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

int parseCount(std::string_view digits, std::string_view variable) {
    if (digits.size() > 2 || std::stoi(std::string(digits)) > kMaxWidth) {
        throw PatternError("{" + std::string(variable) + "} の桁数・文字数は " + std::to_string(kMaxWidth) +
                           " までにしてください");
    }
    return std::stoi(std::string(digits));
}

std::string padded(int value, bool zeroPad, int width) {
    std::string digits = std::to_string(value);
    if (static_cast<int>(digits.size()) < width) {
        digits.insert(0, static_cast<std::size_t>(width) - digits.size(), zeroPad ? '0' : ' ');
    }
    return digits;
}

}  // namespace

NamePattern NamePattern::parse(std::string_view text, PatternScope scope) {
    static const std::map<std::string, Variable, std::less<>> kVariables = {
        {"song", Variable::Song},   {"src", Variable::Source},       {"id", Variable::Id},
        {"lane", Variable::Lane},   {"measure", Variable::Measure},  {"index", Variable::Index}};

    NamePattern pattern;
    std::string literal;
    auto flushLiteral = [&] {
        if (!literal.empty()) {
            pattern.tokens_.push_back(Token{true, literal});
            literal.clear();
        }
    };

    for (std::size_t at = 0; at < text.size();) {
        const char c = text[at];
        if (c == '}' ) {
            if (at + 1 < text.size() && text[at + 1] == '}') {
                literal.push_back('}');
                at += 2;
                continue;
            }
            throw PatternError("対応する { がない } があります（} そのものを書くときは }} と書きます）");
        }
        if (c != '{') {
            literal.push_back(c);
            ++at;
            continue;
        }
        if (at + 1 < text.size() && text[at + 1] == '{') {
            literal.push_back('{');
            at += 2;
            continue;
        }
        const std::size_t close = text.find('}', at);
        if (close == std::string_view::npos) {
            throw PatternError("閉じていない { があります（{ そのものを書くときは {{ と書きます）");
        }
        const std::string_view body = text.substr(at + 1, close - at - 1);
        const std::size_t colon = body.find(':');
        const std::string_view name = body.substr(0, colon);
        std::string_view spec = colon == std::string_view::npos ? std::string_view() : body.substr(colon + 1);

        const auto found = kVariables.find(name);
        if (found == kVariables.end()) {
            throw PatternError("知らない変数です: {" + std::string(name) +
                               "}（使えるのは {song} {src} {lane} {measure} {index} {id}）");
        }
        if (scope == PatternScope::Folder && found->second != Variable::Song) {
            throw PatternError("サブフォルダの名前に使える変数は {song} だけです");
        }

        Token token;
        token.literal = false;
        token.variable = found->second;
        const bool isText = token.variable == Variable::Song || token.variable == Variable::Source ||
                            token.variable == Variable::Id;
        if (spec.starts_with('%')) {
            spec.remove_prefix(1);
        }
        if (isText) {
            if (spec.ends_with('s')) {
                spec.remove_suffix(1);
            }
            if (!spec.empty()) {
                if (spec.size() < 2 || spec.front() != '.' || !isAllDigits(spec.substr(1))) {
                    throw PatternError("{" + std::string(name) + "} に使える書式は .文字数 だけです（例: {" +
                                       std::string(name) + ":.16}）");
                }
                token.precision = parseCount(spec.substr(1), name);
            }
        } else {
            if (spec.ends_with('d')) {
                spec.remove_suffix(1);
            }
            if (!isAllDigits(spec)) {
                throw PatternError("{" + std::string(name) + "} に使える書式は 0 と桁数だけです（例: {" +
                                   std::string(name) + ":04}）");
            }
            token.zeroPad = spec.starts_with('0');
            const std::string_view digits = token.zeroPad ? spec.substr(1) : spec;
            token.width = digits.empty() ? 0 : parseCount(digits, name);
        }
        flushLiteral();
        pattern.tokens_.push_back(token);
        at = close + 1;
    }
    flushLiteral();

    if (scope == PatternScope::Slice && pattern.tokens_.empty()) {
        throw PatternError("ファイル名のパターンが空です");
    }
    return pattern;
}

std::string NamePattern::render(const NameVariables& variables) const {
    std::string result;
    for (const auto& token : tokens_) {
        if (token.literal) {
            result += token.text;
            continue;
        }
        switch (token.variable) {
            case Variable::Song:
            case Variable::Source:
            case Variable::Id: {
                const std::string& value = token.variable == Variable::Song   ? variables.song
                                           : token.variable == Variable::Source ? variables.source
                                                                                : variables.id;
                result += token.precision < 0 ? value : truncateCodePoints(value, static_cast<std::size_t>(token.precision));
                break;
            }
            case Variable::Lane: result += padded(variables.lane, token.zeroPad, token.width); break;
            case Variable::Measure: result += padded(variables.measure, token.zeroPad, token.width); break;
            case Variable::Index: result += padded(variables.index, token.zeroPad, token.width); break;
        }
    }
    return result;
}

}  // namespace b2b::core
