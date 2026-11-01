#include "b2b/app/Ports.hpp"

#include "b2b/infra/audio/AudioFiles.hpp"
#include "b2b/infra/audio/AudioProbe.hpp"
#include "b2b/infra/bmson/BmsonJson.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::app {

namespace {

void ensureParentDirectory(const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

}  // namespace

core::AudioInfo CachingAudioSource::probe(const std::filesystem::path& path) const {
    {
        const std::lock_guard lock(mutex_);
        const auto found = cache_.find(path);
        if (found != cache_.end()) {
            return found->second;
        }
    }
    const core::AudioInfo info = inner_.probe(path);
    const std::lock_guard lock(mutex_);
    cache_[path] = info;
    return info;
}

void CachingAudioSource::clear() {
    const std::lock_guard lock(mutex_);
    cache_.clear();
}

LoadedChart LocalChartSource::load(const std::filesystem::path& path) const {
    auto parsed = infra::parseBmson(infra::readTextFile(path));
    return LoadedChart{std::move(parsed.chart), std::move(parsed.warnings)};
}

core::AudioInfo LocalAudioSource::probe(const std::filesystem::path& path) const {
    return infra::probeAudioFile(path);
}

void LocalFileSink::write(const std::filesystem::path& path, const std::string& bytes) const {
    ensureParentDirectory(path);
    infra::writeTextFileAtomically(path, bytes);
}

core::PcmBuffer LocalAudioCodec::decode(const std::filesystem::path& path, SampleRequest request) const {
    switch (request) {
        case SampleRequest::Native: return infra::decodePcm(path, infra::DecodeFormat::Native);
        case SampleRequest::Pcm16: return infra::decodePcm(path, infra::DecodeFormat::S16);
        case SampleRequest::Float32: return infra::decodePcm(path, infra::DecodeFormat::F32);
    }
    return infra::decodePcm(path, infra::DecodeFormat::Native);
}

void LocalAudioCodec::writeWav(const std::filesystem::path& path, const core::PcmBuffer& buffer) const {
    ensureParentDirectory(path);
    infra::writeBinaryFile(path, infra::encodeWav(buffer));
}

void LocalAudioCodec::writeOgg(const std::filesystem::path& path, const core::PcmBuffer& buffer, int quality) const {
    ensureParentDirectory(path);
    infra::writeBinaryFile(path, infra::encodeOgg(buffer, quality));
}

}  // namespace b2b::app
