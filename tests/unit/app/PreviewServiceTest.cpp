#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "WavWriter.hpp"
#include "b2b/app/PreviewService.hpp"

using namespace b2b;
using test::channel;
using test::makeChart;
using test::note;

namespace {

core::PcmBuffer monoSource() {
    core::PcmBuffer buffer;
    buffer.sampleRate = 1000;
    buffer.channels = 1;
    buffer.format = core::SampleFormat::F32;
    buffer.bytes.resize(3000 * 4);
    for (std::size_t index = 0; index < 3000; ++index) {
        core::setSample(buffer, index, static_cast<double>(index % 100) / 200.0 + 0.1);
    }
    return buffer;
}

struct Fixture {
    test::FakeChartSource charts;
    test::FakeAudioSource audio;
    test::FakeAudioCodec codec;
    test::FakeFileSink files;
    app::ConvertService service{charts, audio, codec, files};
    app::PreviewService preview{codec};
    app::Conversion conversion;

    explicit Fixture(const app::ConvertOptions& options = {}) {
        auto chart = makeChart();
        chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 480, true)})};
        charts.chart = chart;
        audio.files["a.wav"] = core::AudioInfo{1000, 3000};
        codec.sources["a.wav"] = monoSource();
        conversion = service.prepare("songs/song.bmson", options);
    }
};

std::filesystem::path scratchFile(const char* name) {
    auto directory = std::filesystem::temp_directory_path() / "b2b_tests" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "song.bme";
    std::ofstream(file) << "#PLAYER 1\n";
    return file;
}

}  // namespace

TEST(PreviewService, PRV001_PlannedSliceIsTheSourceRange) {
    Fixture fixture;
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto result = fixture.preview.slice(fixture.conversion, 1, {}, app::PreviewSource::Planned, "songs/song.bme");
        ASSERT_TRUE(result.audio) << result.error;
        ASSERT_EQ(result.audio->frames(), 2000);
        EXPECT_FLOAT_EQ(result.audio->samples[0], static_cast<float>(core::sampleAt(monoSource(), 1000)));
        EXPECT_FLOAT_EQ(result.audio->samples[1], result.audio->samples[0]);
    }
    EXPECT_EQ(fixture.codec.decodes["a.wav"].size(), 1u);
}

TEST(PreviewService, PRV002_PlannedSliceUsesTheEdgeSettings) {
    app::ConvertOptions options;
    options.fade.mode = core::EdgeMode::Fade;
    options.fade.fadeOutMs = 2.0;
    Fixture fixture(options);
    const auto result = fixture.preview.slice(fixture.conversion, 0, options, app::PreviewSource::Planned, "songs/song.bme");
    ASSERT_TRUE(result.audio);
    EXPECT_FLOAT_EQ(result.audio->samples.back(), 0.0f);
}

TEST(PreviewService, PRV003_ExportedSliceReadsTheWrittenFile) {
    Fixture fixture;
    const std::string written = fixture.conversion.soundFiles[0];
    fixture.codec.sources[std::filesystem::path(written).filename().string()] = monoSource();
    const auto result = fixture.preview.slice(fixture.conversion, 0, {}, app::PreviewSource::Exported, "out/song.bme");
    ASSERT_TRUE(result.audio) << result.error;
    EXPECT_EQ(result.audio->frames(), 3000);
    EXPECT_EQ(fixture.codec.decodes.count(std::filesystem::path(written).filename().string()), 1u);
}

TEST(PreviewService, PRV004_SongPlacesEveryNoteAtItsTime) {
    Fixture fixture;
    const auto result = fixture.preview.song(fixture.conversion, {}, app::PreviewSource::Planned, "songs/song.bme", 1000);
    ASSERT_TRUE(result.audio) << result.error;
    EXPECT_EQ(result.audio->frames(), 1000 + 2000);
    EXPECT_FLOAT_EQ(result.audio->samples[1000 * 2], static_cast<float>(core::sampleAt(monoSource(), 1000)));
}

TEST(PreviewService, PRV005_FailedConversionCannotBePreviewed) {
    Fixture fixture;
    fixture.audio.files.clear();
    fixture.conversion = fixture.service.prepare("songs/song.bmson", {});
    const auto result = fixture.preview.song(fixture.conversion, {}, app::PreviewSource::Planned, "songs/song.bme", 1000);
    EXPECT_FALSE(result.audio);
    EXPECT_FALSE(result.error.empty());
}

TEST(PreviewService, PRV006_LaunchNeedsAnExecutableAndAWrittenBms) {
    test::FakeLauncher launcher;
    const auto noExecutable = app::launchPlayer(app::PlayerSettings{}, scratchFile("prv006"), launcher);
    EXPECT_FALSE(noExecutable.launched);
    EXPECT_FALSE(noExecutable.error.empty());
    app::PlayerSettings settings;
    settings.executable = "/opt/player/play";
    const auto noBms = app::launchPlayer(settings, "/no/such/song.bme", launcher);
    EXPECT_FALSE(noBms.launched);
    EXPECT_FALSE(noBms.error.empty());
    EXPECT_EQ(launcher.launches, 0);
}

TEST(PreviewService, PRV007_LaunchUsesTheTemplateAndThePlayerFolder) {
    test::FakeLauncher launcher;
    app::PlayerSettings settings;
    settings.executable = "/opt/player/play";
    const auto bms = scratchFile("prv007");
    const auto outcome = app::launchPlayer(settings, bms, launcher);
    EXPECT_TRUE(outcome.launched);
    EXPECT_EQ(launcher.program, "/opt/player/play");
    EXPECT_EQ(launcher.arguments, (std::vector<std::string>{std::filesystem::absolute(bms).string()}));
    EXPECT_EQ(launcher.workingDirectory, std::filesystem::path("/opt/player"));
}

TEST(PreviewService, PRV008_WorkingDirectoryCanBeChosen) {
    test::FakeLauncher launcher;
    app::PlayerSettings settings;
    settings.executable = "/opt/player/play";
    settings.workingDirectory = "/work";
    static_cast<void>(app::launchPlayer(settings, scratchFile("prv008"), launcher));
    EXPECT_EQ(launcher.workingDirectory, std::filesystem::path("/work"));
}

TEST(PreviewService, PRV009_ProbesAreCachedUntilCleared) {
    test::FakeAudioSource inner;
    inner.files["a.wav"] = core::AudioInfo{1000, 10};
    app::CachingAudioSource cache(inner);
    static_cast<void>(cache.probe("x/a.wav"));
    static_cast<void>(cache.probe("x/a.wav"));
    EXPECT_EQ(inner.probes["a.wav"], 1);
    cache.clear();
    static_cast<void>(cache.probe("x/a.wav"));
    EXPECT_EQ(inner.probes["a.wav"], 2);
}
