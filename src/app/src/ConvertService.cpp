#include "b2b/app/ConvertService.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>

#include "b2b/core/slicing/SliceMerge.hpp"
#include "b2b/core/timing/MeasureLayout.hpp"
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

void reportPlanStats(const core::SlicePlanStats& stats, const core::KeyMode& mode, std::vector<Message>& messages) {
    if (stats.canceledBgmNotes > 0) {
        messages.push_back({Severity::Info, "同じ位置に鍵盤のノーツがある BGM のノーツ " +
                                                std::to_string(stats.canceledBgmNotes) + " 個は、bmson と同じく鳴らさないため書き出しません"});
    }
    if (stats.ignoredLongNoteEndSounds > 0) {
        messages.push_back({Severity::Warning, "ロングノーツの終点の音 (up) " + std::to_string(stats.ignoredLongNoteEndSounds) +
                                                   " 個は BMS (#LNTYPE 1) で表せないため書き出しません"});
    }
    if (stats.lanesOutsideMode > 0) {
        messages.push_back({Severity::Warning, mode.name + " にないレーン（x が 1〜" + std::to_string(mode.lanes) + " 以外）のノーツ " +
                                                   std::to_string(stats.lanesOutsideMode) +
                                                   " 個を、BGM として書き出します"});
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

core::BmsWriteOptions writeOptionsFor(const ChartRequest& request, const ConvertOptions& options) {
    core::BmsWriteOptions writeOptions;
    writeOptions.difficulty = request.difficulty ? request.difficulty : options.difficulty;
    return writeOptions;
}

void checkSameFolder(const std::vector<ChartRequest>& requests, std::vector<Message>& messages) {
    const auto folderOf = [](const std::filesystem::path& path) {
        return std::filesystem::absolute(path).parent_path().lexically_normal();
    };
    for (const auto& request : requests) {
        if (folderOf(request.path) != folderOf(requests.front().path)) {
            // Why not 別のフォルダでも読む: 音声ファイルの名前は bmson のフォルダが基準のため、フォルダが違うと同じ名前でも別の音になる
            messages.push_back({Severity::Error, "複数の bmson をまとめて変換するときは、すべて同じフォルダに置いてください: " +
                                                     infra::pathToUtf8(request.path)});
            return;
        }
    }
}

std::string fileNameOf(const std::filesystem::path& path) {
    return infra::pathToUtf8(path.filename());
}

void reportTimingDifferences(ConversionSet& set) {
    const auto& base = set.charts[set.baseIndex].chart;
    for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
        const auto& other = set.charts[chart].chart;
        if (chart == set.baseIndex) {
            continue;
        }
        if (other.info.initBpm != base.info.initBpm || other.info.resolution != base.info.resolution ||
            other.bpmEvents != base.bpmEvents || other.stopEvents != base.stopEvents || other.lines != base.lines) {
            set.charts[chart].messages.push_back(
                {Severity::Warning, "基準の譜面 (" + fileNameOf(set.charts[set.baseIndex].chartPath) +
                                        ") と BPM・STOP・小節線・resolution のどれかが違います。別の曲の bmson が混ざっていないか確かめてください"});
        }
    }
}

void reportAlignment(const core::CutAlignment& alignment, std::vector<Message>& messages) {
    if (alignment.shortenedNotes == 0) {
        return;
    }
    messages.push_back({Severity::Info, "ほかの譜面の切り出し位置に合わせて、" + std::to_string(alignment.shortenedNotes) +
                                            " 個のノーツの音を短く切り、続きを鳴らす BGM のノーツを " +
                                            std::to_string(alignment.addedNotes) + " 個加えました"});
}

bool anyChartHasErrors(const ConversionSet& set) {
    return std::any_of(set.charts.begin(), set.charts.end(),
                       [](const Conversion& conversion) { return containsError(conversion.messages); });
}

constexpr std::size_t kOutsideBaseExamples = 3;

void reportOutsideBase(Conversion& conversion, const core::SlicePlan& localPlan, const std::vector<std::size_t>& globalIndexOf,
                       bool allowExtra) {
    if (conversion.outsideBase.empty()) {
        return;
    }
    const core::MeasureLayout layout(conversion.chart.info.resolution, conversion.chart.lines);
    std::vector<std::string> examples;
    bool more = false;
    for (const std::size_t global : conversion.outsideBase) {
        const auto local = std::find(globalIndexOf.begin(), globalIndexOf.end(), global) - globalIndexOf.begin();
        const core::Slice& slice = localPlan.slices[static_cast<std::size_t>(local)];
        char measure[16];
        std::snprintf(measure, sizeof measure, "%03d", layout.locate(slice.firstPulse).measure);
        const std::string example = std::string("小節 ") + measure + " の " + slice.source;
        if (std::find(examples.begin(), examples.end(), example) != examples.end()) {
            continue;
        }
        if (examples.size() == kOutsideBaseExamples) {
            more = true;
            break;
        }
        examples.push_back(example);
    }
    std::string listed;
    for (const auto& example : examples) {
        listed += (listed.empty() ? "" : "、") + example;
    }
    if (more) {
        listed += " など";
    }
    const std::string count = std::to_string(conversion.outsideBase.size());
    if (allowExtra) {
        conversion.messages.push_back({Severity::Info, "基準の譜面にないスライス " + count + " 個を追加で切り出します（" + listed + "）"});
    } else {
        conversion.messages.push_back({Severity::Error, "基準の譜面にないスライスが " + count + " 個あります（" + listed +
                                                            "）。「基準の譜面にないスライスを追加で切り出す」を選ぶと書き出せます"});
    }
}

}  // namespace

std::size_t defaultBaseIndex(const std::vector<ChartSummary>& charts) {
    std::size_t best = 0;
    for (std::size_t index = 1; index < charts.size(); ++index) {
        const auto& candidate = charts[index];
        const auto& current = charts[best];
        if (candidate.level > current.level || (candidate.level == current.level && candidate.keyNotes > current.keyNotes)) {
            best = index;
        }
    }
    return best;
}

std::optional<int> guessDifficulty(const std::string& chartName) {
    std::string upper = chartName;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
    static const std::vector<std::pair<std::string, int>> kNames = {
        {"BEGINNER", 1}, {"NORMAL", 2}, {"HYPER", 3}, {"ANOTHER", 4}, {"INSANE", 5}, {"LEGGENDARIA", 5}};
    for (const auto& [name, difficulty] : kNames) {
        if (upper.find(name) != std::string::npos) {
            return difficulty;
        }
    }
    return std::nullopt;
}

std::string extensionOf(SoundFormat format) {
    return format == SoundFormat::Ogg ? "ogg" : "wav";
}

bool Conversion::hasErrors() const {
    return containsError(messages) || !document;
}

bool ConversionSet::hasErrors() const {
    return containsError(messages) || charts.empty() ||
           std::any_of(charts.begin(), charts.end(), [](const Conversion& conversion) { return conversion.hasErrors(); });
}

bool RenderedBms::hasErrors() const {
    return containsError(messages);
}

bool ExportOutcome::hasErrors() const {
    return containsError(messages);
}

std::string soundFileOnDisk(const Conversion& conversion, std::size_t index) {
    const std::string& name = conversion.soundFiles.at(index);
    return conversion.soundFolder.empty() ? name : conversion.soundFolder + "/" + name;
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

// Why not bmson に書かれた名前だけを探す: 音声を wav から ogg に変換したのに bmson を書き直し忘れることがよくあり、
// 名前の拡張子だけが違うファイルなら同じ音として使えるため
std::optional<std::string> locateSoundFile(const std::string& declared, const std::filesystem::path& directory,
                                           const AudioSource& audio) {
    static const std::vector<std::string> kExtensions = {".wav", ".ogg", ".flac", ".mp3"};
    if (audio.exists(directory / infra::pathFromUtf8(declared))) {
        return declared;
    }
    const std::string declaredExtension = lowerExtension(declared);
    for (const auto& extension : kExtensions) {
        if (extension == declaredExtension) {
            continue;
        }
        std::filesystem::path candidate = infra::pathFromUtf8(declared);
        candidate.replace_extension(infra::pathFromUtf8(extension));
        if (audio.exists(directory / candidate)) {
            return infra::pathToUtf8(candidate);
        }
    }
    return std::nullopt;
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

bool ConvertService::loadAndPlan(Conversion& conversion) const {
    try {
        LoadedChart loaded = charts_.load(conversion.chartPath);
        conversion.chart = std::move(loaded.chart);
        for (auto& warning : loaded.warnings) {
            conversion.messages.push_back({Severity::Warning, std::move(warning)});
        }
    } catch (const std::exception& error) {
        conversion.messages.push_back({Severity::Error, std::string("bmson を読み込めません: ") + error.what()});
        return false;
    }

    const auto& chart = conversion.chart;
    const core::KeyMode* mode = core::findKeyMode(chart.info.modeHint);
    if (mode == nullptr) {
        conversion.messages.push_back({Severity::Error, "未対応のキーモードです: " + chart.info.modeHint + "（対応しているのは " +
                                                            core::supportedModeNames() + "）"});
        return false;
    }

    const std::filesystem::path directory = conversion.chartPath.parent_path();
    std::map<std::string, std::string> located;
    for (auto& channel : conversion.chart.soundChannels) {
        if (channel.name.empty()) {
            continue;
        }
        auto found = located.find(channel.name);
        if (found == located.end()) {
            const auto actual = locateSoundFile(channel.name, directory, audio_);
            found = located.emplace(channel.name, actual.value_or(channel.name)).first;
            if (actual && *actual != channel.name) {
                conversion.messages.push_back({Severity::Warning, "bmson では " + channel.name + " ですが見つからないため、同じ名前で拡張子が違う " +
                                                                      *actual + " を使います"});
            }
        }
        channel.name = found->second;
    }

    std::map<std::string, core::AudioInfo> probed;
    std::vector<core::AudioInfo> channelAudio;
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
        return false;
    }

    try {
        conversion.plan = core::planSlices(chart, core::bmson::timingOf(chart), channelAudio, mode->lanes);
    } catch (const std::invalid_argument& error) {
        conversion.messages.push_back({Severity::Error, std::string("bmson の内容を解釈できません: ") + error.what()});
        return false;
    }
    reportPlanStats(conversion.plan.stats, *mode, conversion.messages);
    reportUnsupportedInfo(chart.info, conversion.messages);
    return true;
}

Conversion ConvertService::prepare(const std::filesystem::path& chartPath, const ConvertOptions& options) const {
    ConversionSet set = prepareSet({ChartRequest{chartPath, options.difficulty}}, 0, options);
    Conversion conversion = std::move(set.charts.front());
    conversion.messages.insert(conversion.messages.begin(), set.messages.begin(), set.messages.end());
    return conversion;
}

ConversionSet ConvertService::prepareSet(const std::vector<ChartRequest>& requests, std::size_t baseIndex,
                                         const ConvertOptions& options) const {
    ConversionSet set;
    if (requests.empty()) {
        set.messages.push_back({Severity::Error, "bmson が選ばれていません"});
        return set;
    }
    set.baseIndex = baseIndex < requests.size() ? baseIndex : 0;
    for (const auto& request : requests) {
        Conversion conversion;
        conversion.chartPath = request.path;
        set.charts.push_back(std::move(conversion));
    }
    checkOptions(options, set.messages);
    checkSameFolder(requests, set.messages);
    if (containsError(set.messages)) {
        return set;
    }

    bool planned = true;
    for (auto& conversion : set.charts) {
        planned = loadAndPlan(conversion) && planned;
    }
    if (!planned) {
        return set;
    }
    reportTimingDifferences(set);

    std::vector<core::SlicePlan> plans;
    std::vector<core::TimingMap> timings;
    for (const auto& conversion : set.charts) {
        plans.push_back(conversion.plan);
        timings.push_back(core::bmson::timingOf(conversion.chart));
    }
    const std::vector<core::CutAlignment> alignments = core::alignSliceCuts(plans, timings);
    for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
        reportAlignment(alignments[chart], set.charts[chart].messages);
    }
    const core::MergedSlices merged = core::mergeSlicePlans(plans, set.baseIndex);
    set.sliceUsers.resize(merged.slices.size());
    for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
        Conversion& conversion = set.charts[chart];
        conversion.outsideBase = merged.outsideBase[chart];
        reportOutsideBase(conversion, plans[chart], merged.globalIndexOf[chart], options.allowExtraSlices);
        conversion.plan = merged.planFor(plans[chart], chart);
        std::set<std::size_t> used;
        for (const auto& note : conversion.plan.notes) {
            used.insert(note.slice);
        }
        for (const std::size_t slice : used) {
            set.sliceUsers[slice].push_back(chart);
        }
    }
    if (anyChartHasErrors(set)) {
        return set;
    }

    // Why not 名前を付けてから 1 回だけ組み立てる: {id} を名前に使うには 36進数か 62進数かが先に決まっている必要があり、
    // それは定義の数で決まる。名前に関係しないので、仮の名前で一度組み立てて進数を確かめる
    const std::vector<std::string> placeholders(merged.slices.size(), "_");
    core::IdBase namingBase = core::IdBase::Base36;
    for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
        Conversion& conversion = set.charts[chart];
        core::BmsWriteResult trial =
            core::writeBms(conversion.chart, conversion.plan, placeholders, writeOptionsFor(requests[chart], options));
        if (!trial.document) {
            appendWriterMessages(trial, conversion.messages);
            continue;
        }
        // Why not 基準の譜面の進数だけで決める: 追加で切り出したスライスの ID は基準の譜面の定義の数を超えることがあり、
        // 36進数では表せない ID が名前に入るおそれがあるため、どれかが 62進数なら 62進数にする
        if (trial.document->base == core::IdBase::Base62) {
            namingBase = core::IdBase::Base62;
        }
    }
    if (anyChartHasErrors(set)) {
        return set;
    }

    const Conversion& base = set.charts[set.baseIndex];
    NamingResult naming = nameSounds(base.chart, base.plan, namingBase, options.naming, extensionOf(options.soundFormat),
                                     options.encoding);
    set.messages.insert(set.messages.end(), naming.messages.begin(), naming.messages.end());
    if (naming.hasErrors()) {
        return set;
    }

    for (std::size_t chart = 0; chart < set.charts.size(); ++chart) {
        Conversion& conversion = set.charts[chart];
        conversion.soundFiles = naming.paths;
        conversion.soundFolder = naming.folder;
        core::BmsWriteResult written =
            core::writeBms(conversion.chart, conversion.plan, conversion.soundFiles, writeOptionsFor(requests[chart], options));
        appendWriterMessages(written, conversion.messages);
        conversion.document = std::move(written.document);
    }
    return set;
}

ChartSummary ConvertService::summarize(const std::filesystem::path& chartPath) const {
    ChartSummary summary;
    summary.path = chartPath;
    try {
        const LoadedChart loaded = charts_.load(chartPath);
        const auto& info = loaded.chart.info;
        summary.title = info.title;
        summary.chartName = info.chartName;
        summary.level = info.level;
        summary.modeHint = info.modeHint;
        const core::KeyMode* mode = core::findKeyMode(info.modeHint);
        const int lanes = mode != nullptr ? mode->lanes : core::kBeat7kLanes;
        for (const auto& channel : loaded.chart.soundChannels) {
            summary.keyNotes += static_cast<int>(std::count_if(channel.notes.begin(), channel.notes.end(),
                                                               [lanes](const auto& note) { return note.x >= 1 && note.x <= lanes; }));
        }
    } catch (const std::exception& error) {
        summary.error = std::string("bmson を読み込めません: ") + error.what();
    }
    return summary;
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
            const std::filesystem::path target = bmsDirectory / infra::pathFromUtf8(soundFileOnDisk(conversion, index));
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
    if (const core::KeyMode* mode = core::findKeyMode(conversion.chart.info.modeHint)) {
        const bool pms = lowerExtension(infra::pathToUtf8(output)) == ".pms";
        // Why not 拡張子を気にしない: プレイヤーは拡張子が .pms かどうかで pop'n の譜面かを決めるため、合っていないと別のモードとして読まれる
        if (pms != (mode->extension == "pms")) {
            outcome.messages.push_back({Severity::Warning, mode->name + " の譜面は拡張子を ." + mode->extension +
                                                               " にしてください（.pms かどうかで、プレイヤーが pop'n の譜面かを決めるため）"});
        }
    }
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

std::filesystem::path ConvertService::defaultOutputPath(const std::filesystem::path& chartPath, const std::string& modeHint) {
    const core::KeyMode* mode = core::findKeyMode(modeHint);
    std::filesystem::path output = chartPath;
    output.replace_extension("." + (mode != nullptr ? mode->extension : std::string("bme")));
    return output;
}

}  // namespace b2b::app
