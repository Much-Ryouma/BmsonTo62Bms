#include "b2b/core/audio/Pcm.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace b2b::core {

namespace {

constexpr double kU8Scale = 128.0;
constexpr double kS16Scale = 32768.0;
constexpr double kS24Scale = 8388608.0;
constexpr double kS32Scale = 2147483648.0;

std::int32_t readS24(const std::uint8_t* at) {
    const auto value = static_cast<std::uint32_t>(at[0]) | (static_cast<std::uint32_t>(at[1]) << 8) |
                       (static_cast<std::uint32_t>(at[2]) << 16);
    return static_cast<std::int32_t>(value << 8) >> 8;
}

void writeS24(std::uint8_t* at, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    at[0] = static_cast<std::uint8_t>(bits & 0xFF);
    at[1] = static_cast<std::uint8_t>((bits >> 8) & 0xFF);
    at[2] = static_cast<std::uint8_t>((bits >> 16) & 0xFF);
}

template <typename Integer>
Integer readLittle(const std::uint8_t* at) {
    Integer value{};
    std::memcpy(&value, at, sizeof(Integer));
    return value;
}

template <typename Integer>
void writeLittle(std::uint8_t* at, Integer value) {
    std::memcpy(at, &value, sizeof(Integer));
}

double quantize(double value, double scale, double minimum, double maximum) {
    return std::clamp(std::round(value * scale), minimum, maximum);
}

}  // namespace

int bytesPerSample(SampleFormat format) noexcept {
    switch (format) {
        case SampleFormat::U8: return 1;
        case SampleFormat::S16: return 2;
        case SampleFormat::S24: return 3;
        case SampleFormat::S32: return 4;
        case SampleFormat::F32: return 4;
    }
    return 2;
}

int bitsPerSample(SampleFormat format) noexcept {
    return bytesPerSample(format) * 8;
}

std::size_t PcmBuffer::frameBytes() const noexcept {
    return static_cast<std::size_t>(channels) * static_cast<std::size_t>(bytesPerSample(format));
}

std::int64_t PcmBuffer::frames() const noexcept {
    const std::size_t size = frameBytes();
    return size == 0 ? 0 : static_cast<std::int64_t>(bytes.size() / size);
}

std::size_t PcmBuffer::sampleCount() const noexcept {
    return static_cast<std::size_t>(frames()) * static_cast<std::size_t>(channels);
}

double sampleAt(const PcmBuffer& buffer, std::size_t index) {
    const std::uint8_t* at = buffer.bytes.data() + index * static_cast<std::size_t>(bytesPerSample(buffer.format));
    switch (buffer.format) {
        case SampleFormat::U8: return (static_cast<double>(*at) - kU8Scale) / kU8Scale;
        case SampleFormat::S16: return static_cast<double>(readLittle<std::int16_t>(at)) / kS16Scale;
        case SampleFormat::S24: return static_cast<double>(readS24(at)) / kS24Scale;
        case SampleFormat::S32: return static_cast<double>(readLittle<std::int32_t>(at)) / kS32Scale;
        case SampleFormat::F32: return static_cast<double>(readLittle<float>(at));
    }
    return 0.0;
}

void setSample(PcmBuffer& buffer, std::size_t index, double value) {
    std::uint8_t* at = buffer.bytes.data() + index * static_cast<std::size_t>(bytesPerSample(buffer.format));
    switch (buffer.format) {
        case SampleFormat::U8:
            *at = static_cast<std::uint8_t>(quantize(value, kU8Scale, -128.0, 127.0) + kU8Scale);
            return;
        case SampleFormat::S16:
            writeLittle(at, static_cast<std::int16_t>(quantize(value, kS16Scale, -32768.0, 32767.0)));
            return;
        case SampleFormat::S24:
            writeS24(at, static_cast<std::int32_t>(quantize(value, kS24Scale, -8388608.0, 8388607.0)));
            return;
        case SampleFormat::S32:
            writeLittle(at, static_cast<std::int32_t>(quantize(value, kS32Scale, -2147483648.0, 2147483647.0)));
            return;
        case SampleFormat::F32:
            writeLittle(at, static_cast<float>(value));
            return;
    }
}

PcmBuffer extractFrames(const PcmBuffer& buffer, std::int64_t startFrame, std::int64_t endFrame) {
    const std::int64_t total = buffer.frames();
    const std::int64_t start = std::clamp<std::int64_t>(startFrame, 0, total);
    const std::int64_t end = std::clamp<std::int64_t>(endFrame, start, total);
    PcmBuffer slice;
    slice.sampleRate = buffer.sampleRate;
    slice.channels = buffer.channels;
    slice.format = buffer.format;
    const auto first = buffer.bytes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(start) * buffer.frameBytes());
    const auto last = buffer.bytes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(end) * buffer.frameBytes());
    slice.bytes.assign(first, last);
    return slice;
}

std::vector<float> toFloat(const PcmBuffer& buffer) {
    std::vector<float> samples(buffer.sampleCount());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<float>(sampleAt(buffer, index));
    }
    return samples;
}

bool fitsIn24Bits(const PcmBuffer& buffer) {
    if (buffer.format != SampleFormat::S32) {
        return false;
    }
    for (std::size_t index = 0; index < buffer.sampleCount(); ++index) {
        if ((readLittle<std::int32_t>(buffer.bytes.data() + index * 4) & 0xFF) != 0) {
            return false;
        }
    }
    return true;
}

PcmBuffer narrowTo24Bits(const PcmBuffer& buffer) {
    if (buffer.format != SampleFormat::S32) {
        throw std::invalid_argument("only 32-bit integer samples can be narrowed to 24 bits");
    }
    PcmBuffer narrowed;
    narrowed.sampleRate = buffer.sampleRate;
    narrowed.channels = buffer.channels;
    narrowed.format = SampleFormat::S24;
    narrowed.bytes.resize(buffer.sampleCount() * 3);
    for (std::size_t index = 0; index < buffer.sampleCount(); ++index) {
        writeS24(narrowed.bytes.data() + index * 3, readLittle<std::int32_t>(buffer.bytes.data() + index * 4) >> 8);
    }
    return narrowed;
}

}  // namespace b2b::core
