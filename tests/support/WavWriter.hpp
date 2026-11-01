#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace b2b::test {

inline void writePcmWav(const std::filesystem::path& path, int sampleRate, int channels, int bits,
                        const std::vector<std::uint8_t>& data) {
    const auto dataBytes = static_cast<std::uint32_t>(data.size());
    const auto blockAlign = static_cast<std::uint16_t>(channels * bits / 8);
    std::ofstream out(path, std::ios::binary);
    auto u32 = [&out](std::uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); };
    auto u16 = [&out](std::uint16_t value) { out.write(reinterpret_cast<const char*>(&value), 2); };
    out.write("RIFF", 4);
    u32(36 + dataBytes);
    out.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(static_cast<std::uint16_t>(channels));
    u32(static_cast<std::uint32_t>(sampleRate));
    u32(static_cast<std::uint32_t>(sampleRate) * blockAlign);
    u16(blockAlign);
    u16(static_cast<std::uint16_t>(bits));
    out.write("data", 4);
    u32(dataBytes);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

inline void writeSilentWav(const std::filesystem::path& path, int sampleRate, int channels, std::int64_t frames) {
    writePcmWav(path, sampleRate, channels, 16, std::vector<std::uint8_t>(static_cast<std::size_t>(frames * channels * 2)));
}

inline std::vector<std::uint8_t> countingBytes(std::size_t size) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 37 + 11) & 0xFF);
    }
    return bytes;
}

}  // namespace b2b::test
