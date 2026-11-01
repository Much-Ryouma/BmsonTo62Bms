#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "b2b/app/Messages.hpp"
#include "b2b/app/Ports.hpp"
#include "b2b/app/SoundNaming.hpp"
#include "b2b/core/audio/Fade.hpp"
#include "b2b/core/bms/BmsWriter.hpp"
#include "b2b/infra/text/TextEncoding.hpp"

namespace b2b::app {

enum class SoundFormat { Wav, Ogg };

inline constexpr int kDefaultOggQuality = 5;

struct ConvertOptions {
    infra::TextEncoding encoding = infra::TextEncoding::ShiftJis;
    std::optional<int> difficulty;
    SoundFormat soundFormat = SoundFormat::Wav;
    int oggQuality = kDefaultOggQuality;
    NamingOptions naming;
    core::FadeSettings fade;
    bool allowExtraSlices = false;
};

[[nodiscard]] std::string extensionOf(SoundFormat format);

struct Conversion {
    std::filesystem::path chartPath;
    core::bmson::Chart chart;
    core::SlicePlan plan;
    std::vector<std::string> soundFiles;
    std::string soundFolder;
    std::optional<core::BmsDocument> document;
    std::vector<Message> messages;
    std::vector<std::size_t> outsideBase;

    [[nodiscard]] bool hasErrors() const;
};

struct ChartRequest {
    std::filesystem::path path;
    std::optional<int> difficulty;
};

struct ConversionSet {
    std::vector<Conversion> charts;
    std::size_t baseIndex = 0;
    std::vector<Message> messages;
    std::vector<std::vector<std::size_t>> sliceUsers;

    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] const Conversion& base() const { return charts.at(baseIndex); }
};

struct ChartSummary {
    std::filesystem::path path;
    std::string title;
    std::string chartName;
    std::int64_t level = 0;
    std::string modeHint;
    int keyNotes = 0;
    std::optional<std::string> error;
};

[[nodiscard]] std::size_t defaultBaseIndex(const std::vector<ChartSummary>& charts);

[[nodiscard]] std::optional<int> guessDifficulty(const std::string& chartName);

struct RenderedBms {
    std::string bytes;
    std::vector<Message> messages;

    [[nodiscard]] bool hasErrors() const;
};

struct WriteOutcome {
    bool written = false;
    std::filesystem::path path;
    std::vector<Message> messages;
};

struct ExportOutcome {
    int writtenFiles = 0;
    std::vector<Message> messages;

    [[nodiscard]] bool hasErrors() const;
};

using ExportProgress = std::function<void(std::size_t done, std::size_t total)>;

[[nodiscard]] std::string soundFileOnDisk(const Conversion& conversion, std::size_t index);

[[nodiscard]] RenderedBms renderBms(const core::BmsDocument& document, infra::TextEncoding encoding);

[[nodiscard]] SampleRequest sampleRequestFor(const std::string& source, SoundFormat format);

[[nodiscard]] std::optional<std::string> locateSoundFile(const std::string& declared, const std::filesystem::path& directory,
                                                       const AudioSource& audio);

class ConvertService {
public:
    ConvertService(const ChartSource& charts, const AudioSource& audio, const AudioCodec& codec, const FileSink& files);

    [[nodiscard]] Conversion prepare(const std::filesystem::path& chartPath, const ConvertOptions& options) const;
    [[nodiscard]] ConversionSet prepareSet(const std::vector<ChartRequest>& requests, std::size_t baseIndex,
                                           const ConvertOptions& options) const;
    [[nodiscard]] ChartSummary summarize(const std::filesystem::path& chartPath) const;
    [[nodiscard]] ExportOutcome exportSounds(const Conversion& conversion, const std::filesystem::path& bmsPath,
                                             const ConvertOptions& options, const ExportProgress& progress = {}) const;
    [[nodiscard]] WriteOutcome writeBms(const Conversion& conversion, const std::filesystem::path& output,
                                        infra::TextEncoding encoding) const;

    [[nodiscard]] static std::filesystem::path defaultOutputPath(const std::filesystem::path& chartPath,
                                                              const std::string& modeHint = "beat-7k");

private:
    [[nodiscard]] bool loadAndPlan(Conversion& conversion) const;

    const ChartSource& charts_;
    const AudioSource& audio_;
    const AudioCodec& codec_;
    const FileSink& files_;
};

}  // namespace b2b::app
