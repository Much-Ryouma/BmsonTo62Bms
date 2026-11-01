#pragma once

#include <cstdint>
#include <vector>

#include "b2b/core/audio/Pcm.hpp"

namespace b2b::core {

struct StereoAudio {
    int sampleRate = 0;
    std::vector<float> samples;

    [[nodiscard]] std::int64_t frames() const noexcept { return static_cast<std::int64_t>(samples.size() / 2); }
    [[nodiscard]] double seconds() const noexcept;
};

[[nodiscard]] StereoAudio stereoOf(const PcmBuffer& buffer);

[[nodiscard]] StereoAudio resampled(const StereoAudio& audio, int sampleRate);

void mixAt(StereoAudio& output, const StereoAudio& sound, double startSeconds);

}  // namespace b2b::core
