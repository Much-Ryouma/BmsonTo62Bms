#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "WavWriter.hpp"
#include "b2b/app/ConvertService.hpp"
#include "b2b/infra/audio/AudioFiles.hpp"

using namespace b2b;
using test::channel;
using test::makeChart;
using test::note;

namespace {

core::PcmBuffer sourceAudio(std::int64_t frames) {
    core::PcmBuffer buffer;
    buffer.sampleRate = 1000;
    buffer.channels = 1;
    buffer.format = core::SampleFormat::S16;
    buffer.bytes = test::countingBytes(static_cast<std::size_t>(frames) * 2);
    return buffer;
}

struct Fixture {
    test::FakeChartSource charts;
    test::FakeAudioSource audio;
    test::FakeAudioCodec codec;
    test::FakeFileSink files;
    app::ConvertService service{charts, audio, codec, files};

    Fixture() {
        auto chart = makeChart();
        chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 480, true)}), channel("a.wav", {note(3, 960)}),
                               channel("b.ogg", {note(4, 0)})};
        charts.chart = chart;
        audio.files["a.wav"] = core::AudioInfo{1000, 3000};
        audio.files["b.ogg"] = core::AudioInfo{1000, 2000};
        codec.sources["a.wav"] = sourceAudio(3000);
        codec.sources["b.ogg"] = sourceAudio(2000);
    }

    app::ExportOutcome run(const app::ConvertOptions& options, const app::ExportProgress& progress = {}) {
        conversion = service.prepare("songs/song.bmson", options);
        EXPECT_FALSE(conversion.hasErrors());
        return service.exportSounds(conversion, "songs/song.bme", options, progress);
    }

    app::Conversion conversion;
};

std::string writtenPath(const app::Conversion& conversion, std::size_t index) {
    return infra::pathToUtf8(std::filesystem::path("songs") / infra::pathFromUtf8(app::soundFileOnDisk(conversion, index)));
}

std::filesystem::path scratchDirectory(const char* name) {
    auto directory = std::filesystem::temp_directory_path() / "b2b_tests" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

}  // namespace

TEST(SoundExport, EXP001_EachSourceIsDecodedOnceAndEverySliceWritten) {
    Fixture fixture;
    const auto outcome = fixture.run({});
    EXPECT_FALSE(outcome.hasErrors());
    EXPECT_EQ(fixture.codec.decodes["a.wav"].size(), 1u);
    EXPECT_EQ(fixture.codec.written.size(), fixture.conversion.plan.slices.size());
    EXPECT_EQ(outcome.writtenFiles, static_cast<int>(fixture.conversion.plan.slices.size()));
}

TEST(SoundExport, EXP002_SampleFormatRequestDependsOnSourceAndOutput) {
    EXPECT_EQ(app::sampleRequestFor("a.wav", app::SoundFormat::Wav), app::SampleRequest::Native);
    EXPECT_EQ(app::sampleRequestFor("a.FLAC", app::SoundFormat::Wav), app::SampleRequest::Native);
    EXPECT_EQ(app::sampleRequestFor("a.ogg", app::SoundFormat::Wav), app::SampleRequest::Pcm16);
    EXPECT_EQ(app::sampleRequestFor("a.mp3", app::SoundFormat::Wav), app::SampleRequest::Pcm16);
    EXPECT_EQ(app::sampleRequestFor("a.wav", app::SoundFormat::Ogg), app::SampleRequest::Float32);
}

TEST(SoundExport, EXP003_CutSlicesMatchTheSourceExactly) {
    Fixture fixture;
    static_cast<void>(fixture.run({}));
    const auto& slices = fixture.conversion.plan.slices;
    for (std::size_t index = 0; index < slices.size(); ++index) {
        const auto& written = fixture.codec.written.at(writtenPath(fixture.conversion, index));
        const auto expected = core::extractFrames(fixture.codec.sources.at(slices[index].source), slices[index].startFrame,
                                                  slices[index].endFrame);
        EXPECT_EQ(written.kind, "wav");
        EXPECT_EQ(written.buffer.bytes, expected.bytes) << index;
    }
}

TEST(SoundExport, EXP004_FadeSilencesTheLastSample) {
    Fixture fixture;
    app::ConvertOptions options;
    options.fade.mode = core::EdgeMode::Fade;
    options.fade.fadeOutMs = 2.0;
    static_cast<void>(fixture.run(options));
    for (const auto& [path, written] : fixture.codec.written) {
        ASSERT_GT(written.buffer.frames(), 0) << path;
        EXPECT_EQ(core::sampleAt(written.buffer, written.buffer.sampleCount() - 1), 0.0) << path;
    }
}

TEST(SoundExport, EXP005_UnreadableSourceIsReportedAndOthersAreWritten) {
    Fixture fixture;
    fixture.codec.sources.erase("b.ogg");
    const auto outcome = fixture.run({});
    EXPECT_TRUE(outcome.hasErrors());
    EXPECT_TRUE(std::any_of(outcome.messages.begin(), outcome.messages.end(), [](const app::Message& message) {
        return message.severity == app::Severity::Error && message.text.find("b.ogg") != std::string::npos;
    }));
    EXPECT_EQ(fixture.codec.written.size(), fixture.conversion.plan.slices.size() - 1);
}

TEST(SoundExport, EXP006_OggQualityIsPassedOn) {
    Fixture fixture;
    app::ConvertOptions options;
    options.soundFormat = app::SoundFormat::Ogg;
    options.oggQuality = 7;
    static_cast<void>(fixture.run(options));
    ASSERT_FALSE(fixture.codec.written.empty());
    for (const auto& [path, written] : fixture.codec.written) {
        EXPECT_EQ(written.kind, "ogg") << path;
        EXPECT_EQ(written.quality, 7) << path;
        EXPECT_NE(path.find(".ogg"), std::string::npos);
    }
}

TEST(SoundExport, EXP007_RealFilesKeepBitDepthAndSamples) {
    const auto directory = scratchDirectory("exp007");
    const auto data16 = test::countingBytes(44100 * 2 * 2);
    const auto data24 = test::countingBytes(44100 * 3);
    test::writePcmWav(directory / "drums.wav", 44100, 2, 16, data16);
    test::writePcmWav(directory / "bass.wav", 44100, 1, 24, data24);
    std::ofstream(directory / "song.bmson")
        << R"({"version":"1.0.0","info":{"title":"Song","init_bpm":120,"resolution":240},)"
        << R"("sound_channels":[{"name":"drums.wav","notes":[{"x":1,"y":0},{"x":2,"y":240,"c":true}]},)"
        << R"({"name":"bass.wav","notes":[{"x":0,"y":0},{"x":3,"y":480,"c":true}]}]})";

    const app::LocalChartSource charts;
    const app::LocalAudioSource audio;
    const app::LocalAudioCodec codec;
    const app::LocalFileSink files;
    const app::ConvertService service(charts, audio, codec, files);
    const app::ConvertOptions options;
    const auto conversion = service.prepare(directory / "song.bmson", options);
    ASSERT_FALSE(conversion.hasErrors());
    const auto exported = service.exportSounds(conversion, directory / "song.bme", options);
    ASSERT_FALSE(exported.hasErrors());

    for (std::size_t index = 0; index < conversion.plan.slices.size(); ++index) {
        const auto& slice = conversion.plan.slices[index];
        const auto written = infra::decodePcm(directory / infra::pathFromUtf8(conversion.soundFiles[index]),
                                              infra::DecodeFormat::Native);
        const auto source = infra::decodePcm(directory / slice.source, infra::DecodeFormat::Native);
        EXPECT_EQ(written.format, source.format) << slice.source;
        EXPECT_EQ(written.bytes, core::extractFrames(source, slice.startFrame, slice.endFrame).bytes) << index;
    }
}

TEST(SoundExport, EXP008_ProgressIsReportedPerFile) {
    Fixture fixture;
    std::vector<std::pair<std::size_t, std::size_t>> reports;
    static_cast<void>(fixture.run({}, [&reports](std::size_t done, std::size_t total) { reports.emplace_back(done, total); }));
    const std::size_t total = fixture.conversion.plan.slices.size();
    ASSERT_EQ(reports.size(), total);
    EXPECT_EQ(reports.back(), std::make_pair(total, total));
}

TEST(SoundExport, EXP009_SubfolderHoldsFilesButNotTheDefinitions) {
    Fixture fixture;
    app::ConvertOptions options;
    options.naming.folderPattern = "wav";
    static_cast<void>(fixture.run(options));
    const std::string name = fixture.conversion.soundFiles.front();
    EXPECT_EQ(name.find('/'), std::string::npos);
    // Why not 文字列で比べる: Windows ではパスの区切りが \ になるため、パスとして正規化して比べる
    const auto expected = (std::filesystem::path("songs") / "wav" / infra::pathFromUtf8(name)).lexically_normal();
    EXPECT_EQ(std::count_if(fixture.codec.written.begin(), fixture.codec.written.end(),
                            [&](const auto& written) { return infra::pathFromUtf8(written.first).lexically_normal() == expected; }),
              1);
    const auto& lines = fixture.conversion.document->lines;
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [&](const core::BmsLine& line) { return line.text == "#WAV01 " + name; }));
}
