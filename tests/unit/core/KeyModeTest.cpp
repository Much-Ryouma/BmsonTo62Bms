#include <gtest/gtest.h>

#include "b2b/core/bms/KeyMode.hpp"

using namespace b2b::core;

TEST(KeyMode, KEY001_Beat7kIsTheDefault) {
    ASSERT_NE(findKeyMode(""), nullptr);
    EXPECT_EQ(findKeyMode("")->name, "beat-7k");
    EXPECT_EQ(findKeyMode("beat-7k")->lanes, 8);
}

TEST(KeyMode, KEY002_Beat14kMapsBothSides) {
    const KeyMode& mode = *findKeyMode("beat-14k");
    const std::vector<std::string> expected = {"11", "12", "13", "14", "15", "18", "19", "16",
                                               "21", "22", "23", "24", "25", "28", "29", "26"};
    for (int lane = 1; lane <= 16; ++lane) {
        EXPECT_EQ(channelForLane(mode, lane, false), expected[static_cast<std::size_t>(lane - 1)]) << lane;
    }
    EXPECT_EQ(channelForLane(mode, 8, true), "56");
    EXPECT_EQ(channelForLane(mode, 16, true), "66");
    EXPECT_EQ(mode.player, 3);
    EXPECT_EQ(mode.extension, "bme");
}

TEST(KeyMode, KEY003_Popn9kUsesTheSecondRowForButtons6To9) {
    const KeyMode& mode = *findKeyMode("popn-9k");
    const std::vector<std::string> expected = {"11", "12", "13", "14", "15", "22", "23", "24", "25"};
    for (int lane = 1; lane <= 9; ++lane) {
        EXPECT_EQ(channelForLane(mode, lane, false), expected[static_cast<std::size_t>(lane - 1)]) << lane;
    }
    EXPECT_EQ(channelForLane(mode, 9, true), "65");
    EXPECT_EQ(mode.player, 1);
    EXPECT_EQ(mode.extension, "pms");
    EXPECT_EQ(mode.normalJudgePercent, 70);
}

TEST(KeyMode, KEY004_UnknownModesAreNotSupported) {
    EXPECT_EQ(findKeyMode("keyboard-24k"), nullptr);
    EXPECT_THROW(static_cast<void>(channelForLane(*findKeyMode("popn-9k"), 10, false)), std::out_of_range);
}
