#include "b2b/app/ConvertService.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::app {

namespace {

constexpr std::string_view kLineEnd = "\r\n";
constexpr int kMinOggQuality = 0;
constexpr int kMaxOggQuality = 10;

std::string joined(const std::vector<std::string>& items) {
    std::string text;
    for (const auto& item : items) {
        if (!text.empty()) {
            text += " ";
        }
        text += item;
    }
    return text;
}

std::string lowerExtension(const std::string& source) {
    std::string extension = infra::pathToUtf8(infra::pathFromUtf8(source).extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return extension;
}

void reportPlanStats(const core::SlicePlanStats& stats, std::vector<Message>& messages) {
    if (stats.canceledBgmNotes > 0) {
        messages.push_back({Severity::Info, "同じ位置に鍵盤のノーツがある BGM のノーツ " +
                                                std::to_string(stats.canceledBgmNotes) + " 個は、bmson と同じく鳴らさないため書き出しません"});
    }
    if (stats.ignoredLongNoteEndSounds > 0) {
        messages.push_back({Severity::Warning, "ロングノーツの終点の音 (up) " + std::to_string(stats.ignoredLongNoteEndSounds) +
                                                   " 個は BMS (#LNTYPE 1) で表せないため書き出しません"});
    }
    if (stats.notesPastAudioEnd > 0) {
        messages.push_back({Severity::Warning, "音声ファイルの長さより後を鳴らすノーツが " +
                                                   std::to_string(stats.notesPastAudioEnd) + " 個あります（無音になります）"});
    }
}

void reportUnsupportedInfo(const core::bmson::Info& info, std::vector<Message>& messages) {
    if (!info.titleImage.empty()) {
        messages.push_back({Severity::Warning, "title_image は BMS に対応する命令がないため書き出しません"});
    }
    if (!info.chartName.empty()) {
        messages.push_back({Severity::Warning, "chart_name は BMS に対応する命令がないため書き出しません"});
    }
}

void checkOptions(const ConvertOptions& options, std::vector<Message>& messages) {
    if (const auto error = core::fadeSettingsError(options.fade)) {
        messages.push_back({Severity::Error, *error});
    }
    if (options.oggQuality < kMinOggQuality || options.oggQuality > kMaxOggQuality) {
        messages.push_back({Severity::Error, "OGG の品質は 0〜10 で指定してください"});
    }
    const NamingResult naming = checkNamingPatterns(options.naming);
    messages.insert(messages.end(), naming.messages.begin(), naming.messages.end());
}

void appendWriterMessages(core::BmsWriteResult& written, std::vector<Message>& messages) {
    for (auto& warning : written.warnings) {
        messages.push_back({Severity::Warning, std::move(warning)});
    }
    for (auto& error : written.errors) {
        messages.push_back({Severity::Error, std::move(error)});
    }
}

}  // namespace

std::string extensionOf(SoundFormat format) {
    return format == SoundFormat::Ogg ? "ogg" : "wav";
}

bool Conversion::hasErrors() const {
    return containsError(messages) || !document;
}

bool RenderedBms::hasErrors() const {
    return containsError(messages);
}

bool ExportOutcome::hasErrors() const {
    return containsError(messages);
}

RenderedBms renderBms(const core::BmsDocument& document, infra::TextEncoding encoding) {
    RenderedBms rendered;
    for (const auto& line : document.lines) {
        const infra::EncodedText encoded = infra::encodeText(line.text, encoding);
        if (!encoded.unrepresentable.empty()) {
            const std::string characters = joined(encoded.unrepresentable);
            if (line.kind == core::BmsLineKind::Path) {
                rendered.messages.push_back({Severity::Error, "ファイル名に " + std::string(infra::encodingName(encoding)) +
                                                                  " で表せない文字があります（" + characters + "）: " + line.text});
            } else {
                rendered.messages.push_back({Severity::Warning, std::string(infra::encodingName(encoding)) +
                                                                    " で表せない文字を ? に置き換えました（" + characters + "）: " + line.text});
            }
        }
        rendered.bytes += encoded.bytes;
        rendered.bytes += kLineEnd;
    }
    return rendered;
}

SampleRequest sampleRequestFor(const std::string& source, SoundFormat format) {
    if (format == SoundFormat::Ogg) {
        return SampleRequest::Float32;
    }
    const std::string extension = lowerExtension(source);
    // Why not どの形式もそのまま書く: OGG や MP3 はビット深度を持たないため、元と同じにできない。広く読める 16 bit にそろえる
    return extension == ".wav" || extension == ".flac" ? SampleRequest::Native : SampleRequest::Pcm16;
}

ConvertService::ConvertService(const ChartSource& charts, const AudioSource& audio, const AudioCodec& codec,
                               const FileSink& files)
    : charts_(charts), audio_(audio), codec_(codec), files_(files) {}

Conversion ConvertService::prepare(const std::filesystem::path& chartPath, const ConvertOptions& options) const {
    Conversion conversion;
    conversion.chartPath = chartPath;
    checkOptions(options, conversion.messages);
    if (containsError(conversion.messages)) {
        return conversion;
    }
    try {
        LoadedChart loaded = charts_.load(chartPath);
        conversion.chart = std::move(loaded.chart);
        for (auto& warning : loaded.warnings) {
            conversion.messages.push_back({Severity::Warning, std::move(warning)});
        }
    } catch (const std::exception& error) {
        conversion.messages.push_back({Severity::Error, std::string("bmson を読み込めません: ") + error.what()});
        return conversion;
    }

    const auto& chart = conversion.chart;
    if (!core::isSupportedMode(chart.info.modeHint)) {
        conversion.messages.push_back(
            {Severity::Error, "未対応のキーモードです: " + chart.info.modeHint + "（対応しているのは beat-7k だけです）"});
        return conversion;
    }

    std::map<std::string, core::AudioInfo> probed;
    std::vector<core::AudioInfo> channelAudio;
    const std::filesystem::path directory = chartPath.parent_path();
    for (const auto& channel : chart.soundChannels) {
        if (channel.name.empty()) {
            conversion.messages.push_back({Severity::Error, "音声ファイル名が空の sound_channels があります"});
            channelAudio.emplace_back();
            continue;
        }
        auto found = probed.find(channel.name);
        if (found == probed.end()) {
            try {
                found = probed.emplace(channel.name, audio_.probe(directory / infra::pathFromUtf8(channel.name))).first;
            } catch (const std::exception& error) {
                conversion.messages.push_back({Severity::Error, std::string(error.what())});
                found = probed.emplace(channel.name, core::AudioInfo{}).first;
            }
        }
        channelAudio.push_back(found->second);
    }
    if (containsError(conversion.messages)) {
        return conversion;
    }

    try {
        conversion.plan = core::planSlices(chart, core::bmson::timingOf(chart), channelAudio);
    } catch (const std::invalid_argument& error) {
        conversion.messages.push_back({Severity::Error, std::string("bmson の内容を解釈できません: ") + error.what()});
        return conversion;
    }
    reportPlanStats(conversion.plan.stats, conversion.messages);
    reportUnsupportedInfo(chart.info, conversion.messages);

    core::BmsWriteOptions writeOptions;
    writeOptions.difficulty = options.difficulty;
    // Why not 名前を付けてから 1 回だけ組み立てる: {id} を名前に使うには 36進数か 62進数かが先に決まっている必要があり、
    // それは定義の数で決まる。名前に関係しないので、仮の名前で一度組み立てて進数を確かめる
    const std::vector<std::string> placeholders(conversion.plan.slices.size(), "_");
    core::BmsWriteResult trial = core::writeBms(chart, conversion.plan, placeholders, writeOptions);
    if (!trial.document) {
        appendWriterMessages(trial, conversion.messages);
        return conversion;
    }

    NamingResult naming = nameSounds(chart, conversion.plan, trial.document->base, options.naming,
                                     extensionOf(options.soundFormat), options.encoding);
    conversion.messages.insert(conversion.messages.end(), naming.messages.begin(), naming.messages.end());
    if (naming.hasErrors()) {
        return conversion;
    }
    conversion.soundFiles = std::move(naming.paths);

    core::BmsWriteResult written = core::writeBms(chart, conversion.plan, conversion.soundFiles, writeOptions);
    appendWriterMessages(written, conversion.messages);
    conversion.document = std::move(written.document);
    return conversion;
}

ExportOutcome ConvertService::exportSounds(const Conversion& conversion, const std::filesystem::path& bmsPath,
                                           const ConvertOptions& options, const ExportProgress& progress) const {
    ExportOutcome outcome;
    if (conversion.hasErrors()) {
        outcome.messages.push_back({Severity::Error, "変換に失敗しているため音声ファイルを書き出せません"});
        return outcome;
    }
    std::map<std::string, std::vector<std::size_t>> slicesBySource;
    for (std::size_t index = 0; index < conversion.plan.slices.size(); ++index) {
        slicesBySource[conversion.plan.slices[index].source].push_back(index);
    }

    const std::filesystem::path chartDirectory = conversion.chartPath.parent_path();
    const std::filesystem::path bmsDirectory = bmsPath.parent_path();
    const std::size_t total = conversion.plan.slices.size();
    std::size_t done = 0;
    for (const auto& [source, indices] : slicesBySource) {
        core::PcmBuffer audio;
        try {
            const SampleRequest request = sampleRequestFor(source, options.soundFormat);
            audio = codec_.decode(chartDirectory / infra::pathFromUtf8(source), request);
            // Why not 32 bit のまま書く: 24 bit の FLAC も 32 bit で読み出されるため、下位 8 bit がすべて 0 なら元の 24 bit に戻す
            if (request == SampleRequest::Native && lowerExtension(source) == ".flac" && core::fitsIn24Bits(audio)) {
                audio = core::narrowTo24Bits(audio);
            }
        } catch (const std::exception& error) {
            outcome.messages.push_back({Severity::Error, std::string(error.what())});
            done += indices.size();
            continue;
        }

        for (const std::size_t index : indices) {
            const core::Slice& slice = conversion.plan.slices[index];
            core::PcmBuffer piece = core::extractFrames(audio, slice.startFrame, slice.endFrame);
            core::applyFade(piece, options.fade, slice.startFrame == 0);
            const std::filesystem::path target = bmsDirectory / infra::pathFromUtf8(conversion.soundFiles[index]);
            try {
                if (options.soundFormat == SoundFormat::Ogg) {
                    codec_.writeOgg(target, piece, options.oggQuality);
                } else {
                    codec_.writeWav(target, piece);
                }
                ++outcome.writtenFiles;
            } catch (const std::exception& error) {
                outcome.messages.push_back({Severity::Error, "音声ファイルを書き出せません: " +
                                                                 infra::pathToUtf8(target) + "（" + error.what() + "）"});
            }
            ++done;
            if (progress) {
                progress(done, total);
            }
        }
    }
    outcome.messages.push_back({Severity::Info, std::to_string(outcome.writtenFiles) + " 個の音声ファイルを書き出しました"});
    return outcome;
}

WriteOutcome ConvertService::writeBms(const Conversion& conversion, const std::filesystem::path& output,
                                      infra::TextEncoding encoding) const {
    WriteOutcome outcome;
    outcome.path = output;
    if (!conversion.document) {
        outcome.messages.push_back({Severity::Error, "変換に失敗しているため書き出せません"});
        return outcome;
    }
    RenderedBms rendered = renderBms(*conversion.document, encoding);
    outcome.messages = std::move(rendered.messages);
    if (std::filesystem::absolute(output).parent_path().lexically_normal() !=
        std::filesystem::absolute(conversion.chartPath).parent_path().lexically_normal()) {
        outcome.messages.push_back({Severity::Warning,
                                    "bmson と別のフォルダに書き出します。BGA や画像のファイル名は bmson のフォルダを基準に書いているため、"
                                    "BMS のフォルダに同じファイルがないと表示されません"});
    }
    if (containsError(outcome.messages)) {
        return outcome;
    }
    try {
        files_.write(output, rendered.bytes);
        outcome.written = true;
    } catch (const std::exception& error) {
        outcome.messages.push_back({Severity::Error, std::string("BMS を書き出せません: ") + error.what()});
    }
    return outcome;
}

std::filesystem::path ConvertService::defaultOutputPath(const std::filesystem::path& chartPath) {
    std::filesystem::path output = chartPath;
    output.replace_extension(".bme");
    return output;
}

}  // namespace b2b::app
