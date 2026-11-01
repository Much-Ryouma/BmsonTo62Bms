#include <gtest/gtest.h>

#include <cstring>

#include "b2b/core/audio/Fade.hpp"

using namespace b2b::core;

namespace {

PcmBuffer ones(std::int64_t frames) {
    PcmBuffer buffer;
    buffer.sampleRate = 1000;
    buffer.channels = 1;
    buffer.format = SampleFormat::F32;
    buffer.bytes.resize(static_cast<std::size_t>(frames) * 4);
    for (std::int64_t index = 0; index < frames; ++index) {
        setSample(buffer, static_cast<std::size_t>(index), 1.0);
    }
    return buffer;
}

FadeSettings fade(double outMs, bool in = false, double inMs = 0.0) {
    FadeSettings settings;
    settings.mode = EdgeMode::Fade;
    settings.fadeOutMs = outMs;
    settings.fadeIn = in;
    settings.fadeInMs = inMs;
    return settings;
}

}  // namespace

TEST(Fade, FAD001_CutLeavesTheSoundAlone) {
    PcmBuffer buffer = ones(100);
    const auto original = buffer.bytes;
    applyFade(buffer, FadeSettings{}, false);
    EXPECT_EQ(buffer.bytes, original);
}

TEST(Fade, FAD002_FadeOutRampsTheLastFramesToZero) {
    PcmBuffer buffer = ones(100);
    applyFade(buffer, fade(4.0), false);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 95), 1.0);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 96), 0.75);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 97), 0.5);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 98), 0.25);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 99), 0.0);
}

TEST(Fade, FAD003_FadeInRampsTheFirstFramesFromZero) {
    PcmBuffer buffer = ones(100);
    applyFade(buffer, fade(0.0, true, 4.0), false);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 0), 0.0);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 1), 0.25);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 2), 0.5);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 3), 0.75);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 4), 1.0);
}

TEST(Fade, FAD004_NoFadeInAtTheStartOfTheFile) {
    PcmBuffer buffer = ones(100);
    applyFade(buffer, fade(0.0, true, 4.0), true);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 0), 1.0);
}

TEST(Fade, FAD005_FadeIsAtMostHalfTheSlice) {
    PcmBuffer buffer = ones(10);
    applyFade(buffer, fade(50.0), false);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 4), 1.0);
    EXPECT_FLOAT_EQ(static_cast<float>(sampleAt(buffer, 5)), 0.8f);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 9), 0.0);
}

TEST(Fade, FAD006_LengthsMustBeWithin0To50Ms) {
    EXPECT_TRUE(fadeSettingsError(fade(51.0)).has_value());
    EXPECT_TRUE(fadeSettingsError(fade(-1.0)).has_value());
    EXPECT_TRUE(fadeSettingsError(fade(2.0, true, 60.0)).has_value());
    EXPECT_FALSE(fadeSettingsError(fade(0.0, true, 50.0)).has_value());
}
