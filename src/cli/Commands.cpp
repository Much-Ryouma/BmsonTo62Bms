#include "Commands.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "b2b/app/ConvertService.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::cli {

namespace {

struct CommonArguments {
    std::vector<std::string> chartFiles;
    std::string base;
    bool allowExtra = false;
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
    command.add_option("charts", arguments.chartFiles, "bmson files (several difficulties of one song share the sounds)")->required();
    command.add_option("--base", arguments.base, "bmson file whose slices the others follow (default: the highest level)");
    command.add_flag("--allow-extra", arguments.allowExtra, "Also cut slices that the base chart does not have");
    command.add_option("--encoding", arguments.encoding, "Text encoding of the BMS file (sjis or utf8)")
        ->check(CLI::IsMember({"sjis", "utf8"}));
    command.add_option("--difficulty", arguments.difficulty,
                       "#DIFFICULTY (1 BEGINNER .. 5 INSANE) of a single chart; omitted if not given. "
                       "With several charts it is guessed from chart_name")
        ->check(CLI::Range(1, 5));
    command.add_option("--format", arguments.format, "Format of the sliced sounds (wav or ogg)")
        ->check(CLI::IsMember({"wav", "ogg"}));
    command.add_option("--quality", arguments.quality, "OGG quality (0 .. 10)")->check(CLI::Range(0, 10));
    command.add_option("--folder", arguments.naming.folderPattern, "Pattern of the folder the sound files are written to (only {song}); #WAV keeps file names only");
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
    options.allowExtraSlices = arguments.allowExtra;
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

struct PreparedSet {
    app::ConversionSet set;
    bool usageError = false;
};

PreparedSet prepareSet(const Services& services, const CommonArguments& arguments) {
    PreparedSet prepared;
    std::vector<app::ChartRequest> requests;
    std::vector<app::ChartSummary> summaries;
    const bool single = arguments.chartFiles.size() == 1;
    for (const auto& file : arguments.chartFiles) {
        const auto path = infra::pathFromUtf8(file);
        summaries.push_back(services.service.summarize(path));
        // Why not 1 譜面のときも chart_name から推し量る: 1 譜面のときは今までどおり --difficulty だけで決め、
        // 指定しなければ書かない（勝手に書くと、プレイヤーの難易度の表示が変わるため）
        requests.push_back({path, single ? arguments.difficulty : app::guessDifficulty(summaries.back().chartName)});
    }
    if (!single && arguments.difficulty) {
        std::cerr << "error: --difficulty can be used only with a single chart\n";
        prepared.usageError = true;
        return prepared;
    }
    std::size_t baseIndex = app::defaultBaseIndex(summaries);
    if (!arguments.base.empty()) {
        const auto wanted = infra::pathFromUtf8(arguments.base);
        const auto found = std::find_if(requests.begin(), requests.end(), [&](const app::ChartRequest& request) {
            return request.path == wanted || request.path.filename() == wanted.filename();
        });
        if (found == requests.end()) {
            std::cerr << "error: --base must be one of the given bmson files: " << arguments.base << "\n";
            prepared.usageError = true;
            return prepared;
        }
        baseIndex = static_cast<std::size_t>(found - requests.begin());
    }
    prepared.set = services.service.prepareSet(requests, baseIndex, optionsOf(arguments));
    printMessages(prepared.set.messages);
    for (const auto& conversion : prepared.set.charts) {
        if (!single && !conversion.messages.empty()) {
            std::cerr << "[" << infra::pathToUtf8(conversion.chartPath.filename()) << "]\n";
        }
        printMessages(conversion.messages);
    }
    return prepared;
}

std::string chartsUsing(const app::ConversionSet& set, std::size_t slice) {
    std::string text;
    for (const std::size_t chart : set.sliceUsers.at(slice)) {
        text += (text.empty() ? "" : ",") + std::to_string(chart + 1);
    }
    return text;
}

int runSlices(const CommonArguments& arguments, bool json) {
    const Services services;
    const PreparedSet prepared = prepareSet(services, arguments);
    if (prepared.usageError) {
        return toInt(ExitCode::UsageError);
    }
    const app::ConversionSet& set = prepared.set;
    if (set.hasErrors()) {
        return toInt(ExitCode::ConversionError);
    }
    const app::Conversion& conversion = set.base();
    const auto& slices = conversion.plan.slices;
    const bool several = set.charts.size() > 1;
    if (json) {
        nlohmann::ordered_json array = nlohmann::ordered_json::array();
        for (std::size_t index = 0; index < slices.size(); ++index) {
            const auto& slice = slices[index];
            nlohmann::ordered_json item = {{"id", idOf(conversion, index)},
                                           {"kind", slice.kind == core::SliceKind::Key ? "key" : "bgm"},
                                           {"file", app::soundFileOnDisk(conversion, index)},
                                           {"source", slice.source},
                                           {"startFrame", slice.startFrame},
                                           {"endFrame", slice.endFrame},
                                           {"firstPulse", slice.firstPulse},
                                           {"firstLane", slice.firstLane}};
            if (several) {
                nlohmann::ordered_json charts = nlohmann::ordered_json::array();
                for (const std::size_t chart : set.sliceUsers[index]) {
                    charts.push_back(infra::pathToUtf8(set.charts[chart].chartPath.filename()));
                }
                item["charts"] = charts;
            }
            array.push_back(item);
        }
        std::cout << array.dump(2) << "\n";
        return toInt(ExitCode::Success);
    }
    if (several) {
        for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
            std::printf("chart %zu: %s%s\n", chart + 1, infra::pathToUtf8(set.charts[chart].chartPath.filename()).c_str(),
                        chart == set.baseIndex ? " (base)" : "");
        }
        std::printf("%-3s %-4s %12s %12s  %-8s %-24s %s\n", "ID", "KIND", "START", "END", "CHARTS", "SOURCE", "FILE");
    } else {
        std::printf("%-3s %-4s %12s %12s  %-24s %s\n", "ID", "KIND", "START", "END", "SOURCE", "FILE");
    }
    for (std::size_t index = 0; index < slices.size(); ++index) {
        const auto& slice = slices[index];
        const std::string id = idOf(conversion, index);
        const char* kind = slice.kind == core::SliceKind::Key ? "key" : "bgm";
        if (several) {
            std::printf("%-3s %-4s %12lld %12lld  %-8s %-24s %s\n", id.c_str(), kind, static_cast<long long>(slice.startFrame),
                        static_cast<long long>(slice.endFrame), chartsUsing(set, index).c_str(), slice.source.c_str(),
                        app::soundFileOnDisk(conversion, index).c_str());
        } else {
            std::printf("%-3s %-4s %12lld %12lld  %-24s %s\n", id.c_str(), kind, static_cast<long long>(slice.startFrame),
                        static_cast<long long>(slice.endFrame), slice.source.c_str(), app::soundFileOnDisk(conversion, index).c_str());
        }
    }
    std::printf("slices: %zu (key %d, bgm %d)\n", slices.size(), conversion.plan.countOf(core::SliceKind::Key),
                conversion.plan.countOf(core::SliceKind::Bgm));
    return toInt(ExitCode::Success);
}

int runConvert(const CommonArguments& arguments, const std::string& outputFile, bool bmsOnly) {
    if (!outputFile.empty() && arguments.chartFiles.size() > 1) {
        std::cerr << "error: -o can be used only with a single chart\n";
        return toInt(ExitCode::UsageError);
    }
    const Services services;
    const PreparedSet prepared = prepareSet(services, arguments);
    if (prepared.usageError) {
        return toInt(ExitCode::UsageError);
    }
    const app::ConversionSet& set = prepared.set;
    if (set.hasErrors()) {
        return toInt(ExitCode::ConversionError);
    }
    std::vector<std::filesystem::path> outputs;
    for (const auto& conversion : set.charts) {
        outputs.push_back(outputFile.empty()
                              ? app::ConvertService::defaultOutputPath(conversion.chartPath, conversion.chart.info.modeHint)
                              : infra::pathFromUtf8(outputFile));
    }

    const app::ConvertOptions options = optionsOf(arguments);
    if (!bmsOnly) {
        const app::ExportOutcome exported = services.service.exportSounds(set.base(), outputs[set.baseIndex], options);
        printMessages(exported.messages);
        if (exported.hasErrors()) {
            return toInt(ExitCode::ConversionError);
        }
    }
    int exitCode = toInt(ExitCode::Success);
    for (std::size_t index = 0; index < set.charts.size(); ++index) {
        const app::Conversion& conversion = set.charts[index];
        const app::WriteOutcome outcome = services.service.writeBms(conversion, outputs[index], options.encoding);
        printMessages(outcome.messages);
        if (!outcome.written) {
            exitCode = toInt(ExitCode::ConversionError);
            continue;
        }
        const auto& usage = conversion.document->usage;
        std::cout << "wrote: " << infra::pathToUtf8(outcome.path) << (set.charts.size() > 1 && index == set.baseIndex ? " (base)" : "")
                  << "\n"
                  << "mode:  " << core::findKeyMode(conversion.chart.info.modeHint)->name << "\n"
                  << "base:  " << (conversion.document->base == core::IdBase::Base62 ? "62" : "36") << "\n"
                  << "ids:   WAV " << usage.wavDefined << " (largest " << usage.wav << "), BMP " << usage.bmp << ", BPM "
                  << usage.bpm << ", STOP " << usage.stop << "\n";
    }
    return exitCode;
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
    auto* command = app.add_subcommand("convert", "Convert bmson files into BMS files and sliced sound files shared by them");
    addCommonOptions(*command, *arguments);
    command->add_option("-o,--output", *output,
                        "Output BMS file of a single chart (default: <chart>.bme, or .pms for popn-9k, next to each bmson)");
    command->add_flag("--bms-only", *bmsOnly, "Write only the BMS file, not the sound files");
    command->callback([&result, arguments, output, bmsOnly] {
        result.exitCode = runConvert(*arguments, *output, *bmsOnly);
    });
}

}  // namespace b2b::cli
