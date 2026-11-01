#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <numbers>

#include "WavWriter.hpp"
#include "b2b/infra/audio/AudioFiles.hpp"

using namespace b2b;

namespace {

std::filesystem::path scratchDirectory(const char* name) {
    auto directory = std::filesystem::temp_directory_path() / "b2b_tests" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

core::PcmBuffer bufferOf(core::SampleFormat format, int channels, std::size_t frames) {
    core::PcmBuffer buffer;
    buffer.sampleRate = 48000;
    buffer.channels = channels;
    buffer.format = format;
    buffer.bytes = test::countingBytes(frames * buffer.frameBytes());
    if (format == core::SampleFormat::F32) {
        for (std::size_t index = 0; index < buffer.sampleCount(); ++index) {
            core::setSample(buffer, index, std::sin(static_cast<double>(index)) * 0.5);
        }
    }
    return buffer;
}

}  // namespace

TEST(AudioFiles, ACD001_NativeDecodingKeeps16BitSamples) {
    const auto path = scratchDirectory("acd001") / "a.wav";
    const auto data = test::countingBytes(400);
    test::writePcmWav(path, 44100, 2, 16, data);
    const auto decoded = infra::decodePcm(path, infra::DecodeFormat::Native);
    EXPECT_EQ(decoded.format, core::SampleFormat::S16);
    EXPECT_EQ(decoded.channels, 2);
    EXPECT_EQ(decoded.sampleRate, 44100);
    EXPECT_EQ(decoded.bytes, data);
}

TEST(AudioFiles, ACD002_WavRoundTripKeepsFormatAndBytes) {
    const auto directory = scratchDirectory("acd002");
    for (const auto format : {core::SampleFormat::S16, core::SampleFormat::S24, core::SampleFormat::S32,
                              core::SampleFormat::U8, core::SampleFormat::F32}) {
        const auto original = bufferOf(format, 2, 301);
        const auto path = directory / ("f" + std::to_string(static_cast<int>(format)) + ".wav");
        infra::writeBinaryFile(path, infra::encodeWav(original));
        const auto decoded = infra::decodePcm(path, infra::DecodeFormat::Native);
        EXPECT_EQ(decoded.format, format);
        EXPECT_EQ(decoded.channels, 2);
        EXPECT_EQ(decoded.sampleRate, 48000);
        EXPECT_EQ(decoded.bytes, original.bytes) << static_cast<int>(format);
    }
}

TEST(AudioFiles, ACD003_FloatWavUsesTheFloatFormatTag) {
    const std::string bytes = infra::encodeWav(bufferOf(core::SampleFormat::F32, 1, 4));
    std::uint32_t fmtSize = 0;
    std::uint16_t tag = 0;
    std::memcpy(&fmtSize, bytes.data() + 16, 4);
    std::memcpy(&tag, bytes.data() + 20, 2);
    EXPECT_EQ(fmtSize, 18u);
    EXPECT_EQ(tag, 3);
}

TEST(AudioFiles, ACD004_FloatDecodingScales16BitValues) {
    const auto path = scratchDirectory("acd004") / "a.wav";
    test::writePcmWav(path, 44100, 1, 16, {0x00, 0x40, 0x00, 0xC0});
    const auto decoded = infra::decodePcm(path, infra::DecodeFormat::F32);
    ASSERT_EQ(decoded.format, core::SampleFormat::F32);
    EXPECT_FLOAT_EQ(static_cast<float>(core::sampleAt(decoded, 0)), 0.5f);
    EXPECT_FLOAT_EQ(static_cast<float>(core::sampleAt(decoded, 1)), -0.5f);
}

TEST(AudioFiles, ACD005_OggRoundTripIsClose) {
    core::PcmBuffer sine;
    sine.sampleRate = 44100;
    sine.channels = 2;
    sine.format = core::SampleFormat::F32;
    sine.bytes.resize(44100 * 2 * 4);
    for (std::size_t frame = 0; frame < 44100; ++frame) {
        const double value = 0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / 44100.0);
        core::setSample(sine, frame * 2, value);
        core::setSample(sine, frame * 2 + 1, value);
    }
    const auto path = scratchDirectory("acd005") / "a.ogg";
    infra::writeBinaryFile(path, infra::encodeOgg(sine, 5));
    const auto decoded = infra::decodePcm(path, infra::DecodeFormat::F32);
    EXPECT_EQ(decoded.sampleRate, 44100);
    EXPECT_EQ(decoded.channels, 2);
    ASSERT_EQ(decoded.frames(), 44100);
    double worst = 0.0;
    for (std::size_t index = 2000; index < 80000; ++index) {
        worst = std::max(worst, std::abs(core::sampleAt(decoded, index) - core::sampleAt(sine, index)));
    }
    EXPECT_LT(worst, 0.05);
}

TEST(AudioFiles, ACD006_EmptySlicesAreStillReadable) {
    const auto directory = scratchDirectory("acd006");
    const auto empty = bufferOf(core::SampleFormat::S16, 2, 0);
    infra::writeBinaryFile(directory / "a.wav", infra::encodeWav(empty));
    EXPECT_EQ(infra::decodePcm(directory / "a.wav", infra::DecodeFormat::Native).frames(), 0);
    infra::writeBinaryFile(directory / "a.ogg", infra::encodeOgg(empty, 5));
    EXPECT_EQ(infra::decodePcm(directory / "a.ogg", infra::DecodeFormat::F32).frames(), 0);
}
