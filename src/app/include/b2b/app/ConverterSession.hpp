#pragma once

#include <filesystem>
#include <vector>

#include "b2b/app/ConvertService.hpp"
#include "b2b/app/PreviewService.hpp"

namespace b2b::app {

struct ExportAllOutcome {
    bool succeeded = false;
    std::vector<Message> messages;
};

class ConverterSession {
public:
    ConverterSession(const ChartSource& charts, const AudioSource& audio, const AudioCodec& codec, const FileSink& files);

    void openChart(const std::filesystem::path& chartPath);
    void setOutputPath(const std::filesystem::path& outputPath);
    void setOptions(const ConvertOptions& options);
    void refresh();

    [[nodiscard]] bool hasChart() const noexcept { return !chartPath_.empty(); }
    [[nodiscard]] const std::filesystem::path& chartPath() const noexcept { return chartPath_; }
    [[nodiscard]] const std::filesystem::path& outputPath() const noexcept { return outputPath_; }
    [[nodiscard]] const ConvertOptions& options() const noexcept { return options_; }
    [[nodiscard]] const Conversion& conversion() const noexcept { return conversion_; }
    [[nodiscard]] bool canExport() const;
    [[nodiscard]] bool exported() const noexcept { return exported_; }

    [[nodiscard]] ExportAllOutcome exportAll(const ExportProgress& progress = {});

    [[nodiscard]] PreviewResult previewSlice(std::size_t index, PreviewSource source);
    [[nodiscard]] PreviewResult previewSong(PreviewSource source, int sampleRate);
    [[nodiscard]] LaunchOutcome launchPlayer(const PlayerSettings& settings, ProcessLauncher& launcher) const;

private:
    CachingAudioSource audio_;
    ConvertService service_;
    PreviewService preview_;
    std::filesystem::path chartPath_;
    std::filesystem::path outputPath_;
    ConvertOptions options_;
    Conversion conversion_;
    bool exported_ = false;
};

}  // namespace b2b::app
