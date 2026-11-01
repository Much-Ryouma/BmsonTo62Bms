#include "b2b/infra/audio/AudioFiles.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include <vorbis/vorbisenc.h>

#include "b2b/infra/fs/Utf8Path.hpp"
#include "miniaudio.h"

namespace b2b::infra {

namespace {

constexpr ma_uint64 kChunkFrames = 4096;
constexpr int kOggSerialNumber = 0x62326232;
constexpr int kOggBlockFrames = 1024;
constexpr std::uint16_t kWavePcm = 1;
constexpr std::uint16_t kWaveFloat = 3;

class DecoderHandle {
public:
    DecoderHandle(const std::filesystem::path& path, ma_format format) {
        const ma_decoder_config config = ma_decoder_config_init(format, 0, 0);
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

ma_format requestedFormat(DecodeFormat format) {
    switch (format) {
        case DecodeFormat::Native: return ma_format_unknown;
        case DecodeFormat::S16: return ma_format_s16;
        case DecodeFormat::F32: return ma_format_f32;
    }
    return ma_format_unknown;
}

core::SampleFormat sampleFormatOf(ma_format format, const std::filesystem::path& path) {
    switch (format) {
        case ma_format_u8: return core::SampleFormat::U8;
        case ma_format_s16: return core::SampleFormat::S16;
        case ma_format_s24: return core::SampleFormat::S24;
        case ma_format_s32: return core::SampleFormat::S32;
        case ma_format_f32: return core::SampleFormat::F32;
        default: break;
    }
    throw AudioDecodeError("対応していないサンプル形式の音声です: " + pathToUtf8(path));
}

class ByteWriter {
public:
    void u16(std::uint16_t value) { append(&value, 2); }
    void u32(std::uint32_t value) { append(&value, 4); }
    void text(const char* value) { bytes_.append(value, 4); }
    void append(const void* data, std::size_t size) { bytes_.append(static_cast<const char*>(data), size); }
    std::string take() { return std::move(bytes_); }

private:
    std::string bytes_;
};

struct VorbisEncoder {
    vorbis_info info{};
    vorbis_comment comment{};
    vorbis_dsp_state dsp{};
    vorbis_block block{};
    ogg_stream_state stream{};
    bool analysisReady = false;
    bool streamReady = false;
    std::string output;

    VorbisEncoder() {
        vorbis_info_init(&info);
        vorbis_comment_init(&comment);
    }
    ~VorbisEncoder() {
        if (streamReady) { ogg_stream_clear(&stream); }
        if (analysisReady) {
            vorbis_block_clear(&block);
            vorbis_dsp_clear(&dsp);
        }
        vorbis_comment_clear(&comment);
        vorbis_info_clear(&info);
    }
    VorbisEncoder(const VorbisEncoder&) = delete;
    VorbisEncoder& operator=(const VorbisEncoder&) = delete;

    void writePage(const ogg_page& page) {
        output.append(reinterpret_cast<const char*>(page.header), static_cast<std::size_t>(page.header_len));
        output.append(reinterpret_cast<const char*>(page.body), static_cast<std::size_t>(page.body_len));
    }

    void drain() {
        ogg_packet packet;
        ogg_page page;
        while (vorbis_analysis_blockout(&dsp, &block) == 1) {
            vorbis_analysis(&block, nullptr);
            vorbis_bitrate_addblock(&block);
            while (vorbis_bitrate_flushpacket(&dsp, &packet) != 0) {
                ogg_stream_packetin(&stream, &packet);
                while (ogg_stream_pageout(&stream, &page) != 0) {
                    writePage(page);
                }
            }
        }
    }
};

}  // namespace

core::PcmBuffer decodePcm(const std::filesystem::path& path, DecodeFormat format) {
    DecoderHandle handle(path, requestedFormat(format));
    ma_decoder& decoder = handle.get();
    if (decoder.outputSampleRate == 0 || decoder.outputChannels == 0) {
        throw AudioDecodeError("音声ファイルの形式を判別できません: " + pathToUtf8(path));
    }
    core::PcmBuffer buffer;
    buffer.sampleRate = static_cast<int>(decoder.outputSampleRate);
    buffer.channels = static_cast<int>(decoder.outputChannels);
    buffer.format = sampleFormatOf(decoder.outputFormat, path);

    std::vector<std::uint8_t> chunk(static_cast<std::size_t>(kChunkFrames) * buffer.frameBytes());
    for (;;) {
        ma_uint64 framesRead = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead);
        buffer.bytes.insert(buffer.bytes.end(), chunk.begin(),
                            chunk.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(framesRead) * buffer.frameBytes()));
        if (result == MA_AT_END || framesRead == 0) {
            break;
        }
        if (result != MA_SUCCESS) {
            throw AudioDecodeError("音声ファイルの読み込みに失敗しました: " + pathToUtf8(path) + " (" +
                                   ma_result_description(result) + ")");
        }
    }
    return buffer;
}

std::string encodeWav(const core::PcmBuffer& buffer) {
    const bool isFloat = buffer.format == core::SampleFormat::F32;
    const auto dataBytes = static_cast<std::uint32_t>(buffer.bytes.size());
    const auto blockAlign = static_cast<std::uint16_t>(buffer.frameBytes());
    // Why not 浮動小数点でも 16 バイトの fmt にする: WAVE の仕様では PCM 以外の形式は cbSize を含む 18 バイトの fmt が必要なため
    const std::uint32_t fmtBytes = isFloat ? 18 : 16;
    const std::uint32_t padding = dataBytes % 2;

    ByteWriter writer;
    writer.text("RIFF");
    writer.u32(4 + (8 + fmtBytes) + (8 + dataBytes + padding));
    writer.text("WAVE");
    writer.text("fmt ");
    writer.u32(fmtBytes);
    writer.u16(isFloat ? kWaveFloat : kWavePcm);
    writer.u16(static_cast<std::uint16_t>(buffer.channels));
    writer.u32(static_cast<std::uint32_t>(buffer.sampleRate));
    writer.u32(static_cast<std::uint32_t>(buffer.sampleRate) * blockAlign);
    writer.u16(blockAlign);
    writer.u16(static_cast<std::uint16_t>(core::bitsPerSample(buffer.format)));
    if (isFloat) {
        writer.u16(0);
    }
    writer.text("data");
    writer.u32(dataBytes);
    writer.append(buffer.bytes.data(), buffer.bytes.size());
    if (padding != 0) {
        const char zero = 0;
        writer.append(&zero, 1);
    }
    return writer.take();
}

std::string encodeOgg(const core::PcmBuffer& buffer, int quality) {
    VorbisEncoder encoder;
    const float vorbisQuality = static_cast<float>(std::clamp(quality, 0, 10)) / 10.0f;
    if (vorbis_encode_init_vbr(&encoder.info, buffer.channels, buffer.sampleRate, vorbisQuality) != 0) {
        throw AudioEncodeError("OGG に変換できない形式です（" + std::to_string(buffer.channels) + " ch, " +
                               std::to_string(buffer.sampleRate) + " Hz）");
    }
    vorbis_comment_add_tag(&encoder.comment, "ENCODER", "bmsonto62bms");
    vorbis_analysis_init(&encoder.dsp, &encoder.info);
    vorbis_block_init(&encoder.dsp, &encoder.block);
    encoder.analysisReady = true;
    // Why not 乱数の通し番号にする: 同じ入力から同じファイルができるほうが、書き出し結果を比べやすいため
    ogg_stream_init(&encoder.stream, kOggSerialNumber);
    encoder.streamReady = true;

    ogg_packet header;
    ogg_packet comment;
    ogg_packet codebook;
    vorbis_analysis_headerout(&encoder.dsp, &encoder.comment, &header, &comment, &codebook);
    ogg_stream_packetin(&encoder.stream, &header);
    ogg_stream_packetin(&encoder.stream, &comment);
    ogg_stream_packetin(&encoder.stream, &codebook);
    ogg_page page;
    while (ogg_stream_flush(&encoder.stream, &page) != 0) {
        encoder.writePage(page);
    }

    const std::vector<float> samples = core::toFloat(buffer);
    const auto channels = static_cast<std::size_t>(buffer.channels);
    const std::size_t frames = channels == 0 ? 0 : samples.size() / channels;
    for (std::size_t first = 0; first < frames; first += kOggBlockFrames) {
        const std::size_t count = std::min<std::size_t>(kOggBlockFrames, frames - first);
        float** planes = vorbis_analysis_buffer(&encoder.dsp, static_cast<int>(count));
        for (std::size_t frame = 0; frame < count; ++frame) {
            for (std::size_t channel = 0; channel < channels; ++channel) {
                planes[channel][frame] = samples[(first + frame) * channels + channel];
            }
        }
        vorbis_analysis_wrote(&encoder.dsp, static_cast<int>(count));
        encoder.drain();
    }
    vorbis_analysis_wrote(&encoder.dsp, 0);
    encoder.drain();
    while (ogg_stream_flush(&encoder.stream, &page) != 0) {
        encoder.writePage(page);
    }
    return std::move(encoder.output);
}

void writeBinaryFile(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("ファイルを書き込めません: " + pathToUtf8(path));
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("ファイルの書き込みに失敗しました: " + pathToUtf8(path));
    }
}

}  // namespace b2b::infra
