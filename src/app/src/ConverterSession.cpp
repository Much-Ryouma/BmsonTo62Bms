#include "b2b/app/ConverterSession.hpp"

#include <algorithm>

namespace b2b::app {

namespace {

const std::filesystem::path kNoPath;
const Conversion kNoConversion;

}  // namespace

ConverterSession::ConverterSession(const ChartSource& charts, const AudioSource& audio, const AudioCodec& codec,
                                   const FileSink& files)
    : audio_(audio), service_(charts, audio_, codec, files), preview_(codec) {}

void ConverterSession::openChart(const std::filesystem::path& chartPath) {
    openCharts({ChartRequest{chartPath, std::nullopt}}, 0);
}

void ConverterSession::openCharts(const std::vector<ChartRequest>& requests, std::size_t baseIndex) {
    // Why not 前の結果を使い回す: 同じ名前の音声でも、別の bmson のフォルダでは別のファイルなので、読み直す
    audio_.clear();
    preview_.clear();
    requests_ = requests;
    baseIndex_ = baseIndex < requests.size() ? baseIndex : 0;
    selected_ = baseIndex_;
    refresh();
    outputs_.clear();
    for (std::size_t index = 0; index < requests_.size(); ++index) {
        const std::string modeHint = index < set_.charts.size() ? set_.charts[index].chart.info.modeHint : std::string();
        outputs_.push_back(ConvertService::defaultOutputPath(requests_[index].path, modeHint));
    }
}

void ConverterSession::selectChart(std::size_t index) {
    if (index < requests_.size()) {
        selected_ = index;
    }
}

void ConverterSession::setOutputPath(const std::filesystem::path& outputPath) {
    if (selected_ < outputs_.size()) {
        outputs_[selected_] = outputPath;
    }
    exported_ = false;
}

void ConverterSession::setDifficulty(std::optional<int> difficulty) {
    if (selected_ < requests_.size()) {
        requests_[selected_].difficulty = difficulty;
        refresh();
    }
}

void ConverterSession::setOptions(const ConvertOptions& options) {
    options_ = options;
    refresh();
}

void ConverterSession::refresh() {
    exported_ = false;
    if (!hasChart()) {
        set_ = ConversionSet{};
        return;
    }
    set_ = service_.prepareSet(requests_, baseIndex_, options_);
}

const std::filesystem::path& ConverterSession::chartPath() const {
    return selected_ < requests_.size() ? requests_[selected_].path : kNoPath;
}

const std::filesystem::path& ConverterSession::outputPath() const {
    return selected_ < outputs_.size() ? outputs_[selected_] : kNoPath;
}

std::optional<int> ConverterSession::difficulty() const {
    return selected_ < requests_.size() ? requests_[selected_].difficulty : std::nullopt;
}

const Conversion& ConverterSession::conversion() const {
    return selected_ < set_.charts.size() ? set_.charts[selected_] : kNoConversion;
}

bool ConverterSession::canExport() const {
    return hasChart() && !set_.hasErrors() &&
           std::none_of(outputs_.begin(), outputs_.end(), [](const auto& output) { return output.empty(); });
}

ExportAllOutcome ConverterSession::exportAll(const ExportProgress& progress) {
    ExportAllOutcome outcome;
    if (!canExport()) {
        outcome.messages.push_back({Severity::Error, "変換に失敗しているため書き出せません"});
        return outcome;
    }
    const auto folderOf = [](const std::filesystem::path& path) {
        return std::filesystem::absolute(path).parent_path().lexically_normal();
    };
    for (const auto& output : outputs_) {
        if (folderOf(output) != folderOf(outputs_.front())) {
            // Why not BMS ごとに音声を書き出す: 音声ファイルはすべての BMS で共有するので、BMS が同じフォルダにないと参照できない
            outcome.messages.push_back({Severity::Error, "譜面セットの BMS は、すべて同じフォルダに書き出してください"});
            return outcome;
        }
    }
    const ExportOutcome sounds = service_.exportSounds(set_.base(), outputs_[baseIndex_], options_, progress);
    outcome.messages = sounds.messages;
    if (sounds.hasErrors()) {
        return outcome;
    }
    bool written = true;
    for (std::size_t index = 0; index < set_.charts.size(); ++index) {
        const WriteOutcome bms = service_.writeBms(set_.charts[index], outputs_[index], options_.encoding);
        outcome.messages.insert(outcome.messages.end(), bms.messages.begin(), bms.messages.end());
        written = written && bms.written;
    }
    outcome.succeeded = written;
    exported_ = written;
    return outcome;
}

PreviewResult ConverterSession::previewSlice(std::size_t index, PreviewSource source) {
    if (source == PreviewSource::Exported && !exported_) {
        return PreviewResult{std::nullopt, "まだ書き出していません"};
    }
    return preview_.slice(conversion(), index, options_, source, outputPath());
}

PreviewResult ConverterSession::previewSong(PreviewSource source, int sampleRate) {
    if (source == PreviewSource::Exported && !exported_) {
        return PreviewResult{std::nullopt, "まだ書き出していません"};
    }
    return preview_.song(conversion(), options_, source, outputPath(), sampleRate);
}

LaunchOutcome ConverterSession::launchPlayer(const PlayerSettings& settings, ProcessLauncher& launcher) const {
    return app::launchPlayer(settings, outputPath(), launcher);
}

}  // namespace b2b::app
