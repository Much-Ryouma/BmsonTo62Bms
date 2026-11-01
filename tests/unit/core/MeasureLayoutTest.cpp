#include <gtest/gtest.h>

#include "b2b/core/timing/MeasureLayout.hpp"

using namespace b2b::core;

TEST(MeasureLayout, MEA001_WithoutLinesEveryMeasureIsFourBeats) {
    const MeasureLayout layout(240, {});
    EXPECT_EQ(layout.locate(1000), (MeasurePosition{1, 40}));
}

TEST(MeasureLayout, MEA002_LinesDefineIrregularMeasures) {
    const MeasureLayout layout(240, {0, 960, 1680});
    EXPECT_EQ(layout.locate(1700), (MeasurePosition{2, 20}));
    EXPECT_EQ(layout.lengthOf(1), 720);
}

TEST(MeasureLayout, MEA003_FourBeatMeasuresContinueAfterTheLastLine) {
    const MeasureLayout layout(240, {0, 720});
    EXPECT_EQ(layout.startOf(1), 720);
    EXPECT_EQ(layout.startOf(2), 1680);
    EXPECT_EQ(layout.locate(1700), (MeasurePosition{2, 20}));
}

TEST(MeasureLayout, MEA004_ZeroIsAddedWhenMissing) {
    const MeasureLayout layout(240, {480});
    EXPECT_EQ(layout.lengthOf(0), 480);
}

TEST(MeasureLayout, MEA005_NegativeAndDuplicateLinesAreIgnored) {
    const MeasureLayout layout(240, {960, -10, 960});
    EXPECT_EQ(layout.startOf(0), 0);
    EXPECT_EQ(layout.startOf(1), 960);
    EXPECT_EQ(layout.startOf(2), 1920);
}
