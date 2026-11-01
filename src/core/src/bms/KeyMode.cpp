#include "b2b/core/bms/KeyMode.hpp"

#include <array>
#include <stdexcept>

namespace b2b::core {

namespace {

// Why not bmson のレーン番号をそのままチャンネルにする: bmson は鍵盤を左から順に数えるが、BMS はスクラッチが 16、
// 6・7 鍵が 18・19 と飛び飛びで、pop'n の 6〜9 ボタンは 2P 側の 22〜25 を使うため、モードごとに対応表を持つ
// （beatoraja / jbms-parser の bmson と BMS の読み込みと同じ割り当て）
const std::array<KeyMode, 3>& modes() {
    static const std::array<KeyMode, 3> kModes = {
        KeyMode{"beat-7k", 8, {"11", "12", "13", "14", "15", "18", "19", "16"}, 1, "bme", 75},
        KeyMode{"beat-14k", 16,
                {"11", "12", "13", "14", "15", "18", "19", "16", "21", "22", "23", "24", "25", "28", "29", "26"}, 3,
                "bme", 75},
        KeyMode{"popn-9k", 9, {"11", "12", "13", "14", "15", "22", "23", "24", "25"}, 1, "pms", 70},
    };
    return kModes;
}

}  // namespace

const KeyMode* findKeyMode(std::string_view modeHint) {
    const std::string_view name = modeHint.empty() ? std::string_view("beat-7k") : modeHint;
    for (const auto& mode : modes()) {
        if (mode.name == name) {
            return &mode;
        }
    }
    return nullptr;
}

std::string supportedModeNames() {
    std::string names;
    for (const auto& mode : modes()) {
        names += names.empty() ? mode.name : " / " + mode.name;
    }
    return names;
}

std::string channelForLane(const KeyMode& mode, int lane, bool longNote) {
    if (lane < 1 || lane > mode.lanes) {
        throw std::out_of_range("lane out of range for " + mode.name + ": " + std::to_string(lane));
    }
    std::string channel = mode.channels[static_cast<std::size_t>(lane - 1)];
    if (longNote) {
        channel.front() = channel.front() == '1' ? '5' : '6';
    }
    return channel;
}

}  // namespace b2b::core
