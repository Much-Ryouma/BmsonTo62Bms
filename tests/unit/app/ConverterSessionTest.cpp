#include <gtest/gtest.h>

#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "b2b/app/ConverterSession.hpp"

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
    EXPECT_EQ(fixture.session.conversion().soundFiles.front(), "wav/key1.wav");
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
