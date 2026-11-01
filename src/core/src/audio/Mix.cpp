#include "b2b/core/audio/Mix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace b2b::core {

namespace {

float interpolated(const StereoAudio& audio, double position, std::size_t channel) {
    const auto last = static_cast<std::size_t>(audio.frames() - 1);
    const auto index = std::min(static_cast<std::size_t>(position), last);
    const std::size_t next = std::min(index + 1, last);
    const auto fraction = static_cast<float>(position - static_cast<double>(index));
    const float a = audio.samples[index * 2 + channel];
    const float b = audio.samples[next * 2 + channel];
    return a + (b - a) * fraction;
}

}  // namespace

double StereoAudio::seconds() const noexcept {
    return sampleRate > 0 ? static_cast<double>(frames()) / sampleRate : 0.0;
}

StereoAudio stereoOf(const PcmBuffer& buffer) {
    StereoAudio stereo;
    stereo.sampleRate = buffer.sampleRate;
    const auto frames = static_cast<std::size_t>(buffer.frames());
    const auto channels = static_cast<std::size_t>(buffer.channels);
    stereo.samples.resize(frames * 2);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t base = frame * channels;
        const auto left = static_cast<float>(sampleAt(buffer, base));
        // Why not 3 ch 以上をすべて混ぜる: 試聴の目的は区間の確認なので、左右の 2 ch だけで足りる
        const auto right = channels >= 2 ? static_cast<float>(sampleAt(buffer, base + 1)) : left;
        stereo.samples[frame * 2] = left;
        stereo.samples[frame * 2 + 1] = right;
    }
    return stereo;
}

StereoAudio resampled(const StereoAudio& audio, int sampleRate) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample rate must be positive");
    }
    if (audio.sampleRate == sampleRate || audio.frames() == 0) {
        StereoAudio copy = audio;
        copy.sampleRate = sampleRate;
        return copy;
    }
    const double ratio = static_cast<double>(audio.sampleRate) / sampleRate;
    const auto frames = static_cast<std::size_t>(std::floor(static_cast<double>(audio.frames() - 1) / ratio)) + 1;
    StereoAudio result;
    result.sampleRate = sampleRate;
    result.samples.resize(frames * 2);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double position = static_cast<double>(frame) * ratio;
        result.samples[frame * 2] = interpolated(audio, position, 0);
        result.samples[frame * 2 + 1] = interpolated(audio, position, 1);
    }
    return result;
}

void mixAt(StereoAudio& output, const StereoAudio& sound, double startSeconds) {
    if (output.sampleRate <= 0) {
        throw std::invalid_argument("output sample rate must be positive");
    }
    const StereoAudio converted = sound.sampleRate == output.sampleRate ? sound : resampled(sound, output.sampleRate);
    const auto first = static_cast<std::size_t>(std::max<std::int64_t>(0, std::llround(startSeconds * output.sampleRate)));
    const std::size_t needed = (first + static_cast<std::size_t>(converted.frames())) * 2;
    if (output.samples.size() < needed) {
        output.samples.resize(needed, 0.0f);
    }
    for (std::size_t index = 0; index < converted.samples.size(); ++index) {
        output.samples[first * 2 + index] += converted.samples[index];
    }
}

}  // namespace b2b::core
