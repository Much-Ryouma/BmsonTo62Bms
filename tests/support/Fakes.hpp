#pragma once

#include <map>
#include <vector>
#include <optional>
#include <stdexcept>
#include <string>

#include "b2b/app/Ports.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::test {

class FakeChartSource final : public app::ChartSource {
public:
    std::optional<core::bmson::Chart> chart;

    [[nodiscard]] app::LoadedChart load(const std::filesystem::path&) const override {
        if (!chart) {
            throw std::runtime_error("broken");
        }
        return app::LoadedChart{*chart, {}};
    }
};

class FakeAudioSource final : public app::AudioSource {
public:
    std::map<std::string, core::AudioInfo> files;
    mutable std::map<std::string, int> probes;

    [[nodiscard]] core::AudioInfo probe(const std::filesystem::path& path) const override {
        const std::string name = infra::pathToUtf8(path.filename());
        ++probes[name];
        const auto found = files.find(name);
        if (found == files.end()) {
            throw std::runtime_error("音声ファイルを開けません: " + name);
        }
        return found->second;
    }
};

class FakeFileSink final : public app::FileSink {
public:
    mutable std::map<std::string, std::string> written;

    void write(const std::filesystem::path& path, const std::string& bytes) const override {
        written[infra::pathToUtf8(path)] = bytes;
    }
};

class FakeAudioCodec final : public app::AudioCodec {
public:
    struct Written {
        std::string kind;
        core::PcmBuffer buffer;
        int quality = 0;
    };

    std::map<std::string, core::PcmBuffer> sources;
    mutable std::map<std::string, std::vector<app::SampleRequest>> decodes;
    mutable std::map<std::string, Written> written;

    [[nodiscard]] core::PcmBuffer decode(const std::filesystem::path& path, app::SampleRequest request) const override {
        const std::string name = infra::pathToUtf8(path.filename());
        decodes[name].push_back(request);
        const auto found = sources.find(name);
        if (found == sources.end()) {
            throw std::runtime_error("音声ファイルを開けません: " + name);
        }
        return found->second;
    }

    void writeWav(const std::filesystem::path& path, const core::PcmBuffer& buffer) const override {
        written[infra::pathToUtf8(path)] = Written{"wav", buffer, 0};
    }

    void writeOgg(const std::filesystem::path& path, const core::PcmBuffer& buffer, int quality) const override {
        written[infra::pathToUtf8(path)] = Written{"ogg", buffer, quality};
    }
};

class FakeLauncher final : public app::ProcessLauncher {
public:
    std::string program;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    int launches = 0;
    bool succeed = true;

    bool launch(const std::string& launchedProgram, const std::vector<std::string>& launchedArguments,
                const std::filesystem::path& launchedDirectory) override {
        ++launches;
        program = launchedProgram;
        arguments = launchedArguments;
        workingDirectory = launchedDirectory;
        return succeed;
    }
};

}  // namespace b2b::test
