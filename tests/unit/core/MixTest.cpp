#include <gtest/gtest.h>

#include "b2b/core/audio/Mix.hpp"

using namespace b2b::core;

namespace {

StereoAudio stereo(int rate, std::vector<float> leftAndRight) {
    StereoAudio audio;
    audio.sampleRate = rate;
    for (const float value : leftAndRight) {
        audio.samples.push_back(value);
        audio.samples.push_back(value);
    }
    return audio;
}

}  // namespace

TEST(Mix, MIX001_MonoIsCopiedToBothSides) {
    PcmBuffer mono;
    mono.sampleRate = 1000;
    mono.channels = 1;
    mono.format = SampleFormat::S16;
    mono.bytes = {0x00, 0x40};
    const StereoAudio audio = stereoOf(mono);
    ASSERT_EQ(audio.frames(), 1);
    EXPECT_FLOAT_EQ(audio.samples[0], 0.5f);
    EXPECT_FLOAT_EQ(audio.samples[1], 0.5f);
}

TEST(Mix, MIX002_OnlyTheFirstTwoChannelsAreUsed) {
    PcmBuffer surround;
    surround.sampleRate = 1000;
    surround.channels = 3;
    surround.format = SampleFormat::S16;
    surround.bytes = {0x00, 0x20, 0x00, 0x40, 0x00, 0x60};
    const StereoAudio audio = stereoOf(surround);
    ASSERT_EQ(audio.frames(), 1);
    EXPECT_FLOAT_EQ(audio.samples[0], 0.25f);
    EXPECT_FLOAT_EQ(audio.samples[1], 0.5f);
}

TEST(Mix, MIX003_ResamplingInterpolatesLinearly) {
    const StereoAudio audio = resampled(stereo(1000, {0, 1, 2, 3}), 2000);
    EXPECT_EQ(audio.sampleRate, 2000);
    const std::vector<float> expected = {0, 0.5f, 1, 1.5f, 2, 2.5f, 3};
    ASSERT_EQ(audio.frames(), static_cast<std::int64_t>(expected.size()));
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_FLOAT_EQ(audio.samples[index * 2], expected[index]);
    }
}

TEST(Mix, MIX004_OverlappingSoundsAreAdded) {
    StereoAudio output;
    output.sampleRate = 1000;
    const StereoAudio sound = stereo(1000, {0.5f, 0.5f});
    mixAt(output, sound, 0.002);
    mixAt(output, sound, 0.003);
    ASSERT_EQ(output.frames(), 5);
    EXPECT_FLOAT_EQ(output.samples[2 * 2], 0.5f);
    EXPECT_FLOAT_EQ(output.samples[3 * 2], 1.0f);
    EXPECT_FLOAT_EQ(output.samples[4 * 2], 0.5f);
}

TEST(Mix, MIX005_SoundsAreConvertedToTheOutputRate) {
    StereoAudio output;
    output.sampleRate = 2000;
    mixAt(output, stereo(1000, {0, 1}), 0.0);
    ASSERT_EQ(output.frames(), 3);
    EXPECT_FLOAT_EQ(output.samples[1 * 2], 0.5f);
}
