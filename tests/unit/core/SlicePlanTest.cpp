#include <gtest/gtest.h>

#include "ChartBuilder.hpp"
#include "b2b/core/slicing/SlicePlan.hpp"

using namespace b2b::core;
using b2b::test::channel;
using b2b::test::makeChart;
using b2b::test::note;

namespace {

const AudioInfo kAudio{1000, 10000};

SlicePlan plan(const bmson::Chart& chart) {
    return planSlices(chart, bmson::timingOf(chart), std::vector<AudioInfo>(chart.soundChannels.size(), kAudio));
}

}  // namespace

TEST(SlicePlan, SLC001_SliceRunsUntilTheNextNoteOfTheChannel) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 480, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 2u);
    EXPECT_EQ(result.slices[0].startFrame, 0);
    EXPECT_EQ(result.slices[0].endFrame, 1000);
    EXPECT_EQ(result.slices[1].startFrame, 1000);
    EXPECT_EQ(result.slices[1].endFrame, 10000);
}

TEST(SlicePlan, SLC002_LastSliceRunsToTheEndOfTheAudio) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 1u);
    EXPECT_EQ(result.slices[0].startFrame, 0);
    EXPECT_EQ(result.slices[0].endFrame, 10000);
}

TEST(SlicePlan, SLC003_NoteWithoutContinuationRestartsFromTheBeginning) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(1, 480)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 2u);
    EXPECT_EQ(result.slices[1].startFrame, 0);
    EXPECT_EQ(result.slices[1].endFrame, 10000);
}

TEST(SlicePlan, SLC004_ContinuationWithoutEarlierNoteCountsFromSongStart) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 480, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 1u);
    EXPECT_EQ(result.slices[0].startFrame, 1000);
}

TEST(SlicePlan, SLC005_ChordSharesOneSlice) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 0)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 1u);
    ASSERT_EQ(result.notes.size(), 2u);
    EXPECT_EQ(result.notes[0].slice, 0u);
    EXPECT_EQ(result.notes[1].slice, 0u);
}

TEST(SlicePlan, SLC006_IdenticalRangesAreMerged) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(1, 480, true)}),
                           channel("a.wav", {note(2, 960), note(2, 1440, true)})};
    const auto result = plan(chart);
    EXPECT_EQ(result.slices.size(), 2u);
    EXPECT_EQ(result.notes.size(), 4u);
}

TEST(SlicePlan, SLC007_SameRangeOfDifferentFilesStaysSeparate) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0)}), channel("b.wav", {note(2, 960)})};
    const auto result = plan(chart);
    EXPECT_EQ(result.slices.size(), 2u);
}

TEST(SlicePlan, SLC008_SliceUsedByKeyAndBgmIsAKeySound) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(0, 0), note(0, 480, true)}),
                           channel("a.wav", {note(1, 960), note(1, 1440, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 2u);
    EXPECT_EQ(result.slices[0].kind, SliceKind::Key);
    EXPECT_EQ(result.slices[0].firstPulse, 960);
    EXPECT_EQ(result.slices[0].firstLane, 1);
}

TEST(SlicePlan, SLC009_BgmNoteBesideALaneNoteIsCanceled) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(0, 0)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.notes.size(), 1u);
    EXPECT_EQ(result.notes[0].lane, 1);
    EXPECT_EQ(result.stats.canceledBgmNotes, 1);
}

TEST(SlicePlan, SLC010_LongNoteEndSoundIsIgnored) {
    auto chart = makeChart();
    auto end = note(1, 480, true);
    end.up = true;
    chart.soundChannels = {channel("a.wav", {note(1, 0, false, 480), end})};
    const auto result = plan(chart);
    EXPECT_EQ(result.notes.size(), 1u);
    EXPECT_EQ(result.stats.ignoredLongNoteEndSounds, 1);
}

TEST(SlicePlan, SLC011_NotePastTheAudioEndGetsAnEmptySlice) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 9600, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 1u);
    EXPECT_EQ(result.slices[0].frameCount(), 0);
    EXPECT_EQ(result.stats.notesPastAudioEnd, 1);
}

TEST(SlicePlan, SLC012_KeyAndBgmAreNumberedSeparatelyByFirstUse) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(0, 0), note(1, 240, true), note(0, 480, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.slices.size(), 3u);
    EXPECT_EQ(result.slices[0].kind, SliceKind::Bgm);
    EXPECT_EQ(result.slices[0].indexInKind, 1);
    EXPECT_EQ(result.slices[1].kind, SliceKind::Key);
    EXPECT_EQ(result.slices[1].indexInKind, 1);
    EXPECT_EQ(result.slices[2].kind, SliceKind::Bgm);
    EXPECT_EQ(result.slices[2].indexInKind, 2);
}

TEST(SlicePlan, SLC013_StopIsPartOfTheSlice) {
    auto chart = makeChart();
    chart.stopEvents = {{240, 240}};
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(1, 480, true)})};
    const auto result = plan(chart);
    ASSERT_FALSE(result.slices.empty());
    EXPECT_EQ(result.slices[0].endFrame, 1500);
}

TEST(SlicePlan, SLC014_OnlyKeyNotesKeepTheirLength) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0, false, 480)}), channel("b.wav", {note(0, 0, false, 480)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.notes.size(), 2u);
    for (const auto& placed : result.notes) {
        EXPECT_EQ(placed.length, placed.lane == 0 ? 0 : 480);
    }
}

TEST(SlicePlan, SLC015_LanesOutsideTheModeBecomeBgm) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(14, 0, false, 480), note(-1, 480, true), note(9, 960, true)})};
    const auto result = plan(chart);
    ASSERT_EQ(result.notes.size(), 3u);
    for (const auto& placed : result.notes) {
        EXPECT_EQ(placed.lane, 0);
        EXPECT_EQ(placed.length, 0);
    }
    EXPECT_EQ(result.stats.lanesOutsideMode, 3);
    EXPECT_EQ(result.countOf(SliceKind::Bgm), 3);
}

TEST(SlicePlan, SLC016_LanesOutsideTheModeAreNeverCanceled) {
    auto chart = makeChart();
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(14, 0)})};
    const auto result = plan(chart);
    EXPECT_EQ(result.notes.size(), 2u);
    EXPECT_EQ(result.stats.canceledBgmNotes, 0);
}
