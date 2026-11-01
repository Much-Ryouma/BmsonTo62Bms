#include <gtest/gtest.h>

#include "b2b/core/timing/TimingMap.hpp"

using namespace b2b::core;

TEST(TimingMap, TIM001_ConstantTempo) {
    const TimingMap timing(120.0, 240);
    EXPECT_DOUBLE_EQ(timing.secondsAt(480), 1.0);
}

TEST(TimingMap, TIM002_TempoChange) {
    const TimingMap timing(120.0, 240, {{480, 240.0}});
    EXPECT_DOUBLE_EQ(timing.secondsAt(960), 1.5);
}

TEST(TimingMap, TIM003_StopDelaysLaterNotesButNotTheNoteOnIt) {
    const TimingMap timing(120.0, 240, {}, {{480, 240}});
    EXPECT_DOUBLE_EQ(timing.secondsAt(480), 1.0);
    EXPECT_DOUBLE_EQ(timing.secondsAt(720), 2.0);
}
