#pragma once

#include <filesystem>
#include <optional>
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
    void openCharts(const std::vector<ChartRequest>& requests, std::size_t baseIndex);
    void selectChart(std::size_t index);
    void setOutputPath(const std::filesystem::path& outputPath);
    void setDifficulty(std::optional<int> difficulty);
    void setOptions(const ConvertOptions& options);
    void refresh();

    [[nodiscard]] ChartSummary summarize(const std::filesystem::path& chartPath) const { return service_.summarize(chartPath); }

    [[nodiscard]] bool hasChart() const noexcept { return !requests_.empty(); }
    [[nodiscard]] std::size_t chartCount() const noexcept { return requests_.size(); }
    [[nodiscard]] std::size_t selectedIndex() const noexcept { return selected_; }
    [[nodiscard]] std::size_t baseIndex() const noexcept { return baseIndex_; }
    [[nodiscard]] const std::filesystem::path& chartPath() const;
    [[nodiscard]] const std::filesystem::path& outputPath() const;
    [[nodiscard]] const std::vector<std::filesystem::path>& outputPaths() const noexcept { return outputs_; }
    [[nodiscard]] std::optional<int> difficulty() const;
    [[nodiscard]] const ConvertOptions& options() const noexcept { return options_; }
    [[nodiscard]] const Conversion& conversion() const;
    [[nodiscard]] const ConversionSet& conversionSet() const noexcept { return set_; }
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
    std::vector<ChartRequest> requests_;
    std::vector<std::filesystem::path> outputs_;
    std::size_t baseIndex_ = 0;
    std::size_t selected_ = 0;
    ConvertOptions options_;
    ConversionSet set_;
    bool exported_ = false;
};

}  // namespace b2b::app
