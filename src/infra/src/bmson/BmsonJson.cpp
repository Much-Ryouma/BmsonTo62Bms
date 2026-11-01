#include "b2b/infra/bmson/BmsonJson.hpp"

#include <cmath>
#include <limits>
#include <string_view>

#include <nlohmann/json.hpp>

namespace b2b::infra {

namespace {

using Json = nlohmann::json;
using core::Pulse;
namespace bmson = core::bmson;

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
constexpr int kDefaultResolution = 240;

std::string stringOf(const Json& object, const char* key, std::string fallback = {}) {
    const auto found = object.find(key);
    if (found == object.end() || found->is_null()) {
        return fallback;
    }
    if (!found->is_string()) {
        throw BmsonFormatError(std::string(key) + " は文字列である必要があります");
    }
    return found->get<std::string>();
}

double numberOf(const Json& object, const char* key, double fallback) {
    const auto found = object.find(key);
    if (found == object.end() || found->is_null()) {
        return fallback;
    }
    if (!found->is_number()) {
        throw BmsonFormatError(std::string(key) + " は数値である必要があります");
    }
    return found->get<double>();
}

Pulse integerOf(const Json& object, const char* key, Pulse fallback, const std::string& where) {
    const double value = numberOf(object, key, static_cast<double>(fallback));
    if (std::floor(value) != value || std::abs(value) > static_cast<double>(std::numeric_limits<Pulse>::max() / 2)) {
        throw BmsonFormatError(where + key + " は整数である必要があります");
    }
    return static_cast<Pulse>(value);
}

const Json& arrayOf(const Json& object, const char* key) {
    static const Json kEmpty = Json::array();
    const auto found = object.find(key);
    if (found == object.end() || found->is_null()) {
        return kEmpty;
    }
    if (!found->is_array()) {
        throw BmsonFormatError(std::string(key) + " は配列である必要があります");
    }
    return *found;
}

void readInfo(const Json& root, bmson::Chart& chart, std::vector<std::string>& warnings) {
    const auto found = root.find("info");
    if (found == root.end() || !found->is_object()) {
        throw BmsonFormatError("info がありません");
    }
    const Json& info = *found;
    auto& song = chart.info;
    song.title = stringOf(info, "title");
    song.subtitle = stringOf(info, "subtitle");
    song.artist = stringOf(info, "artist");
    for (const auto& subartist : arrayOf(info, "subartists")) {
        if (!subartist.is_string()) {
            throw BmsonFormatError("info.subartists の要素は文字列である必要があります");
        }
        song.subartists.push_back(subartist.get<std::string>());
    }
    song.genre = stringOf(info, "genre");
    song.modeHint = stringOf(info, "mode_hint", "beat-7k");
    song.chartName = stringOf(info, "chart_name");
    song.backImage = stringOf(info, "back_image");
    song.eyecatchImage = stringOf(info, "eyecatch_image");
    song.titleImage = stringOf(info, "title_image");
    song.bannerImage = stringOf(info, "banner_image");
    song.previewMusic = stringOf(info, "preview_music");
    song.judgeRank = numberOf(info, "judge_rank", 100.0);
    song.total = std::abs(numberOf(info, "total", 100.0));
    song.level = std::max<Pulse>(0, integerOf(info, "level", 0, "info."));

    if (!info.contains("init_bpm") || info["init_bpm"].is_null()) {
        throw BmsonFormatError("info.init_bpm がありません（bmson 仕様で必須です）");
    }
    song.initBpm = numberOf(info, "init_bpm", 0.0);
    if (!(song.initBpm > 0.0)) {
        throw BmsonFormatError("info.init_bpm は正の数である必要があります");
    }

    const Pulse resolution = integerOf(info, "resolution", kDefaultResolution, "info.");
    if (resolution < 0) {
        warnings.push_back("info.resolution が負のため、仕様どおり絶対値として読み込みます");
    }
    const Pulse absolute = resolution < 0 ? -resolution : resolution;
    if (absolute > std::numeric_limits<int>::max()) {
        throw BmsonFormatError("info.resolution が大きすぎます");
    }
    song.resolution = absolute == 0 ? kDefaultResolution : static_cast<int>(absolute);
}

void readTiming(const Json& root, bmson::Chart& chart) {
    for (const auto& line : arrayOf(root, "lines")) {
        chart.lines.push_back(integerOf(line, "y", 0, "lines[]."));
    }
    for (const auto& event : arrayOf(root, "bpm_events")) {
        const double bpm = numberOf(event, "bpm", 0.0);
        if (!(bpm > 0.0)) {
            throw BmsonFormatError("bpm_events[].bpm は正の数である必要があります");
        }
        chart.bpmEvents.push_back({integerOf(event, "y", 0, "bpm_events[]."), bpm});
    }
    for (const auto& event : arrayOf(root, "stop_events")) {
        chart.stopEvents.push_back(
            {integerOf(event, "y", 0, "stop_events[]."), integerOf(event, "duration", 0, "stop_events[].")});
    }
}

int laneOf(const Json& note) {
    const auto found = note.find("x");
    if (found == note.end() || found->is_null()) {
        return 0;
    }
    if (!found->is_number() || std::floor(found->get<double>()) != found->get<double>()) {
        throw BmsonFormatError("sound_channels[].notes[].x は整数か null である必要があります");
    }
    return static_cast<int>(found->get<double>());
}

bool flagOf(const Json& note, const char* key) {
    const auto found = note.find(key);
    return found != note.end() && found->is_boolean() && found->get<bool>();
}

void readSoundChannels(const Json& root, bmson::Chart& chart) {
    const auto found = root.find("sound_channels");
    if (found == root.end() || !found->is_array()) {
        throw BmsonFormatError("sound_channels がありません");
    }
    for (const auto& channelJson : *found) {
        bmson::SoundChannel channel;
        channel.name = stringOf(channelJson, "name");
        for (const auto& noteJson : arrayOf(channelJson, "notes")) {
            if (!noteJson.contains("y")) {
                throw BmsonFormatError("sound_channels '" + channel.name + "' に y のないノーツがあります");
            }
            bmson::Note note;
            note.x = laneOf(noteJson);
            note.y = integerOf(noteJson, "y", 0, "notes[].");
            note.l = integerOf(noteJson, "l", 0, "notes[].");
            note.c = flagOf(noteJson, "c");
            note.up = flagOf(noteJson, "up");
            if (note.y < 0) {
                throw BmsonFormatError("sound_channels '" + channel.name + "' に y が負のノーツがあります");
            }
            channel.notes.push_back(note);
        }
        chart.soundChannels.push_back(std::move(channel));
    }
}

std::vector<bmson::BgaEvent> readBgaEvents(const Json& bga, const char* key) {
    std::vector<bmson::BgaEvent> events;
    for (const auto& event : arrayOf(bga, key)) {
        events.push_back({integerOf(event, "y", 0, std::string(key) + "[]."), integerOf(event, "id", 0, std::string(key) + "[].")});
    }
    return events;
}

void readBga(const Json& root, bmson::Chart& chart) {
    const auto found = root.find("bga");
    if (found == root.end() || found->is_null()) {
        return;
    }
    for (const auto& header : arrayOf(*found, "bga_header")) {
        chart.bga.headers.push_back({integerOf(header, "id", 0, "bga_header[]."), stringOf(header, "name")});
    }
    chart.bga.bgaEvents = readBgaEvents(*found, "bga_events");
    chart.bga.layerEvents = readBgaEvents(*found, "layer_events");
    chart.bga.poorEvents = readBgaEvents(*found, "poor_events");
}

}  // namespace

ParsedBmson parseBmson(const std::string& text) {
    std::string_view body = text;
    if (body.starts_with(kUtf8Bom)) {
        body.remove_prefix(kUtf8Bom.size());
    }
    Json root;
    try {
        root = Json::parse(body);
    } catch (const Json::parse_error& error) {
        throw BmsonFormatError(std::string("JSON として読めません: ") + error.what());
    }
    if (!root.is_object()) {
        throw BmsonFormatError("bmson の最上位がオブジェクトではありません");
    }
    ParsedBmson parsed;
    try {
        parsed.chart.version = stringOf(root, "version");
        readInfo(root, parsed.chart, parsed.warnings);
        readTiming(root, parsed.chart);
        readSoundChannels(root, parsed.chart);
        readBga(root, parsed.chart);
    } catch (const Json::exception& error) {
        throw BmsonFormatError(std::string("bmson の形式が正しくありません: ") + error.what());
    }
    return parsed;
}

}  // namespace b2b::infra
