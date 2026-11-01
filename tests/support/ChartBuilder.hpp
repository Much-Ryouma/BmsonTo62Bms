#pragma once

#include <string>
#include <utility>
#include <vector>

#include "b2b/core/bmson/Bmson.hpp"

namespace b2b::test {

inline core::bmson::Chart makeChart(double bpm = 120.0, int resolution = 240) {
    core::bmson::Chart chart;
    chart.version = "1.0.0";
    chart.info.title = "Title";
    chart.info.artist = "Artist";
    chart.info.genre = "Genre";
    chart.info.initBpm = bpm;
    chart.info.resolution = resolution;
    return chart;
}

inline core::bmson::Note note(int x, core::Pulse y, bool c = false, core::Pulse l = 0) {
    core::bmson::Note result;
    result.x = x;
    result.y = y;
    result.c = c;
    result.l = l;
    return result;
}

inline core::bmson::SoundChannel channel(std::string name, std::vector<core::bmson::Note> notes) {
    return core::bmson::SoundChannel{std::move(name), std::move(notes)};
}

}  // namespace b2b::test
