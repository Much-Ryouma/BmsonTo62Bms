#include <gtest/gtest.h>

#include <filesystem>

#include "WavWriter.hpp"
#include "b2b/infra/audio/AudioProbe.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

using namespace b2b;

namespace {

std::filesystem::path scratchDirectory(const char* name) {
    auto directory = std::filesystem::temp_directory_path() / "b2b_tests" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

}  // namespace

TEST(AudioProbe, AUD001_ReportsRateAndExactLength) {
    const auto path = scratchDirectory("aud001") / "a.wav";
    test::writeSilentWav(path, 44100, 2, 1234);
    EXPECT_EQ(infra::probeAudioFile(path), (core::AudioInfo{44100, 1234}));
}

TEST(AudioProbe, AUD002_MissingFileIsAnError) {
    const auto path = scratchDirectory("aud002") / "missing.wav";
    try {
        static_cast<void>(infra::probeAudioFile(path));
        FAIL() << "expected an error";
    } catch (const infra::AudioDecodeError& error) {
        EXPECT_NE(std::string(error.what()).find("missing.wav"), std::string::npos);
    }
}

TEST(AudioProbe, AUD003_JapaneseFileNameOpens) {
    const auto path = scratchDirectory("aud003") / infra::pathFromUtf8("ドラム.wav");
    test::writeSilentWav(path, 48000, 1, 500);
    EXPECT_EQ(infra::probeAudioFile(path), (core::AudioInfo{48000, 500}));
}
