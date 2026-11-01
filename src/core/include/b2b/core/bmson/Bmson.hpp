#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "b2b/core/timing/TimingMap.hpp"

namespace b2b::core::bmson {

struct Info {
    std::string title;
    std::string subtitle;
    std::string artist;
    std::vector<std::string> subartists;
    std::string genre;
    std::string modeHint = "beat-7k";
    std::string chartName;
    std::int64_t level = 0;
    double initBpm = 0.0;
    double judgeRank = 100.0;
    double total = 100.0;
    std::string backImage;
    std::string eyecatchImage;
    std::string titleImage;
    std::string bannerImage;
    std::string previewMusic;
    int resolution = 240;
};

struct Note {
    int x = 0;
    Pulse y = 0;
    Pulse l = 0;
    bool c = false;
    bool up = false;
};

struct SoundChannel {
    std::string name;
    std::vector<Note> notes;
};

struct BgaHeader {
    std::int64_t id = 0;
    std::string name;
};

struct BgaEvent {
    Pulse y = 0;
    std::int64_t id = 0;
};

struct Bga {
    std::vector<BgaHeader> headers;
    std::vector<BgaEvent> bgaEvents;
    std::vector<BgaEvent> layerEvents;
    std::vector<BgaEvent> poorEvents;
};

struct Chart {
    std::string version;
    Info info;
    std::vector<Pulse> lines;
    std::vector<BpmEvent> bpmEvents;
    std::vector<StopEvent> stopEvents;
    std::vector<SoundChannel> soundChannels;
    Bga bga;
};

[[nodiscard]] TimingMap timingOf(const Chart& chart);

}  // namespace b2b::core::bmson
