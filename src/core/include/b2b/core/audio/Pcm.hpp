#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace b2b::core {

enum class SampleFormat { U8, S16, S24, S32, F32 };

[[nodiscard]] int bytesPerSample(SampleFormat format) noexcept;

[[nodiscard]] int bitsPerSample(SampleFormat format) noexcept;

struct PcmBuffer {
    int sampleRate = 0;
    int channels = 0;
    SampleFormat format = SampleFormat::S16;
    std::vector<std::uint8_t> bytes;

    [[nodiscard]] std::size_t frameBytes() const noexcept;
    [[nodiscard]] std::int64_t frames() const noexcept;
    [[nodiscard]] std::size_t sampleCount() const noexcept;
};

[[nodiscard]] double sampleAt(const PcmBuffer& buffer, std::size_t index);

void setSample(PcmBuffer& buffer, std::size_t index, double value);

[[nodiscard]] PcmBuffer extractFrames(const PcmBuffer& buffer, std::int64_t startFrame, std::int64_t endFrame);

[[nodiscard]] std::vector<float> toFloat(const PcmBuffer& buffer);

[[nodiscard]] bool fitsIn24Bits(const PcmBuffer& buffer);

[[nodiscard]] PcmBuffer narrowTo24Bits(const PcmBuffer& buffer);

}  // namespace b2b::core
