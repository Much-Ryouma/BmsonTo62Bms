#include <gtest/gtest.h>

#include <algorithm>

#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "b2b/app/ConvertService.hpp"

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
    app::ConvertService service{charts, audio, codec, files};

    Fixture() {
        auto chart = makeChart();
        chart.soundChannels = {channel("drums.wav", {note(1, 0), note(0, 480, true)})};
        charts.chart = chart;
        audio.files["drums.wav"] = core::AudioInfo{1000, 10000};
    }

    app::Conversion prepare(const app::ConvertOptions& options = {}) { return service.prepare("songs/a.bmson", options); }
};

bool hasMessage(const std::vector<app::Message>& messages, app::Severity severity, const std::string& part) {
    return std::any_of(messages.begin(), messages.end(), [&](const app::Message& message) {
        return message.severity == severity && message.text.find(part) != std::string::npos;
    });
}

}  // namespace

TEST(ConvertService, CNV001_PreparesADocumentWithTemporarySoundNames) {
    Fixture fixture;
    const auto conversion = fixture.prepare();
    ASSERT_FALSE(conversion.hasErrors());
    ASSERT_TRUE(conversion.document);
    EXPECT_EQ(conversion.soundFiles, (std::vector<std::string>{"wav/Title_k0001.wav", "wav/Title_b0001.wav"}));
}

TEST(ConvertService, CNV002_UnreadableChartIsAnError) {
    Fixture fixture;
    fixture.charts.chart.reset();
    const auto conversion = fixture.prepare();
    EXPECT_FALSE(conversion.document);
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Error, "bmson を読み込めません"));
}

TEST(ConvertService, CNV003_MissingAudioIsAnError) {
    Fixture fixture;
    fixture.audio.files.clear();
    const auto conversion = fixture.prepare();
    EXPECT_FALSE(conversion.document);
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Error, "drums.wav"));
}

TEST(ConvertService, CNV004_UnsupportedModeStopsBeforeReadingAudio) {
    Fixture fixture;
    fixture.charts.chart->info.modeHint = "popn-9k";
    const auto conversion = fixture.prepare();
    EXPECT_FALSE(conversion.document);
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Error, "popn-9k"));
    EXPECT_TRUE(fixture.audio.probes.empty());
}

TEST(ConvertService, CNV005_SharedAudioIsProbedOnce) {
    Fixture fixture;
    fixture.charts.chart->soundChannels.push_back(channel("drums.wav", {note(2, 960)}));
    static_cast<void>(fixture.prepare());
    EXPECT_EQ(fixture.audio.probes["drums.wav"], 1);
}

TEST(ConvertService, CNV006_WritesShiftJisWithCrLf) {
    Fixture fixture;
    fixture.charts.chart->info.title = "テスト";
    const auto conversion = fixture.prepare();
    const auto outcome = fixture.service.writeBms(conversion, "out/a.bme", infra::TextEncoding::ShiftJis);
    ASSERT_TRUE(outcome.written);
    const std::string& bytes = fixture.files.written.at("out/a.bme");
    EXPECT_NE(bytes.find("#TITLE \x83\x65\x83\x58\x83\x67\r\n"), std::string::npos);
    EXPECT_EQ(bytes.find("\n", 0), bytes.find("\r\n", 0) + 1);
}

TEST(ConvertService, CNV007_UnrepresentableTitleWarnsButFileNameFails) {
    Fixture title;
    title.charts.chart->info.title = "曲🎵";
    const auto titled = title.service.writeBms(title.prepare(), "a.bme", infra::TextEncoding::ShiftJis);
    EXPECT_TRUE(titled.written);
    EXPECT_TRUE(hasMessage(titled.messages, app::Severity::Warning, "🎵"));

    Fixture image;
    image.charts.chart->bga.headers = {{1, "🎵.png"}};
    image.charts.chart->bga.bgaEvents = {{0, 1}};
    const auto imaged = image.service.writeBms(image.prepare(), "a.bme", infra::TextEncoding::ShiftJis);
    EXPECT_FALSE(imaged.written);
    EXPECT_TRUE(hasMessage(imaged.messages, app::Severity::Error, "🎵"));
    EXPECT_TRUE(image.files.written.empty());
}

TEST(ConvertService, CNV008_DefaultOutputSitsBesideTheChart) {
    EXPECT_EQ(app::ConvertService::defaultOutputPath("C:/songs/a.bmson"), std::filesystem::path("C:/songs/a.bme"));
}

TEST(ConvertService, CNV009_ReportsWhatCannotBeWritten) {
    Fixture fixture;
    fixture.charts.chart->info.chartName = "HYPER";
    fixture.charts.chart->info.titleImage = "title.png";
    fixture.charts.chart->soundChannels[0].notes.push_back(note(0, 0));
    const auto conversion = fixture.prepare();
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Warning, "chart_name"));
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Warning, "title_image"));
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Info, "BGM"));
}

TEST(ConvertService, CNV010_WritingOutsideTheChartFolderWarns) {
    Fixture fixture;
    const auto conversion = fixture.prepare();
    const auto beside = fixture.service.writeBms(conversion, "songs/a.bme", infra::TextEncoding::ShiftJis);
    EXPECT_FALSE(hasMessage(beside.messages, app::Severity::Warning, "別のフォルダ"));
    const auto elsewhere = fixture.service.writeBms(conversion, "out/a.bme", infra::TextEncoding::ShiftJis);
    EXPECT_TRUE(elsewhere.written);
    EXPECT_TRUE(hasMessage(elsewhere.messages, app::Severity::Warning, "別のフォルダ"));
}

TEST(ConvertService, CNV011_InvalidOptionsStopBeforeReadingAnything) {
    Fixture fixture;
    app::ConvertOptions fade;
    fade.fade.fadeOutMs = 60.0;
    app::ConvertOptions quality;
    quality.oggQuality = 11;
    app::ConvertOptions pattern;
    pattern.naming.keyPattern = "{oops}";
    for (const auto& options : {fade, quality, pattern}) {
        const auto conversion = fixture.prepare(options);
        EXPECT_FALSE(conversion.document);
        EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Error, ""));
    }
    EXPECT_TRUE(fixture.audio.probes.empty());
    EXPECT_TRUE(hasMessage(fixture.prepare(pattern).messages, app::Severity::Error, "パラ音のファイル名"));
}

TEST(ConvertService, CNV012_SoundWithAnotherExtensionIsUsedWithAWarning) {
    Fixture fixture;
    fixture.audio.files.clear();
    fixture.audio.files["drums.ogg"] = core::AudioInfo{1000, 10000};
    const auto conversion = fixture.prepare();
    ASSERT_FALSE(conversion.hasErrors());
    EXPECT_EQ(conversion.plan.slices.front().source, "drums.ogg");
    EXPECT_EQ(fixture.audio.probes["drums.ogg"], 1);
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Warning, "drums.ogg"));
}

TEST(ConvertService, CNV013_DeclaredFileIsPreferredAndMissingFilesStillFail) {
    Fixture fixture;
    fixture.audio.files["drums.ogg"] = core::AudioInfo{1000, 10000};
    EXPECT_EQ(fixture.prepare().plan.slices.front().source, "drums.wav");

    fixture.audio.files.clear();
    fixture.audio.files["other.ogg"] = core::AudioInfo{1000, 10000};
    const auto missing = fixture.prepare();
    EXPECT_FALSE(missing.document);
    EXPECT_TRUE(hasMessage(missing.messages, app::Severity::Error, "drums.wav"));
}

TEST(ConvertService, CNV014_LanesOutsideTheModeAreReported) {
    Fixture fixture;
    fixture.charts.chart->soundChannels[0].notes.push_back(note(14, 960));
    const auto conversion = fixture.prepare();
    ASSERT_FALSE(conversion.hasErrors());
    EXPECT_TRUE(hasMessage(conversion.messages, app::Severity::Warning, "BGM として書き出します"));
}
