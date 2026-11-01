#pragma once

#include <cstdint>

#include "b2b/core/audio/Mix.hpp"

namespace b2b::core {

class AudioPlayer {
public:
    virtual ~AudioPlayer() = default;
    [[nodiscard]] virtual int sampleRate() const = 0;
    virtual bool start(StereoAudio audio) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isPlaying() const = 0;
    [[nodiscard]] virtual std::int64_t playedFrames() const = 0;
};

}  // namespace b2b::core
