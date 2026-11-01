#include "b2b/infra/audio/MiniaudioPlayer.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

#include "miniaudio.h"

namespace b2b::infra {

namespace {

constexpr ma_uint32 kChannels = 2;
constexpr ma_uint32 kPeriodMilliseconds = 10;

}  // namespace

struct MiniaudioPlayer::Device {
    ma_device device{};
    bool initialized = false;
    std::mutex bufferMutex;
    core::StereoAudio audio;
    std::atomic<std::int64_t> position{0};
    std::atomic<bool> playing{false};

    static void onData(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
        auto* self = static_cast<Device*>(device->pUserData);
        auto* out = static_cast<float*>(output);
        std::memset(out, 0, static_cast<std::size_t>(frameCount) * kChannels * sizeof(float));
        // Why not ロックを待つ: 音声スレッドが GUI スレッドの処理を待つと音切れになるため、取れなければその周期は無音にする
        std::unique_lock lock(self->bufferMutex, std::try_to_lock);
        if (!lock.owns_lock() || !self->playing.load()) {
            return;
        }
        const std::int64_t total = self->audio.frames();
        const std::int64_t begin = self->position.load();
        const std::int64_t count = std::min<std::int64_t>(frameCount, std::max<std::int64_t>(0, total - begin));
        if (count > 0) {
            std::memcpy(out, self->audio.samples.data() + begin * kChannels,
                        static_cast<std::size_t>(count) * kChannels * sizeof(float));
        }
        self->position.store(begin + count);
        if (begin + count >= total) {
            self->playing.store(false);
        }
    }
};

MiniaudioPlayer::MiniaudioPlayer(int preferredSampleRate)
    : preferredSampleRate_(preferredSampleRate), device_(std::make_unique<Device>()) {}

MiniaudioPlayer::~MiniaudioPlayer() {
    if (device_->initialized) {
        ma_device_uninit(&device_->device);
    }
}

// Why not 起動時にデバイスを開く: 試聴しない使い方（書き出しだけ、テスト）でも音声デバイスを占有し、
// デバイスがない環境では起動自体が失敗するため、最初の再生で開く
bool MiniaudioPlayer::ensureDevice() {
    if (device_->initialized) {
        return true;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = kChannels;
    config.sampleRate = static_cast<ma_uint32>(preferredSampleRate_);
    config.periodSizeInMilliseconds = kPeriodMilliseconds;
    config.dataCallback = &Device::onData;
    config.pUserData = device_.get();
    if (ma_device_init(nullptr, &config, &device_->device) != MA_SUCCESS) {
        lastError_ = "音声出力デバイスを開けません";
        return false;
    }
    if (ma_device_start(&device_->device) != MA_SUCCESS) {
        ma_device_uninit(&device_->device);
        lastError_ = "音声出力デバイスを開始できません";
        return false;
    }
    device_->initialized = true;
    return true;
}

int MiniaudioPlayer::sampleRate() const {
    return device_->initialized ? static_cast<int>(device_->device.sampleRate) : preferredSampleRate_;
}

bool MiniaudioPlayer::start(core::StereoAudio audio) {
    if (!ensureDevice()) {
        return false;
    }
    const int rate = sampleRate();
    core::StereoAudio converted = audio.sampleRate == rate ? std::move(audio) : core::resampled(audio, rate);
    std::lock_guard lock(device_->bufferMutex);
    device_->audio = std::move(converted);
    device_->position.store(0);
    device_->playing.store(device_->audio.frames() > 0);
    return true;
}

void MiniaudioPlayer::stop() {
    device_->playing.store(false);
}

bool MiniaudioPlayer::isPlaying() const {
    return device_->playing.load();
}

std::int64_t MiniaudioPlayer::playedFrames() const {
    return device_->position.load();
}

}  // namespace b2b::infra
