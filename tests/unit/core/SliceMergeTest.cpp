#include <gtest/gtest.h>

#include <stdexcept>

#include "ChartBuilder.hpp"
#include "b2b/core/slicing/SliceMerge.hpp"

using namespace b2b::core;

namespace {

Slice slice(const std::string& source, std::int64_t start, std::int64_t end, SliceKind kind = SliceKind::Bgm,
            Pulse firstPulse = 0, int firstLane = 0) {
    Slice result;
    result.source = source;
    result.startFrame = start;
    result.endFrame = end;
    result.kind = kind;
    result.firstPulse = firstPulse;
    result.firstLane = firstLane;
    result.sampleRate = 1000;
    return result;
}

SlicePlan planOf(std::vector<Slice> slices) {
    SlicePlan plan;
    plan.slices = std::move(slices);
    return plan;
}

}  // namespace

TEST(SliceMerge, MRG001_EqualSlicesShareOneIndex) {
    const auto merged = mergeSlicePlans({planOf({slice("a", 0, 100), slice("a", 100, 200)}), planOf({slice("a", 100, 200)})}, 0);
    ASSERT_EQ(merged.slices.size(), 2u);
    EXPECT_EQ(merged.globalIndexOf[1], (std::vector<std::size_t>{1}));
    EXPECT_TRUE(merged.outsideBase[1].empty());
}

TEST(SliceMerge, MRG002_BaseChartSlicesComeFirst) {
    const auto merged = mergeSlicePlans({planOf({slice("a", 0, 100)}), planOf({slice("b", 0, 100)})}, 1);
    ASSERT_EQ(merged.slices.size(), 2u);
    EXPECT_EQ(merged.slices[0].source, "b");
    EXPECT_EQ(merged.slices[1].source, "a");
    EXPECT_EQ(merged.outsideBase[0], (std::vector<std::size_t>{1}));
}

TEST(SliceMerge, MRG003_ExtraSlicesFollowInChartOrder) {
    const auto merged = mergeSlicePlans({planOf({slice("a", 0, 100)}), planOf({slice("a", 0, 50)}),
                                         planOf({slice("a", 0, 30), slice("a", 0, 50)})},
                                        0);
    ASSERT_EQ(merged.slices.size(), 3u);
    EXPECT_EQ(merged.slices[1].endFrame, 50);
    EXPECT_EQ(merged.slices[2].endFrame, 30);
    EXPECT_TRUE(merged.outsideBase[0].empty());
    EXPECT_EQ(merged.outsideBase[1], (std::vector<std::size_t>{1}));
    EXPECT_EQ(merged.outsideBase[2], (std::vector<std::size_t>{1, 2}));
}

TEST(SliceMerge, MRG004_KeyUseInAnyChartMakesAKeySound) {
    const auto merged = mergeSlicePlans(
        {planOf({slice("a", 0, 100, SliceKind::Bgm, 0, 0)}), planOf({slice("a", 0, 100, SliceKind::Key, 480, 3)})}, 0);
    ASSERT_EQ(merged.slices.size(), 1u);
    EXPECT_EQ(merged.slices[0].kind, SliceKind::Key);
    EXPECT_EQ(merged.slices[0].firstPulse, 480);
    EXPECT_EQ(merged.slices[0].firstLane, 3);
}

TEST(SliceMerge, MRG005_IndexInKindRunsAcrossCharts) {
    const auto merged = mergeSlicePlans(
        {planOf({slice("a", 0, 100, SliceKind::Key, 0, 1), slice("b", 0, 100)}),
         planOf({slice("a", 0, 50, SliceKind::Key, 0, 1), slice("b", 0, 50)})},
        0);
    ASSERT_EQ(merged.slices.size(), 4u);
    EXPECT_EQ(merged.slices[0].indexInKind, 1);
    EXPECT_EQ(merged.slices[1].indexInKind, 1);
    EXPECT_EQ(merged.slices[2].indexInKind, 2);
    EXPECT_EQ(merged.slices[2].kind, SliceKind::Key);
    EXPECT_EQ(merged.slices[3].indexInKind, 2);
    EXPECT_EQ(merged.slices[3].kind, SliceKind::Bgm);
}

TEST(SliceMerge, MRG006_PlanForPointsNotesAtGlobalSlices) {
    SlicePlan second = planOf({slice("c", 0, 10)});
    second.notes = {PlacedNote{0, 2, 480, 240}};
    const std::vector<SlicePlan> plans{planOf({slice("a", 0, 100), slice("b", 0, 100)}), second};
    const auto merged = mergeSlicePlans(plans, 0);
    const SlicePlan plan = merged.planFor(plans[1], 1);
    EXPECT_EQ(plan.slices.size(), 3u);
    ASSERT_EQ(plan.notes.size(), 1u);
    EXPECT_EQ(plan.notes[0], (PlacedNote{2, 2, 480, 240}));
}

TEST(SliceMerge, MRG007_BaseOutOfRangeThrows) {
    EXPECT_THROW((void)mergeSlicePlans({planOf({}), planOf({})}, 2), std::invalid_argument);
}

namespace {

SlicePlan planWith(std::vector<Slice> slices, std::vector<PlacedNote> notes) {
    SlicePlan plan = planOf(std::move(slices));
    plan.notes = std::move(notes);
    return plan;
}

std::vector<TimingMap> timingsFor(std::size_t charts) {
    return std::vector<TimingMap>(charts, bmson::timingOf(b2b::test::makeChart()));
}

}  // namespace

TEST(SliceMerge, MRG008_LongerSliceIsCutWhereAnotherChartCuts) {
    std::vector<SlicePlan> plans{
        planWith({slice("a", 0, 1000, SliceKind::Key)}, {PlacedNote{0, 1, 0, 0}}),
        planWith({slice("a", 0, 500, SliceKind::Key), slice("a", 500, 1000, SliceKind::Key)},
                 {PlacedNote{0, 1, 0, 0}, PlacedNote{1, 2, 240, 0}})};
    const auto alignments = alignSliceCuts(plans, timingsFor(2));
    EXPECT_EQ(alignments[0].shortenedNotes, 1);
    EXPECT_EQ(alignments[0].addedNotes, 1);
    EXPECT_EQ(alignments[1].shortenedNotes, 0);
    ASSERT_EQ(plans[0].notes.size(), 2u);
    const auto& first = plans[0].slices[plans[0].notes[0].slice];
    const auto& rest = plans[0].slices[plans[0].notes[1].slice];
    EXPECT_EQ(plans[0].notes[0].lane, 1);
    EXPECT_EQ(first.endFrame, 500);
    EXPECT_EQ(plans[0].notes[1].lane, 0);
    EXPECT_EQ(plans[0].notes[1].y, 240);
    EXPECT_EQ(rest.startFrame, 500);
    EXPECT_EQ(rest.endFrame, 1000);
    EXPECT_EQ(rest.kind, SliceKind::Bgm);
}

TEST(SliceMerge, MRG009_CutOfAnotherTimelineIsIgnored) {
    std::vector<SlicePlan> plans{planWith({slice("a", 0, 1000, SliceKind::Key)}, {PlacedNote{0, 1, 0, 0}}),
                                 planWith({slice("a", 200, 1000, SliceKind::Key)}, {PlacedNote{0, 1, 240, 0}})};
    const auto alignments = alignSliceCuts(plans, timingsFor(2));
    EXPECT_EQ(alignments[0].shortenedNotes, 0);
    EXPECT_EQ(plans[0].slices[0].endFrame, 1000);
}

TEST(SliceMerge, MRG010_ChordGetsOneContinuation) {
    std::vector<SlicePlan> plans{
        planWith({slice("a", 0, 1000, SliceKind::Key)}, {PlacedNote{0, 1, 0, 0}, PlacedNote{0, 2, 0, 0}}),
        planWith({slice("a", 0, 500), slice("a", 500, 1000)}, {PlacedNote{0, 0, 0, 0}, PlacedNote{1, 0, 240, 0}})};
    const auto alignments = alignSliceCuts(plans, timingsFor(2));
    EXPECT_EQ(alignments[0].shortenedNotes, 2);
    EXPECT_EQ(alignments[0].addedNotes, 1);
    EXPECT_EQ(plans[0].notes.size(), 3u);
}

TEST(SliceMerge, MRG011_AlignedChartsShareEverySlice) {
    std::vector<SlicePlan> plans{
        planWith({slice("a", 0, 1000, SliceKind::Key)}, {PlacedNote{0, 1, 0, 0}}),
        planWith({slice("a", 0, 500, SliceKind::Key), slice("a", 500, 1000, SliceKind::Key)},
                 {PlacedNote{0, 1, 0, 0}, PlacedNote{1, 2, 240, 0}})};
    static_cast<void>(alignSliceCuts(plans, timingsFor(2)));
    const auto merged = mergeSlicePlans(plans, 0);
    EXPECT_EQ(merged.slices.size(), 2u);
    EXPECT_TRUE(merged.outsideBase[1].empty());
    EXPECT_EQ(merged.slices[1].kind, SliceKind::Key);
}
