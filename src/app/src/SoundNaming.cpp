#include "b2b/app/SoundNaming.hpp"

#include <optional>

#include "b2b/core/naming/FileNames.hpp"
#include "b2b/core/naming/NamePattern.hpp"
#include "b2b/core/timing/MeasureLayout.hpp"

namespace b2b::app {

namespace {

std::optional<core::NamePattern> parsePattern(const std::string& text, core::PatternScope scope, const char* label,
                                              std::vector<Message>& messages) {
    try {
        return core::NamePattern::parse(text, scope);
    } catch (const core::PatternError& error) {
        messages.push_back({Severity::Error, std::string(label) + "のパターンが正しくありません: " + error.what()});
        return std::nullopt;
    }
}

}  // namespace

bool containsError(const std::vector<Message>& messages) {
    for (const auto& message : messages) {
        if (message.severity == Severity::Error) {
            return true;
        }
    }
    return false;
}

bool NamingResult::hasErrors() const {
    return containsError(messages);
}

NamingResult checkNamingPatterns(const NamingOptions& options) {
    NamingResult result;
    static_cast<void>(parsePattern(options.folderPattern, core::PatternScope::Folder, "サブフォルダ", result.messages));
    static_cast<void>(parsePattern(options.keyPattern, core::PatternScope::Slice, "パラ音のファイル名", result.messages));
    static_cast<void>(parsePattern(options.bgmPattern, core::PatternScope::Slice, "BGM 音のファイル名", result.messages));
    return result;
}

NamingResult nameSounds(const core::bmson::Chart& chart, const core::SlicePlan& plan, core::IdBase base,
                        const NamingOptions& options, const std::string& extension, infra::TextEncoding encoding) {
    NamingResult result;
    const auto folderPattern = parsePattern(options.folderPattern, core::PatternScope::Folder, "サブフォルダ", result.messages);
    const auto keyPattern = parsePattern(options.keyPattern, core::PatternScope::Slice, "パラ音のファイル名", result.messages);
    const auto bgmPattern = parsePattern(options.bgmPattern, core::PatternScope::Slice, "BGM 音のファイル名", result.messages);
    if (!folderPattern || !keyPattern || !bgmPattern) {
        return result;
    }

    int replaced = 0;
    auto representable = [&](const std::string& text) {
        // Why not 表せない文字のまま名前にする: #WAV に書いたファイル名が BMS の文字コードで表せないと、プレイヤーがファイルを見つけられないため
        infra::ReplacedText filtered = infra::replaceUnrepresentable(text, encoding, '_');
        replaced += filtered.replaced;
        return filtered.text;
    };

    core::NameVariables songOnly;
    songOnly.song = chart.info.title;
    const std::string folder = core::finishFolderName(representable(folderPattern->render(songOnly)));

    const core::MeasureLayout layout(chart.info.resolution, chart.lines);
    std::vector<std::string> stems;
    stems.reserve(plan.slices.size());
    for (std::size_t index = 0; index < plan.slices.size(); ++index) {
        const core::Slice& slice = plan.slices[index];
        core::NameVariables variables;
        variables.song = chart.info.title;
        variables.source = core::fileStemOf(slice.source);
        variables.id = core::formatObjectId(static_cast<int>(index) + 1, base);
        variables.lane = slice.firstLane;
        variables.measure = layout.locate(slice.firstPulse).measure;
        variables.index = slice.indexInKind;
        const auto& pattern = slice.kind == core::SliceKind::Key ? *keyPattern : *bgmPattern;
        stems.push_back(representable(pattern.render(variables)));
    }
    if (replaced > 0) {
        result.messages.push_back({Severity::Warning, std::string(infra::encodingName(encoding)) + " で表せない文字 " +
                                                          std::to_string(replaced) + " 個を、音声ファイルの名前では _ に置き換えました"});
    }

    // Why not #WAV にサブフォルダを含める: サブフォルダは書き出したファイルを置く場所を分けるためだけに使い、
    // BMS の定義はファイル名だけにする（利用者の指定）
    core::SoundNames names = core::finishSoundNames(stems, "", extension);
    result.folder = folder;
    for (auto& warning : names.warnings) {
        result.messages.push_back({Severity::Warning, std::move(warning)});
    }
    if (!names.errors.empty()) {
        result.messages.push_back({Severity::Error, names.errors.front()});
    }
    result.paths = std::move(names.paths);
    return result;
}

}  // namespace b2b::app
