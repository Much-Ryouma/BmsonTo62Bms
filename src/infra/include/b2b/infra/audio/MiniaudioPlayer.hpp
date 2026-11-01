#pragma once

#include <memory>
#include <string>

#include "b2b/core/ports/AudioPlayer.hpp"

namespace b2b::infra {

class MiniaudioPlayer final : public core::AudioPlayer {
public:
    explicit MiniaudioPlayer(int preferredSampleRate = 48000);
    ~MiniaudioPlayer() override;

    MiniaudioPlayer(const MiniaudioPlayer&) = delete;
    MiniaudioPlayer& operator=(const MiniaudioPlayer&) = delete;

    [[nodiscard]] int sampleRate() const override;
    bool start(core::StereoAudio audio) override;
    void stop() override;
    [[nodiscard]] bool isPlaying() const override;
    [[nodiscard]] std::int64_t playedFrames() const override;

    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }

    struct Device;

private:
    bool ensureDevice();

    int preferredSampleRate_;
    std::unique_ptr<Device> device_;
    std::string lastError_;
};

}  // namespace b2b::infra
