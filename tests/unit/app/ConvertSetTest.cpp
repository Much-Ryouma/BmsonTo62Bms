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

core::bmson::Chart hard() {
    auto chart = makeChart();
    chart.info.level = 10;
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 240, true), note(0, 480, true)}),
                           channel("b.wav", {note(3, 0)})};
    return chart;
}

core::bmson::Chart easy() {
    auto chart = makeChart();
    chart.info.level = 3;
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 240, true), note(0, 480, true)})};
    return chart;
}

core::bmson::Chart normal() {
    auto chart = makeChart();
    chart.info.level = 5;
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 240)})};
    return chart;
}

core::bmson::Chart shortCut() {
    auto chart = makeChart();
    chart.info.level = 4;
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(0, 480, true)})};
    return chart;
}

struct Fixture {
    test::FakeChartSource charts;
    test::FakeAudioSource audio;
    test::FakeAudioCodec codec;
    test::FakeFileSink files;
    app::ConvertService service{charts, audio, codec, files};

    Fixture() {
        charts.byName["hard.bmson"] = hard();
        charts.byName["easy.bmson"] = easy();
        charts.byName["normal.bmson"] = normal();
        charts.byName["short.bmson"] = shortCut();
        audio.files["a.wav"] = core::AudioInfo{1000, 10000};
        audio.files["b.wav"] = core::AudioInfo{1000, 10000};
    }

    app::ConversionSet prepare(const std::string& other, bool allowExtra = false) const {
        app::ConvertOptions options;
        options.allowExtraSlices = allowExtra;
        return service.prepareSet({{"songs/hard.bmson", std::nullopt}, {"songs/" + other, std::nullopt}}, 0, options);
    }
};

bool hasMessage(const std::vector<app::Message>& messages, app::Severity severity, const std::string& part) {
    return std::any_of(messages.begin(), messages.end(), [&](const app::Message& message) {
        return message.severity == severity && message.text.find(part) != std::string::npos;
    });
}

std::vector<std::string> wavLines(const app::Conversion& conversion) {
    std::vector<std::string> lines;
    for (const auto& line : conversion.document->lines) {
        if (line.text.rfind("#WAV", 0) == 0) {
            lines.push_back(line.text.substr(0, 6));
        }
    }
    return lines;
}

bool hasLine(const app::Conversion& conversion, const std::string& text) {
    return std::any_of(conversion.document->lines.begin(), conversion.document->lines.end(),
                       [&](const core::BmsLine& line) { return line.text == text; });
}

}  // namespace

TEST(ConvertSet, SET001_ChartsShareSoundsAndDefineOnlyTheirOwn) {
    const Fixture fixture;
    const auto set = fixture.prepare("easy.bmson");
    ASSERT_FALSE(set.hasErrors());
    EXPECT_EQ(set.charts[0].soundFiles, set.charts[1].soundFiles);
    EXPECT_EQ(wavLines(set.charts[0]), (std::vector<std::string>{"#WAV01", "#WAV02", "#WAV03", "#WAV04"}));
    EXPECT_EQ(wavLines(set.charts[1]), (std::vector<std::string>{"#WAV01", "#WAV03", "#WAV04"}));
}

TEST(ConvertSet, SET002_SlicesOutsideTheBaseAreAnErrorByDefault) {
    const Fixture fixture;
    const auto set = fixture.prepare("normal.bmson");
    EXPECT_TRUE(set.hasErrors());
    EXPECT_TRUE(hasMessage(set.charts[1].messages, app::Severity::Error, "基準の譜面にないスライスが 1 個あります"));
    EXPECT_TRUE(hasMessage(set.charts[1].messages, app::Severity::Error, "小節 000 の a.wav"));
}

TEST(ConvertSet, SET003_SlicesOutsideTheBaseAreCutWhenAllowed) {
    const Fixture fixture;
    const auto set = fixture.prepare("normal.bmson", true);
    ASSERT_FALSE(set.hasErrors());
    EXPECT_TRUE(hasMessage(set.charts[1].messages, app::Severity::Info, "追加で切り出します"));
    ASSERT_EQ(set.base().plan.slices.size(), 5u);
    EXPECT_EQ(set.base().plan.slices[4].startFrame, 0);
    EXPECT_EQ(set.base().plan.slices[4].endFrame, 10000);
    EXPECT_EQ(set.charts[1].outsideBase, (std::vector<std::size_t>{4}));
    EXPECT_TRUE(set.charts[0].outsideBase.empty());
}

TEST(ConvertSet, SET004_ChartsMustShareAFolder) {
    const Fixture fixture;
    const auto set = fixture.service.prepareSet({{"songs/hard.bmson", std::nullopt}, {"other/easy.bmson", std::nullopt}}, 0, {});
    EXPECT_TRUE(set.hasErrors());
    EXPECT_TRUE(hasMessage(set.messages, app::Severity::Error, "すべて同じフォルダに"));
}

TEST(ConvertSet, SET005_DifferentTimingIsWarned) {
    Fixture fixture;
    fixture.charts.byName["easy.bmson"].info.initBpm = 150.0;
    const auto set = fixture.prepare("easy.bmson");
    EXPECT_TRUE(hasMessage(set.charts[1].messages, app::Severity::Warning, "BPM・STOP・小節線・resolution のどれかが違います"));
    EXPECT_FALSE(hasMessage(set.charts[0].messages, app::Severity::Warning, "BPM・STOP・小節線・resolution"));
}

TEST(ConvertSet, SET006_DifficultyIsPerChart) {
    const Fixture fixture;
    const auto set = fixture.service.prepareSet({{"songs/hard.bmson", 4}, {"songs/easy.bmson", std::nullopt}}, 0, {});
    ASSERT_FALSE(set.hasErrors());
    EXPECT_TRUE(hasLine(set.charts[0], "#DIFFICULTY 4"));
    EXPECT_FALSE(std::any_of(set.charts[1].document->lines.begin(), set.charts[1].document->lines.end(),
                             [](const core::BmsLine& line) { return line.text.rfind("#DIFFICULTY", 0) == 0; }));
}

TEST(ConvertSet, SET007_SliceUsersNameTheChartsThatPlayIt) {
    const Fixture fixture;
    const auto set = fixture.prepare("easy.bmson");
    ASSERT_EQ(set.sliceUsers.size(), 4u);
    EXPECT_EQ(set.base().plan.slices[1].source, "b.wav");
    EXPECT_EQ(set.sliceUsers[1], (std::vector<std::size_t>{0}));
    EXPECT_EQ(set.sliceUsers[0], (std::vector<std::size_t>{0, 1}));
}

TEST(ConvertSet, SET008_DefaultBaseIsTheHighestLevel) {
    std::vector<app::ChartSummary> summaries(3);
    summaries[0].level = 5;
    summaries[0].keyNotes = 3;
    summaries[1].level = 10;
    summaries[1].keyNotes = 4;
    summaries[2].level = 10;
    summaries[2].keyNotes = 6;
    EXPECT_EQ(app::defaultBaseIndex(summaries), 2u);
}

TEST(ConvertSet, SET009_DifficultyIsGuessedFromChartName) {
    EXPECT_EQ(app::guessDifficulty("ANOTHER"), 4);
    EXPECT_EQ(app::guessDifficulty("hyper"), 3);
    EXPECT_EQ(app::guessDifficulty("LEGGENDARIA"), 5);
    EXPECT_EQ(app::guessDifficulty("SP NORMAL"), 2);
    EXPECT_EQ(app::guessDifficulty("EX"), std::nullopt);
}

TEST(ConvertSet, SET010_SummaryCountsKeyNotes) {
    const Fixture fixture;
    const auto summary = fixture.service.summarize("songs/hard.bmson");
    EXPECT_FALSE(summary.error);
    EXPECT_EQ(summary.keyNotes, 3);
    EXPECT_EQ(summary.level, 10);
    EXPECT_TRUE(fixture.service.summarize("songs/missing.bmson").error.has_value());
}

TEST(ConvertSet, SET011_LongerSliceIsCutToMatchTheBase) {
    const Fixture fixture;
    const auto set = fixture.prepare("short.bmson");
    ASSERT_FALSE(set.hasErrors());
    EXPECT_TRUE(set.charts[1].outsideBase.empty());
    EXPECT_EQ(set.base().plan.slices.size(), 4u);
    EXPECT_EQ(set.charts[1].plan.notes.size(), 3u);
    EXPECT_TRUE(hasMessage(set.charts[1].messages, app::Severity::Info, "ほかの譜面の切り出し位置に合わせて、1 個のノーツ"));
    EXPECT_FALSE(hasMessage(set.charts[0].messages, app::Severity::Info, "ほかの譜面の切り出し位置"));
}

TEST(ConvertSet, SET012_BaseSliceIsCutToMatchAnotherChart) {
    const Fixture fixture;
    const auto set = fixture.service.prepareSet({{"songs/short.bmson", std::nullopt}, {"songs/easy.bmson", std::nullopt}}, 0, {});
    ASSERT_FALSE(set.hasErrors());
    EXPECT_TRUE(set.charts[1].outsideBase.empty());
    EXPECT_EQ(set.base().plan.slices.size(), 3u);
    EXPECT_EQ(set.base().plan.notes.size(), 3u);
    EXPECT_TRUE(hasMessage(set.charts[0].messages, app::Severity::Info, "BGM のノーツを 1 個加えました"));
}
