#pragma once

#include <filesystem>
#include <stdexcept>

#include "b2b/core/slicing/SlicePlan.hpp"

namespace b2b::infra {

class AudioDecodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] core::AudioInfo probeAudioFile(const std::filesystem::path& path);

}  // namespace b2b::infra
