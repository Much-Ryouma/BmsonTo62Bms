#pragma once

#include <filesystem>
#include <string>

#include "b2b/core/audio/Pcm.hpp"
#include "b2b/infra/audio/AudioProbe.hpp"

namespace b2b::infra {

enum class DecodeFormat { Native, S16, F32 };

class AudioEncodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] core::PcmBuffer decodePcm(const std::filesystem::path& path, DecodeFormat format);

[[nodiscard]] std::string encodeWav(const core::PcmBuffer& buffer);

[[nodiscard]] std::string encodeOgg(const core::PcmBuffer& buffer, int quality);

void writeBinaryFile(const std::filesystem::path& path, const std::string& bytes);

}  // namespace b2b::infra
