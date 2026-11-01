#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace b2b::core {

struct KeyMode {
    std::string name;
    int lanes = 0;
    std::vector<std::string> channels;
    int player = 1;
    std::string extension;
    int normalJudgePercent = 75;
};

[[nodiscard]] const KeyMode* findKeyMode(std::string_view modeHint);

[[nodiscard]] std::string supportedModeNames();

[[nodiscard]] std::string channelForLane(const KeyMode& mode, int lane, bool longNote);

}  // namespace b2b::core
