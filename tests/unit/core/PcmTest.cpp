#include <gtest/gtest.h>

#include <cstring>

#include "b2b/core/audio/Pcm.hpp"

using namespace b2b::core;

namespace {

PcmBuffer bufferOf(SampleFormat format, std::vector<std::uint8_t> bytes, int channels = 1) {
    PcmBuffer buffer;
    buffer.sampleRate = 1000;
    buffer.channels = channels;
    buffer.format = format;
    buffer.bytes = std::move(bytes);
    return buffer;
}

std::vector<std::uint8_t> floatBytes(float value) {
    std::vector<std::uint8_t> bytes(4);
    std::memcpy(bytes.data(), &value, 4);
    return bytes;
}

}  // namespace

TEST(Pcm, PCM001_SamplesAreNormalized) {
    EXPECT_DOUBLE_EQ(sampleAt(bufferOf(SampleFormat::U8, {0x80}), 0), 0.0);
    EXPECT_DOUBLE_EQ(sampleAt(bufferOf(SampleFormat::S16, {0x00, 0x80}), 0), -1.0);
    EXPECT_DOUBLE_EQ(sampleAt(bufferOf(SampleFormat::S24, {0x00, 0x00, 0x40}), 0), 0.5);
    EXPECT_DOUBLE_EQ(sampleAt(bufferOf(SampleFormat::S32, {0x00, 0x00, 0x00, 0x40}), 0), 0.5);
    EXPECT_DOUBLE_EQ(sampleAt(bufferOf(SampleFormat::F32, floatBytes(0.25f)), 0), 0.25);
}

TEST(Pcm, PCM002_ReadingAndWritingBackKeepsEveryValue) {
    for (const SampleFormat format : {SampleFormat::S16, SampleFormat::S24}) {
        const int width = bytesPerSample(format);
        const std::uint32_t count = width == 2 ? 0x10000u : 0x1000000u;
        PcmBuffer buffer = bufferOf(format, std::vector<std::uint8_t>(static_cast<std::size_t>(count) * static_cast<std::size_t>(width)));
        for (std::uint32_t value = 0; value < count; ++value) {
            std::memcpy(buffer.bytes.data() + static_cast<std::size_t>(value) * static_cast<std::size_t>(width), &value,
                        static_cast<std::size_t>(width));
        }
        const std::vector<std::uint8_t> original = buffer.bytes;
        for (std::size_t index = 0; index < buffer.sampleCount(); ++index) {
            setSample(buffer, index, sampleAt(buffer, index));
        }
        EXPECT_EQ(buffer.bytes, original);
    }
}

TEST(Pcm, PCM003_WritingBeyondFullScaleClamps) {
    PcmBuffer buffer = bufferOf(SampleFormat::S16, {0, 0, 0, 0});
    setSample(buffer, 0, 1.5);
    setSample(buffer, 1, -1.5);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 0), 32767.0 / 32768.0);
    EXPECT_DOUBLE_EQ(sampleAt(buffer, 1), -1.0);
}

TEST(Pcm, PCM004_ExtractCopiesTheFramesAsIs) {
    std::vector<std::uint8_t> bytes(60);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(index);
    }
    const PcmBuffer buffer = bufferOf(SampleFormat::S24, bytes, 2);
    const PcmBuffer slice = extractFrames(buffer, 3, 7);
    EXPECT_EQ(slice.frames(), 4);
    EXPECT_EQ(slice.bytes, std::vector<std::uint8_t>(bytes.begin() + 18, bytes.begin() + 42));
    EXPECT_EQ(slice.format, SampleFormat::S24);
    EXPECT_EQ(slice.channels, 2);
}

TEST(Pcm, PCM005_ExtractClampsToTheBuffer) {
    const PcmBuffer buffer = bufferOf(SampleFormat::S16, std::vector<std::uint8_t>(20));
    EXPECT_EQ(extractFrames(buffer, -5, 100).frames(), 10);
}

TEST(Pcm, PCM006_ThirtyTwoBitSamplesWithEmptyLowBytesFitIn24Bits) {
    EXPECT_TRUE(fitsIn24Bits(bufferOf(SampleFormat::S32, {0x00, 0x56, 0x34, 0x12})));
    EXPECT_FALSE(fitsIn24Bits(bufferOf(SampleFormat::S32, {0x00, 0x56, 0x34, 0x12, 0x01, 0x56, 0x34, 0x12})));
}

TEST(Pcm, PCM007_NarrowingKeepsTheUpper24Bits) {
    const PcmBuffer narrowed = narrowTo24Bits(bufferOf(SampleFormat::S32, {0x00, 0x56, 0x34, 0x12}));
    EXPECT_EQ(narrowed.format, SampleFormat::S24);
    EXPECT_EQ(narrowed.bytes, (std::vector<std::uint8_t>{0x56, 0x34, 0x12}));
}
