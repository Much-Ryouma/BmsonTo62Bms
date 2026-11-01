#include "b2b/core/audio/Fade.hpp"

#include <algorithm>
#include <cmath>

namespace b2b::core {

namespace {

constexpr double kMillisecondsPerSecond = 1000.0;

std::int64_t fadeFrames(double milliseconds, const PcmBuffer& buffer) {
    const auto frames = std::llround(milliseconds * buffer.sampleRate / kMillisecondsPerSecond);
    return std::clamp<std::int64_t>(frames, 0, buffer.frames() / 2);
}

void scaleFrame(PcmBuffer& buffer, std::int64_t frame, double gain) {
    const auto channels = static_cast<std::size_t>(buffer.channels);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        const std::size_t index = static_cast<std::size_t>(frame) * channels + channel;
        setSample(buffer, index, sampleAt(buffer, index) * gain);
    }
}

bool inRange(double milliseconds) {
    return milliseconds >= 0.0 && milliseconds <= kMaxFadeMs;
}

}  // namespace

std::optional<std::string> fadeSettingsError(const FadeSettings& settings) {
    if (!inRange(settings.fadeOutMs)) {
        return "フェードアウトの長さは 0〜50 ms で指定してください";
    }
    if (settings.fadeIn && !inRange(settings.fadeInMs)) {
        return "フェードインの長さは 0〜50 ms で指定してください";
    }
    return std::nullopt;
}

void applyFade(PcmBuffer& buffer, const FadeSettings& settings, bool startsAtFileStart) {
    if (settings.mode != EdgeMode::Fade) {
        return;
    }
    const std::int64_t frames = buffer.frames();
    const std::int64_t fadeOut = fadeFrames(settings.fadeOutMs, buffer);
    for (std::int64_t step = 0; step < fadeOut; ++step) {
        const double gain = static_cast<double>(fadeOut - 1 - step) / static_cast<double>(fadeOut);
        scaleFrame(buffer, frames - fadeOut + step, gain);
    }
    // Why not ファイルの先頭から鳴らすスライスにもかける: 元の音声の頭そのものなので切り口ではなく、アタックを弱めてしまうだけのため
    if (!settings.fadeIn || startsAtFileStart) {
        return;
    }
    const std::int64_t fadeIn = fadeFrames(settings.fadeInMs, buffer);
    for (std::int64_t step = 0; step < fadeIn; ++step) {
        scaleFrame(buffer, step, static_cast<double>(step) / static_cast<double>(fadeIn));
    }
}

}  // namespace b2b::core
