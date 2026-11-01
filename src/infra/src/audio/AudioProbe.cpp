#include "b2b/infra/audio/AudioProbe.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "b2b/infra/fs/Utf8Path.hpp"
#include "miniaudio.h"

namespace b2b::infra {

namespace {

class DecoderHandle {
public:
    explicit DecoderHandle(const std::filesystem::path& path) {
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#if defined(_WIN32)
        // Why not ma_decoder_init_file: 文字列が CP932 として解釈され、日本語のファイル名を開けないため
        const ma_result result = ma_decoder_init_file_w(path.c_str(), &config, &decoder_);
#else
        const ma_result result = ma_decoder_init_file(path.c_str(), &config, &decoder_);
#endif
        if (result != MA_SUCCESS) {
            throw AudioDecodeError("音声ファイルを開けません: " + pathToUtf8(path) + " (" +
                                   ma_result_description(result) + ")");
        }
    }
    ~DecoderHandle() { ma_decoder_uninit(&decoder_); }
    DecoderHandle(const DecoderHandle&) = delete;
    DecoderHandle& operator=(const DecoderHandle&) = delete;

    ma_decoder& get() noexcept { return decoder_; }

private:
    ma_decoder decoder_{};
};

}  // namespace

core::AudioInfo probeAudioFile(const std::filesystem::path& path) {
    DecoderHandle handle(path);
    ma_decoder& decoder = handle.get();
    if (decoder.outputSampleRate == 0 || decoder.outputChannels == 0) {
        throw AudioDecodeError("音声ファイルの形式を判別できません: " + pathToUtf8(path));
    }

    // Why not ma_decoder_get_length_in_pcm_frames を使う: ogg や mp3 では長さが推定値のことがあり、
    // 最後のスライスの終わりがずれるため、実際に最後まで読んで数える
    constexpr ma_uint64 kChunkFrames = 4096;
    std::array<float, kChunkFrames * 8> chunk{};
    const ma_uint64 framesPerChunk = std::min<ma_uint64>(kChunkFrames, chunk.size() / decoder.outputChannels);
    ma_uint64 total = 0;
    for (;;) {
        ma_uint64 framesRead = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), framesPerChunk, &framesRead);
        total += framesRead;
        if (result == MA_AT_END || framesRead == 0) {
            break;
        }
        if (result != MA_SUCCESS) {
            throw AudioDecodeError("音声ファイルの読み込みに失敗しました: " + pathToUtf8(path) + " (" +
                                   ma_result_description(result) + ")");
        }
    }
    return core::AudioInfo{static_cast<int>(decoder.outputSampleRate), static_cast<std::int64_t>(total)};
}

}  // namespace b2b::infra
