#include "Commands.hpp"

#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "b2b/app/ConvertService.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::cli {

namespace {

struct CommonArguments {
    std::string chartFile;
    std::string encoding = "sjis";
    std::optional<int> difficulty;
    std::string format = "wav";
    int quality = app::kDefaultOggQuality;
    app::NamingOptions naming;
    std::string edge = "cut";
    double fadeOutMs = core::FadeSettings{}.fadeOutMs;
    std::optional<double> fadeInMs;
};

struct Services {
    app::LocalChartSource charts;
    app::LocalAudioSource audio;
    app::LocalAudioCodec codec;
    app::LocalFileSink files;
    app::ConvertService service{charts, audio, codec, files};
};

const std::map<std::string, infra::TextEncoding> kEncodings = {{"sjis", infra::TextEncoding::ShiftJis},
                                                                {"utf8", infra::TextEncoding::Utf8}};

void addCommonOptions(CLI::App& command, CommonArguments& arguments) {
    command.add_option("chart", arguments.chartFile, "bmson file")->required();
    command.add_option("--encoding", arguments.encoding, "Text encoding of the BMS file (sjis or utf8)")
        ->check(CLI::IsMember({"sjis", "utf8"}));
    command.add_option("--difficulty", arguments.difficulty, "#DIFFICULTY (1 BEGINNER .. 5 INSANE); omitted if not given")
        ->check(CLI::Range(1, 5));
    command.add_option("--format", arguments.format, "Format of the sliced sounds (wav or ogg)")
        ->check(CLI::IsMember({"wav", "ogg"}));
    command.add_option("--quality", arguments.quality, "OGG quality (0 .. 10)")->check(CLI::Range(0, 10));
    command.add_option("--folder", arguments.naming.folderPattern, "Pattern of the sound folder (only {song})");
    command.add_option("--key-name", arguments.naming.keyPattern, "Pattern of key sound file names");
    command.add_option("--bgm-name", arguments.naming.bgmPattern, "Pattern of BGM sound file names");
    command.add_option("--edge", arguments.edge, "Edges of the slices: cut (sample exact) or fade")
        ->check(CLI::IsMember({"cut", "fade"}));
    command.add_option("--fade-out", arguments.fadeOutMs, "Fade-out length in ms when --edge fade (0 .. 50)");
    command.add_option("--fade-in", arguments.fadeInMs, "Fade-in length in ms when --edge fade (0 .. 50); off if not given");
}

app::ConvertOptions optionsOf(const CommonArguments& arguments) {
    app::ConvertOptions options;
    options.encoding = kEncodings.at(arguments.encoding);
    options.difficulty = arguments.difficulty;
    options.soundFormat = arguments.format == "ogg" ? app::SoundFormat::Ogg : app::SoundFormat::Wav;
    options.oggQuality = arguments.quality;
    options.naming = arguments.naming;
    options.fade.mode = arguments.edge == "fade" ? core::EdgeMode::Fade : core::EdgeMode::Cut;
    options.fade.fadeOutMs = arguments.fadeOutMs;
    options.fade.fadeIn = arguments.fadeInMs.has_value();
    if (arguments.fadeInMs) {
        options.fade.fadeInMs = *arguments.fadeInMs;
    }
    return options;
}

const char* labelOf(app::Severity severity) {
    switch (severity) {
        case app::Severity::Error: return "error";
        case app::Severity::Warning: return "warning";
        case app::Severity::Info: return "info";
    }
    return "info";
}

void printMessages(const std::vector<app::Message>& messages) {
    for (const auto& message : messages) {
        std::cerr << labelOf(message.severity) << ": " << message.text << "\n";
    }
}

std::string idOf(const app::Conversion& conversion, std::size_t slice) {
    const core::IdBase base = conversion.document ? conversion.document->base : core::IdBase::Base62;
    return core::formatObjectId(static_cast<int>(slice) + 1, base);
}

int runSlices(const CommonArguments& arguments, bool json) {
    const Services services;
    const app::Conversion conversion =
        services.service.prepare(infra::pathFromUtf8(arguments.chartFile), optionsOf(arguments));
    printMessages(conversion.messages);
    if (conversion.hasErrors()) {
        return toInt(ExitCode::ConversionError);
    }
    const auto& slices = conversion.plan.slices;
    if (json) {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (std::size_t index = 0; index < slices.size(); ++index) {
            const auto& slice = slices[index];
            array.push_back({{"id", idOf(conversion, index)},
                             {"kind", slice.kind == core::SliceKind::Key ? "key" : "bgm"},
                             {"file", conversion.soundFiles[index]},
                             {"source", slice.source},
                             {"startFrame", slice.startFrame},
                             {"endFrame", slice.endFrame},
                             {"firstPulse", slice.firstPulse},
                             {"firstLane", slice.firstLane}});
        }
        std::cout << array.dump(2) << "\n";
        return toInt(ExitCode::Success);
    }
    std::printf("%-3s %-4s %12s %12s  %-24s %s\n", "ID", "KIND", "START", "END", "SOURCE", "FILE");
    for (std::size_t index = 0; index < slices.size(); ++index) {
        const auto& slice = slices[index];
        std::printf("%-3s %-4s %12lld %12lld  %-24s %s\n", idOf(conversion, index).c_str(),
                    slice.kind == core::SliceKind::Key ? "key" : "bgm", static_cast<long long>(slice.startFrame),
                    static_cast<long long>(slice.endFrame), slice.source.c_str(), conversion.soundFiles[index].c_str());
    }
    std::printf("slices: %zu (key %d, bgm %d)\n", slices.size(), conversion.plan.countOf(core::SliceKind::Key),
                conversion.plan.countOf(core::SliceKind::Bgm));
    return toInt(ExitCode::Success);
}

int runConvert(const CommonArguments& arguments, const std::string& outputFile, bool bmsOnly) {
    const Services services;
    const auto chartPath = infra::pathFromUtf8(arguments.chartFile);
    const app::ConvertOptions options = optionsOf(arguments);

    const app::Conversion conversion = services.service.prepare(chartPath, options);
    printMessages(conversion.messages);
    if (conversion.hasErrors()) {
        return toInt(ExitCode::ConversionError);
    }
    const auto output =
        outputFile.empty() ? app::ConvertService::defaultOutputPath(chartPath) : infra::pathFromUtf8(outputFile);

    if (!bmsOnly) {
        const app::ExportOutcome exported = services.service.exportSounds(conversion, output, options);
        printMessages(exported.messages);
        if (exported.hasErrors()) {
            return toInt(ExitCode::ConversionError);
        }
    }
    const app::WriteOutcome outcome = services.service.writeBms(conversion, output, options.encoding);
    printMessages(outcome.messages);
    if (!outcome.written) {
        return toInt(ExitCode::ConversionError);
    }
    const auto& usage = conversion.document->usage;
    std::cout << "wrote: " << infra::pathToUtf8(outcome.path) << "\n"
              << "base:  " << (conversion.document->base == core::IdBase::Base62 ? "62" : "36") << "\n"
              << "ids:   WAV " << usage.wav << ", BMP " << usage.bmp << ", BPM " << usage.bpm << ", STOP " << usage.stop
              << "\n";
    return toInt(ExitCode::Success);
}

}  // namespace

void registerSlicesCommand(CLI::App& app, CommandResult& result) {
    auto arguments = std::make_shared<CommonArguments>();
    auto json = std::make_shared<bool>(false);
    auto* command = app.add_subcommand("slices", "List the sound slices the chart needs, with their BMS ids and file names");
    addCommonOptions(*command, *arguments);
    command->add_flag("--json", *json, "Print as JSON");
    command->callback([&result, arguments, json] { result.exitCode = runSlices(*arguments, *json); });
}

void registerConvertCommand(CLI::App& app, CommandResult& result) {
    auto arguments = std::make_shared<CommonArguments>();
    auto output = std::make_shared<std::string>();
    auto bmsOnly = std::make_shared<bool>(false);
    auto* command = app.add_subcommand("convert", "Convert a bmson file into a BMS file and sliced sound files");
    addCommonOptions(*command, *arguments);
    command->add_option("-o,--output", *output, "Output BMS file (default: <chart>.bme next to the bmson)");
    command->add_flag("--bms-only", *bmsOnly, "Write only the BMS file, not the sound files");
    command->callback([&result, arguments, output, bmsOnly] {
        result.exitCode = runConvert(*arguments, *output, *bmsOnly);
    });
}

}  // namespace b2b::cli
