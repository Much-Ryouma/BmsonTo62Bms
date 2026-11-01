#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "b2b/app/ConvertService.hpp"
#include "b2b/core/audio/Mix.hpp"
#include "b2b/core/player/LaunchCommand.hpp"

namespace b2b::app {

enum class PreviewSource { Planned, Exported };

struct PreviewResult {
    std::optional<core::StereoAudio> audio;
    std::string error;
};

class PreviewService {
public:
    explicit PreviewService(const AudioCodec& codec);

    void clear();

    [[nodiscard]] PreviewResult slice(const Conversion& conversion, std::size_t index, const ConvertOptions& options,
                                      PreviewSource source, const std::filesystem::path& bmsPath);
    [[nodiscard]] PreviewResult song(const Conversion& conversion, const ConvertOptions& options, PreviewSource source,
                                     const std::filesystem::path& bmsPath, int sampleRate);

private:
    [[nodiscard]] core::StereoAudio plannedSlice(const Conversion& conversion, std::size_t index,
                                                 const ConvertOptions& options);
    [[nodiscard]] core::StereoAudio exportedSlice(const Conversion& conversion, std::size_t index,
                                                  const std::filesystem::path& bmsPath) const;

    const AudioCodec& codec_;
    std::map<std::string, core::PcmBuffer> sources_;
};

struct PlayerSettings {
    std::string executable;
    std::string commandTemplate = core::kDefaultLaunchTemplate;
    std::string workingDirectory;

    friend bool operator==(const PlayerSettings&, const PlayerSettings&) = default;
};

struct LaunchOutcome {
    bool launched = false;
    core::LaunchCommand command;
    std::filesystem::path workingDirectory;
    std::string error;
};

[[nodiscard]] LaunchOutcome launchPlayer(const PlayerSettings& settings, const std::filesystem::path& bmsPath,
                                         ProcessLauncher& launcher);

}  // namespace b2b::app
