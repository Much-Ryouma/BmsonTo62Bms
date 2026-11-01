#include "b2b/app/ConverterSession.hpp"

namespace b2b::app {

ConverterSession::ConverterSession(const ChartSource& charts, const AudioSource& audio, const AudioCodec& codec,
                                   const FileSink& files)
    : audio_(audio), service_(charts, audio_, codec, files), preview_(codec) {}

void ConverterSession::openChart(const std::filesystem::path& chartPath) {
    // Why not 前の結果を使い回す: 同じ名前の音声でも、別の bmson のフォルダでは別のファイルなので、読み直す
    audio_.clear();
    preview_.clear();
    chartPath_ = chartPath;
    outputPath_ = ConvertService::defaultOutputPath(chartPath);
    refresh();
}

void ConverterSession::setOutputPath(const std::filesystem::path& outputPath) {
    outputPath_ = outputPath;
    exported_ = false;
}

void ConverterSession::setOptions(const ConvertOptions& options) {
    options_ = options;
    refresh();
}

void ConverterSession::refresh() {
    exported_ = false;
    if (!hasChart()) {
        conversion_ = Conversion{};
        return;
    }
    conversion_ = service_.prepare(chartPath_, options_);
}

bool ConverterSession::canExport() const {
    return hasChart() && !conversion_.hasErrors() && !outputPath_.empty();
}

ExportAllOutcome ConverterSession::exportAll(const ExportProgress& progress) {
    ExportAllOutcome outcome;
    if (!canExport()) {
        outcome.messages.push_back({Severity::Error, "変換に失敗しているため書き出せません"});
        return outcome;
    }
    const ExportOutcome sounds = service_.exportSounds(conversion_, outputPath_, options_, progress);
    outcome.messages = sounds.messages;
    if (sounds.hasErrors()) {
        return outcome;
    }
    const WriteOutcome bms = service_.writeBms(conversion_, outputPath_, options_.encoding);
    outcome.messages.insert(outcome.messages.end(), bms.messages.begin(), bms.messages.end());
    outcome.succeeded = bms.written;
    exported_ = bms.written;
    return outcome;
}

PreviewResult ConverterSession::previewSlice(std::size_t index, PreviewSource source) {
    if (source == PreviewSource::Exported && !exported_) {
        return PreviewResult{std::nullopt, "まだ書き出していません"};
    }
    return preview_.slice(conversion_, index, options_, source, outputPath_);
}

PreviewResult ConverterSession::previewSong(PreviewSource source, int sampleRate) {
    if (source == PreviewSource::Exported && !exported_) {
        return PreviewResult{std::nullopt, "まだ書き出していません"};
    }
    return preview_.song(conversion_, options_, source, outputPath_, sampleRate);
}

LaunchOutcome ConverterSession::launchPlayer(const PlayerSettings& settings, ProcessLauncher& launcher) const {
    return app::launchPlayer(settings, outputPath_, launcher);
}

}  // namespace b2b::app
