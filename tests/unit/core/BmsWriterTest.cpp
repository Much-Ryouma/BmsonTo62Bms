#include <gtest/gtest.h>

#include <algorithm>

#include "ChartBuilder.hpp"
#include "b2b/core/bms/BmsWriter.hpp"

using namespace b2b::core;
using b2b::test::channel;
using b2b::test::makeChart;
using b2b::test::note;

namespace {

BmsWriteResult write(const bmson::Chart& chart, BmsWriteOptions options = {}) {
    const std::vector<AudioInfo> audio(chart.soundChannels.size(), AudioInfo{1000, 1000000});
    const SlicePlan plan = planSlices(chart, bmson::timingOf(chart), audio);
    std::vector<std::string> files;
    for (std::size_t index = 0; index < plan.slices.size(); ++index) {
        files.push_back("s" + std::to_string(index + 1) + ".wav");
    }
    return writeBms(chart, plan, files, options);
}

std::vector<std::string> linesStartingWith(const BmsDocument& document, const std::string& prefix) {
    std::vector<std::string> found;
    for (const auto& line : document.lines) {
        if (line.text.rfind(prefix, 0) == 0) {
            found.push_back(line.text);
        }
    }
    return found;
}

bool hasLine(const BmsDocument& document, const std::string& text) {
    return std::any_of(document.lines.begin(), document.lines.end(), [&](const BmsLine& line) { return line.text == text; });
}

bmson::Chart chartWithSlices(int count) {
    auto chart = makeChart();
    std::vector<bmson::Note> notes;
    for (int index = 0; index < count; ++index) {
        notes.push_back(note(0, static_cast<Pulse>(index) * 60, true));
    }
    chart.soundChannels = {channel("a.wav", notes)};
    return chart;
}

}  // namespace

TEST(BmsWriter, BMW001_HeaderCarriesSongInfo) {
    auto chart = makeChart();
    chart.info.subartists = {"Vocal", "Mix"};
    chart.info.level = 7;
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    for (const char* expected : {"#PLAYER 1", "#GENRE Genre", "#TITLE Title", "#ARTIST Artist", "#SUBARTIST Vocal",
                                 "#SUBARTIST Mix", "#BPM 120", "#PLAYLEVEL 7", "#DEFEXRANK 134", "#LNTYPE 1"}) {
        EXPECT_TRUE(hasLine(document, expected)) << expected;
    }
    EXPECT_TRUE(linesStartingWith(document, "#SUBTITLE").empty());
    EXPECT_TRUE(linesStartingWith(document, "#TOTAL").empty());
    EXPECT_TRUE(linesStartingWith(document, "#BASE").empty());
}

TEST(BmsWriter, BMW002_DifficultyIsWrittenOnlyWhenChosen) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    EXPECT_TRUE(linesStartingWith(*write(chart).document, "#DIFFICULTY").empty());
    BmsWriteOptions options;
    options.difficulty = 3;
    EXPECT_TRUE(hasLine(*write(chart, options).document, "#DIFFICULTY 3"));
}

TEST(BmsWriter, BMW003_LanesMapToBeat7kChannels) {
    const std::vector<std::string> normal = {"11", "12", "13", "14", "15", "18", "19", "16"};
    const std::vector<std::string> longNote = {"51", "52", "53", "54", "55", "58", "59", "56"};
    for (int lane = 1; lane <= 8; ++lane) {
        EXPECT_EQ(channelForLane(lane, false), normal[static_cast<std::size_t>(lane - 1)]);
        EXPECT_EQ(channelForLane(lane, true), longNote[static_cast<std::size_t>(lane - 1)]);
    }
}

TEST(BmsWriter, BMW004_OverlappingBgmUsesSeveralLines) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(0, 0)}), channel("b.wav", {note(0, 0)})};
    EXPECT_EQ(linesStartingWith(*write(chart).document, "#00001:").size(), 2u);
}

TEST(BmsWriter, BMW005_PositionsUseTheCoarsestDivision) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(1, 480, true)})};
    EXPECT_TRUE(hasLine(*write(chart).document, "#00011:0102"));
}

TEST(BmsWriter, BMW006_IrregularMeasureGetsALength) {
    auto chart = makeChart();
    chart.lines = {0, 720, 1680};
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(1, 1700, true)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    EXPECT_TRUE(hasLine(document, "#00002:0.75"));
    EXPECT_TRUE(linesStartingWith(document, "#00102:").empty());
}

TEST(BmsWriter, BMW007_LongNoteWritesStartAndEndWithTheSameId) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 480, false, 960)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    EXPECT_TRUE(hasLine(document, "#00051:0001"));
    EXPECT_TRUE(hasLine(document, "#00151:0001"));
}

TEST(BmsWriter, BMW008_TempoChangesShareIdsForEqualValues) {
    auto chart = makeChart();
    chart.bpmEvents = {{960, 150.5}, {1920, 150.5}, {2880, 90.0}};
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    EXPECT_TRUE(hasLine(document, "#BPM01 150.5"));
    EXPECT_TRUE(hasLine(document, "#BPM02 90"));
    EXPECT_TRUE(hasLine(document, "#00108:01"));
    EXPECT_TRUE(hasLine(document, "#00208:01"));
    EXPECT_TRUE(hasLine(document, "#00308:02"));
}

TEST(BmsWriter, BMW009_StopIsWrittenInBmsUnitsKeepingFractions) {
    auto chart = makeChart();
    chart.stopEvents = {{0, 25}};
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    EXPECT_TRUE(hasLine(*write(chart).document, "#STOP01 5"));
    EXPECT_TRUE(hasLine(*write(chart).document, "#00009:01"));

    auto fine = makeChart(120.0, 480);
    fine.stopEvents = {{0, 5}};
    fine.soundChannels = {channel("a.wav", {note(1, 0)})};
    EXPECT_TRUE(hasLine(*write(fine).document, "#STOP01 0.5"));
}

TEST(BmsWriter, BMW010_BgaIdsFollowFirstUse) {
    auto chart = makeChart();
    chart.bga.headers = {{5, "b.png"}, {3, "a.png"}};
    chart.bga.bgaEvents = {{0, 3}};
    chart.bga.layerEvents = {{960, 5}};
    chart.bga.poorEvents = {{960, 3}};
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    EXPECT_TRUE(hasLine(document, "#BMP01 a.png"));
    EXPECT_TRUE(hasLine(document, "#BMP02 b.png"));
    EXPECT_TRUE(hasLine(document, "#00004:01"));
    EXPECT_TRUE(hasLine(document, "#00107:02"));
    EXPECT_TRUE(hasLine(document, "#00106:01"));
}

TEST(BmsWriter, BMW011_UnknownBgaIdIsAnError) {
    auto chart = makeChart();
    chart.bga.bgaEvents = {{0, 9}};
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = write(chart);
    EXPECT_FALSE(result.document);
    EXPECT_FALSE(result.errors.empty());
}

TEST(BmsWriter, BMW012_MoreThan1295IdsSwitchToBase62) {
    const auto base36 = write(chartWithSlices(1295));
    ASSERT_TRUE(base36.document);
    EXPECT_EQ(base36.document->base, IdBase::Base36);
    EXPECT_TRUE(linesStartingWith(*base36.document, "#BASE").empty());
    EXPECT_TRUE(hasLine(*base36.document, "#WAVZZ s1295.wav"));

    const auto base62 = write(chartWithSlices(1296));
    ASSERT_TRUE(base62.document);
    EXPECT_EQ(base62.document->base, IdBase::Base62);
    EXPECT_TRUE(hasLine(*base62.document, "#BASE 62"));
    EXPECT_TRUE(hasLine(*base62.document, "#WAVKu s1296.wav"));
    EXPECT_FALSE(base62.warnings.empty());
}

TEST(BmsWriter, BMW013_MoreThan3843IdsIsAnError) {
    const auto result = write(chartWithSlices(3844));
    EXPECT_FALSE(result.document);
    EXPECT_FALSE(result.errors.empty());
}

TEST(BmsWriter, BMW014_MeasureBeyond999IsAnError) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 1000 * 960)})};
    const auto result = write(chart);
    EXPECT_FALSE(result.document);
    EXPECT_FALSE(result.errors.empty());
}

TEST(BmsWriter, BMW015_UnsupportedModeIsAnError) {
    auto chart = makeChart();
    chart.info.modeHint = "popn-9k";
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    EXPECT_FALSE(write(chart).document);
}

TEST(BmsWriter, BMW016_StackedKeyNotesKeepTheFirst) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0)}), channel("b.wav", {note(1, 0)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto lines = linesStartingWith(*result.document, "#00011:");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "#00011:01");
    EXPECT_FALSE(result.warnings.empty());
}

TEST(BmsWriter, BMW017_LongNoteEndingWhereTheNextStartsIsAnError) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0, false, 480), note(1, 480, true, 480)})};
    const auto result = write(chart);
    EXPECT_FALSE(result.document);
    EXPECT_FALSE(result.errors.empty());
}

TEST(BmsWriter, BMW018_FileNameLinesAreMarkedAsPaths) {
    auto chart = makeChart();
    chart.info.eyecatchImage = "eye.png";
    chart.bga.headers = {{1, "a.png"}};
    chart.bga.bgaEvents = {{0, 1}};
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = write(chart);
    ASSERT_TRUE(result.document);
    const auto& document = *result.document;
    for (const auto& line : document.lines) {
        const bool path = line.text.rfind("#WAV", 0) == 0 || line.text.rfind("#BMP", 0) == 0 ||
                          line.text.rfind("#STAGEFILE", 0) == 0;
        EXPECT_EQ(line.kind == BmsLineKind::Path, path) << line.text;
    }
}
