#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "b2b/core/audio/Mix.hpp"
#include "b2b/core/audio/Pcm.hpp"
#include "b2b/core/bmson/Bmson.hpp"
#include "b2b/core/slicing/SlicePlan.hpp"

namespace b2b::app {

struct LoadedChart {
    core::bmson::Chart chart;
    std::vector<std::string> warnings;
};

class ChartSource {
public:
    virtual ~ChartSource() = default;
    [[nodiscard]] virtual LoadedChart load(const std::filesystem::path& path) const = 0;
};

class AudioSource {
public:
    virtual ~AudioSource() = default;
    [[nodiscard]] virtual core::AudioInfo probe(const std::filesystem::path& path) const = 0;
};

class FileSink {
public:
    virtual ~FileSink() = default;
    virtual void write(const std::filesystem::path& path, const std::string& bytes) const = 0;
};

enum class SampleRequest { Native, Pcm16, Float32 };

class AudioCodec {
public:
    virtual ~AudioCodec() = default;
    [[nodiscard]] virtual core::PcmBuffer decode(const std::filesystem::path& path, SampleRequest request) const = 0;
    virtual void writeWav(const std::filesystem::path& path, const core::PcmBuffer& buffer) const = 0;
    virtual void writeOgg(const std::filesystem::path& path, const core::PcmBuffer& buffer, int quality) const = 0;
};

class ProcessLauncher {
public:
    virtual ~ProcessLauncher() = default;
    virtual bool launch(const std::string& program, const std::vector<std::string>& arguments,
                        const std::filesystem::path& workingDirectory) = 0;
};

class CachingAudioSource final : public AudioSource {
public:
    explicit CachingAudioSource(const AudioSource& inner) : inner_(inner) {}

    [[nodiscard]] core::AudioInfo probe(const std::filesystem::path& path) const override;
    void clear();

private:
    const AudioSource& inner_;
    mutable std::mutex mutex_;
    mutable std::map<std::filesystem::path, core::AudioInfo> cache_;
};

class LocalChartSource final : public ChartSource {
public:
    [[nodiscard]] LoadedChart load(const std::filesystem::path& path) const override;
};

class LocalAudioSource final : public AudioSource {
public:
    [[nodiscard]] core::AudioInfo probe(const std::filesystem::path& path) const override;
};

class LocalFileSink final : public FileSink {
public:
    void write(const std::filesystem::path& path, const std::string& bytes) const override;
};

class LocalAudioCodec final : public AudioCodec {
public:
    [[nodiscard]] core::PcmBuffer decode(const std::filesystem::path& path, SampleRequest request) const override;
    void writeWav(const std::filesystem::path& path, const core::PcmBuffer& buffer) const override;
    void writeOgg(const std::filesystem::path& path, const core::PcmBuffer& buffer, int quality) const override;
};

}  // namespace b2b::app
