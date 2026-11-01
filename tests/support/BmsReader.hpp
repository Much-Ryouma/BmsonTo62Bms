#pragma once

#include <algorithm>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "b2b/core/bms/ObjectId.hpp"

namespace b2b::test {

struct ReadNote {
    double seconds = 0.0;
    std::string channel;
    int id = 0;
};

struct ReadLongNote {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    std::string channel;
    int id = 0;
};

struct ReadBms {
    core::IdBase base = core::IdBase::Base36;
    std::map<int, std::string> wav;
    std::vector<ReadNote> notes;
    std::vector<ReadLongNote> longNotes;
};

// Why not bmsonto62bms の MeasureLayout や TimingMap を使う: 書き出し側と同じ計算を使うと、
// 計算の誤りがあっても読み戻しで一致してしまい見つけられないため、BMS の仕様どおりに別に計算する
inline ReadBms readBms(const std::string& text) {
    struct RawEvent {
        int measure;
        double position;
        std::string channel;
        std::string id;
    };
    ReadBms result;
    double bpm = 0.0;
    std::map<std::string, std::string> wavRaw;
    std::map<std::string, double> bpmRaw;
    std::map<std::string, double> stopRaw;
    std::map<int, double> ratios;
    std::vector<RawEvent> events;

    const std::regex data(R"(^#(\d{3})([0-9A-Za-z]{2}):(.*)$)");
    const std::regex definition(R"(^#(WAV|BPM|STOP)([0-9A-Za-z]{2}) (.*)$)");
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::smatch match;
        if (line == "#BASE 62") {
            result.base = core::IdBase::Base62;
        } else if (line.rfind("#BPM ", 0) == 0) {
            bpm = std::stod(line.substr(5));
        } else if (std::regex_match(line, match, definition)) {
            if (match[1] == "WAV") { wavRaw[match[2]] = match[3]; }
            if (match[1] == "BPM") { bpmRaw[match[2]] = std::stod(match[3]); }
            if (match[1] == "STOP") { stopRaw[match[2]] = std::stod(match[3]); }
        } else if (std::regex_match(line, match, data)) {
            const int measure = std::stoi(match[1]);
            const std::string channel = match[2];
            const std::string body = match[3];
            if (channel == "02") {
                ratios[measure] = std::stod(body);
                continue;
            }
            const std::size_t slots = body.size() / 2;
            for (std::size_t slot = 0; slot < slots; ++slot) {
                const std::string id = body.substr(slot * 2, 2);
                if (id != "00") {
                    events.push_back({measure, static_cast<double>(slot) / static_cast<double>(slots), channel, id});
                }
            }
        }
    }

    auto idOf = [&result](const std::string& id) {
        const auto value = core::parseObjectId(id, result.base);
        if (!value) {
            throw std::runtime_error("bad id: " + id);
        }
        return *value;
    };
    for (const auto& [id, file] : wavRaw) {
        result.wav[idOf(id)] = file;
    }
    std::map<int, double> bpmValues;
    for (const auto& [id, value] : bpmRaw) { bpmValues[idOf(id)] = value; }
    std::map<int, double> stopValues;
    for (const auto& [id, value] : stopRaw) { stopValues[idOf(id)] = value; }

    std::stable_sort(events.begin(), events.end(), [](const RawEvent& a, const RawEvent& b) {
        return a.measure != b.measure ? a.measure < b.measure : a.position < b.position;
    });
    int lastMeasure = 0;
    for (const auto& event : events) {
        lastMeasure = std::max(lastMeasure, event.measure);
    }

    double seconds = 0.0;
    std::map<std::string, std::vector<std::pair<double, int>>> longNoteObjects;
    auto event = events.cbegin();
    for (int measure = 0; measure <= lastMeasure; ++measure) {
        const double beats = 4.0 * (ratios.contains(measure) ? ratios[measure] : 1.0);
        double position = 0.0;
        while (event != events.cend() && event->measure == measure) {
            seconds += (event->position - position) * beats * 60.0 / bpm;
            position = event->position;
            auto group = event;
            for (; group != events.cend() && group->measure == measure && group->position == position; ++group) {
                const std::string& channel = group->channel;
                if (channel == "01" || channel.front() == '1') {
                    result.notes.push_back({seconds, channel, idOf(group->id)});
                } else if (channel.front() == '5') {
                    longNoteObjects[channel].push_back({seconds, idOf(group->id)});
                }
            }
            for (auto change = event; change != group; ++change) {
                if (change->channel == "08") {
                    bpm = bpmValues.at(idOf(change->id));
                }
            }
            for (auto stop = event; stop != group; ++stop) {
                if (stop->channel == "09") {
                    seconds += stopValues.at(idOf(stop->id)) / 48.0 * 60.0 / bpm;
                }
            }
            event = group;
        }
        seconds += (1.0 - position) * beats * 60.0 / bpm;
    }

    for (const auto& [channel, objects] : longNoteObjects) {
        for (std::size_t index = 0; index + 1 < objects.size(); index += 2) {
            result.longNotes.push_back({objects[index].first, objects[index + 1].first, channel, objects[index].second});
        }
    }
    return result;
}

}  // namespace b2b::test
