#include <gtest/gtest.h>

#include "b2b/core/bms/HeaderValues.hpp"

using namespace b2b::core;

TEST(HeaderValues, HDR001_StandardJudgeRankBecomesDefExRank134) {
    EXPECT_EQ(judgeHeaderFor(100.0), (JudgeHeader{JudgeHeaderKind::DefExRank, 134}));
    EXPECT_EQ(134 * 75 / 100, 100);
}

TEST(HeaderValues, HDR002_JudgeRank75BecomesDefExRank100) {
    EXPECT_EQ(judgeHeaderFor(75.0), (JudgeHeader{JudgeHeaderKind::DefExRank, 100}));
}

TEST(HeaderValues, HDR003_SmallJudgeRankIsRankAndZeroIsOmitted) {
    EXPECT_EQ(judgeHeaderFor(3.0), (JudgeHeader{JudgeHeaderKind::Rank, 3}));
    EXPECT_EQ(judgeHeaderFor(0.0).kind, JudgeHeaderKind::None);
}

TEST(HeaderValues, HDR004_StandardTotalIsOmitted) {
    EXPECT_FALSE(bmsTotalFor(100.0, 1000).has_value());
}

TEST(HeaderValues, HDR005_TotalIsScaledFromTheDefault) {
    EXPECT_NEAR(*bmsTotalFor(120.0, 1000), 1.2 * 7.605 * 1000.0 / 16.5, 1e-9);
}

TEST(HeaderValues, HDR006_DefaultTotalHasAFloorOf260) {
    EXPECT_DOUBLE_EQ(*bmsTotalFor(200.0, 50), 520.0);
}

TEST(HeaderValues, HDR007_DecimalsAreTrimmed) {
    EXPECT_EQ(formatDecimal(120.0), "120");
    EXPECT_EQ(formatDecimal(150.5), "150.5");
    EXPECT_EQ(formatDecimal(0.1 + 0.2), "0.3");
    EXPECT_EQ(formatDecimal(1.0 / 3.0), "0.333333");
    EXPECT_EQ(formatDecimal(-0.0), "0");
}

TEST(HeaderValues, HDR008_PopnUsesItsOwnNormalWindow) {
    EXPECT_EQ(judgeHeaderFor(100.0, 70), (JudgeHeader{JudgeHeaderKind::DefExRank, 143}));
    EXPECT_EQ(143 * 70 / 100, 100);
}
