#include <gtest/gtest.h>

#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "b2b/app/ConverterSession.hpp"

#include <algorithm>

using namespace b2b;
using test::channel;
using test::makeChart;
using test::note;

namespace {

struct Fixture {
    test::FakeChartSource charts;
    test::FakeAudioSource audio;
    test::FakeAudioCodec codec;
    test::FakeFileSink files;
    app::ConverterSession session{charts, audio, codec, files};

    Fixture() {
        auto chart = makeChart();
        chart.soundChannels = {channel("a.wav", {note(1, 0), note(0, 480, true)})};
        charts.chart = chart;
        audio.files["a.wav"] = core::AudioInfo{1000, 3000};
        core::PcmBuffer source;
        source.sampleRate = 1000;
        source.channels = 1;
        source.format = core::SampleFormat::S16;
        source.bytes.resize(6000);
        codec.sources["a.wav"] = source;
    }
};

}  // namespace

TEST(ConverterSession, SES001_OpeningAChartConvertsIt) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    EXPECT_FALSE(fixture.session.conversion().hasErrors());
    EXPECT_EQ(fixture.session.outputPath(), std::filesystem::path("songs/a.bme"));
    EXPECT_TRUE(fixture.session.canExport());
}

TEST(ConverterSession, SES002_ChangingOptionsConvertsAgain) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    app::ConvertOptions options;
    options.naming.keyPattern = "key{index}";
    fixture.session.setOptions(options);
    EXPECT_EQ(fixture.session.conversion().soundFiles.front(), "key1.wav");
}

TEST(ConverterSession, SES003_ExportWritesSoundsAndBms) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    const auto outcome = fixture.session.exportAll();
    EXPECT_TRUE(outcome.succeeded);
    EXPECT_TRUE(fixture.session.exported());
    EXPECT_EQ(fixture.codec.written.size(), 2u);
    EXPECT_EQ(fixture.files.written.count("songs/a.bme"), 1u);
}

TEST(ConverterSession, SES004_ChangesAfterExportClearTheExportedState) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    static_cast<void>(fixture.session.exportAll());
    fixture.session.setOptions(app::ConvertOptions{});
    EXPECT_FALSE(fixture.session.exported());
    static_cast<void>(fixture.session.exportAll());
    fixture.session.setOutputPath("other/a.bme");
    EXPECT_FALSE(fixture.session.exported());
}

TEST(ConverterSession, SES005_ExportedPreviewNeedsAnExport) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    const auto result = fixture.session.previewSlice(0, app::PreviewSource::Exported);
    EXPECT_FALSE(result.audio);
    EXPECT_EQ(result.error, "まだ書き出していません");
}

TEST(ConverterSession, SES006_OpeningAnotherChartProbesAgain) {
    Fixture fixture;
    fixture.session.openChart("songs/a.bmson");
    fixture.session.setOptions(app::ConvertOptions{});
    EXPECT_EQ(fixture.audio.probes["a.wav"], 1);
    fixture.session.openChart("other/b.bmson");
    EXPECT_EQ(fixture.audio.probes["a.wav"], 2);
}

TEST(ConverterSession, SES007_FailedConversionIsNotExported) {
    Fixture fixture;
    fixture.audio.files.clear();
    fixture.session.openChart("songs/a.bmson");
    const auto outcome = fixture.session.exportAll();
    EXPECT_FALSE(outcome.succeeded);
    EXPECT_FALSE(outcome.messages.empty());
    EXPECT_TRUE(fixture.codec.written.empty());
}

namespace {

struct SetFixture {
    test::FakeChartSource charts;
    test::FakeAudioSource audio;
    test::FakeAudioCodec codec;
    test::FakeFileSink files;
    app::ConverterSession session{charts, audio, codec, files};

    SetFixture() {
        auto hard = makeChart();
        hard.info.level = 10;
        hard.soundChannels = {channel("a.wav", {note(1, 0), note(2, 240, true), note(0, 480, true)}),
                              channel("b.wav", {note(3, 0)})};
        auto easy = makeChart();
        easy.soundChannels = {channel("a.wav", {note(1, 0), note(2, 240, true), note(0, 480, true)})};
        charts.byName["hard.bmson"] = hard;
        charts.byName["easy.bmson"] = easy;
        core::PcmBuffer source;
        source.sampleRate = 1000;
        source.channels = 1;
        source.format = core::SampleFormat::S16;
        source.bytes.resize(20000);
        for (const char* name : {"a.wav", "b.wav"}) {
            audio.files[name] = core::AudioInfo{1000, 10000};
            codec.sources[name] = source;
        }
        session.openCharts({{"songs/hard.bmson", std::nullopt}, {"songs/easy.bmson", std::nullopt}}, 0);
    }
};

bool definesDifficulty(const app::Conversion& conversion, const std::string& line) {
    return std::any_of(conversion.document->lines.begin(), conversion.document->lines.end(),
                       [&](const core::BmsLine& written) { return written.text == line; });
}

}  // namespace

TEST(ConverterSession, SES008_OpeningASetSelectsTheBase) {
    SetFixture fixture;
    EXPECT_EQ(fixture.session.selectedIndex(), 0u);
    EXPECT_EQ(fixture.session.outputPath(), std::filesystem::path("songs/hard.bme"));
    EXPECT_EQ(fixture.session.conversion().plan.notes.size(), 4u);
    fixture.session.selectChart(1);
    EXPECT_EQ(fixture.session.outputPath(), std::filesystem::path("songs/easy.bme"));
    EXPECT_EQ(fixture.session.conversion().plan.notes.size(), 3u);
}

TEST(ConverterSession, SES009_ExportWritesSharedSoundsOnceAndEveryBms) {
    SetFixture fixture;
    const auto outcome = fixture.session.exportAll();
    EXPECT_TRUE(outcome.succeeded);
    EXPECT_EQ(fixture.codec.written.size(), 4u);
    EXPECT_EQ(fixture.codec.decodes["a.wav"].size(), 1u);
    EXPECT_EQ(fixture.files.written.count("songs/hard.bme"), 1u);
    EXPECT_EQ(fixture.files.written.count("songs/easy.bme"), 1u);
}

TEST(ConverterSession, SES010_DifficultyBelongsToTheSelectedChart) {
    SetFixture fixture;
    fixture.session.selectChart(1);
    fixture.session.setDifficulty(1);
    EXPECT_TRUE(definesDifficulty(fixture.session.conversionSet().charts[1], "#DIFFICULTY 1"));
    EXPECT_FALSE(definesDifficulty(fixture.session.conversionSet().charts[0], "#DIFFICULTY 1"));
}

TEST(ConverterSession, SES011_SetOutputsMustShareAFolder) {
    SetFixture fixture;
    fixture.session.selectChart(1);
    fixture.session.setOutputPath("other/easy.bme");
    const auto outcome = fixture.session.exportAll();
    EXPECT_FALSE(outcome.succeeded);
    EXPECT_EQ(outcome.messages.front().text, "譜面セットの BMS は、すべて同じフォルダに書き出してください");
    EXPECT_TRUE(fixture.codec.written.empty());
}
