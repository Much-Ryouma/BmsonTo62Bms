#include "b2b/app/PreviewService.hpp"

#include <stdexcept>

#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::app {

PreviewService::PreviewService(const AudioCodec& codec) : codec_(codec) {}

void PreviewService::clear() {
    sources_.clear();
}

core::StereoAudio PreviewService::plannedSlice(const Conversion& conversion, std::size_t index,
                                               const ConvertOptions& options) {
    const core::Slice& slice = conversion.plan.slices.at(index);
    auto found = sources_.find(slice.source);
    if (found == sources_.end()) {
        const auto path = conversion.chartPath.parent_path() / infra::pathFromUtf8(slice.source);
        found = sources_.emplace(slice.source, codec_.decode(path, SampleRequest::Float32)).first;
    }
    core::PcmBuffer piece = core::extractFrames(found->second, slice.startFrame, slice.endFrame);
    core::applyFade(piece, options.fade, slice.startFrame == 0);
    return core::stereoOf(piece);
}

core::StereoAudio PreviewService::exportedSlice(const Conversion& conversion, std::size_t index,
                                                const std::filesystem::path& bmsPath) const {
    const auto path = bmsPath.parent_path() / infra::pathFromUtf8(soundFileOnDisk(conversion, index));
    return core::stereoOf(codec_.decode(path, SampleRequest::Float32));
}

PreviewResult PreviewService::slice(const Conversion& conversion, std::size_t index, const ConvertOptions& options,
                                    PreviewSource source, const std::filesystem::path& bmsPath) {
    PreviewResult result;
    if (conversion.hasErrors() || index >= conversion.plan.slices.size()) {
        result.error = "試聴できるスライスがありません";
        return result;
    }
    try {
        result.audio = source == PreviewSource::Planned ? plannedSlice(conversion, index, options)
                                                        : exportedSlice(conversion, index, bmsPath);
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

PreviewResult PreviewService::song(const Conversion& conversion, const ConvertOptions& options, PreviewSource source,
                                   const std::filesystem::path& bmsPath, int sampleRate) {
    PreviewResult result;
    if (conversion.hasErrors()) {
        result.error = "変換に失敗しているため試聴できません";
        return result;
    }
    try {
        const core::TimingMap timing = core::bmson::timingOf(conversion.chart);
        std::map<std::size_t, core::StereoAudio> slices;
        core::StereoAudio mix;
        mix.sampleRate = sampleRate;
        for (const auto& note : conversion.plan.notes) {
            auto found = slices.find(note.slice);
            if (found == slices.end()) {
                core::StereoAudio audio = source == PreviewSource::Planned ? plannedSlice(conversion, note.slice, options)
                                                                           : exportedSlice(conversion, note.slice, bmsPath);
                found = slices.emplace(note.slice, core::resampled(audio, sampleRate)).first;
            }
            core::mixAt(mix, found->second, timing.secondsAt(note.y));
        }
        result.audio = std::move(mix);
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

LaunchOutcome launchPlayer(const PlayerSettings& settings, const std::filesystem::path& bmsPath,
                           ProcessLauncher& launcher) {
    LaunchOutcome outcome;
    if (settings.executable.empty()) {
        outcome.error = "外部プレイヤーの実行ファイルが設定されていません（設定 → 外部プレイヤー）";
        return outcome;
    }
    if (!std::filesystem::exists(bmsPath)) {
        outcome.error = "BMS がまだ書き出されていません。先に「書き出す」を押してください";
        return outcome;
    }
    outcome.command = core::buildLaunchCommand(settings.commandTemplate, settings.executable,
                                               infra::pathToUtf8(std::filesystem::absolute(bmsPath)));
    if (outcome.command.program.empty()) {
        outcome.error = "起動のコマンドラインが空です";
        return outcome;
    }
    // Why not BMS のフォルダで起動する: beatoraja などは起動したフォルダから自分の設定を読むため、既定ではプレイヤーのフォルダにする
    outcome.workingDirectory = settings.workingDirectory.empty()
                                   ? infra::pathFromUtf8(settings.executable).parent_path()
                                   : infra::pathFromUtf8(settings.workingDirectory);
    outcome.launched = launcher.launch(outcome.command.program, outcome.command.arguments, outcome.workingDirectory);
    if (!outcome.launched) {
        outcome.error = "外部プレイヤーを起動できません: " + outcome.command.program;
    }
    return outcome;
}

}  // namespace b2b::app
