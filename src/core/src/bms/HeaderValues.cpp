#include "b2b/core/bms/HeaderValues.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace b2b::core {

namespace {

constexpr int kNormalWindowPercent = 75;
constexpr int kSmallestJudgeRankForExRank = 5;
constexpr double kDefaultTotalFloor = 260.0;
constexpr double kStandardTotal = 100.0;

}  // namespace

JudgeHeader judgeHeaderFor(double bmsonJudgeRank) {
    const auto rank = static_cast<int>(std::lround(bmsonJudgeRank));
    if (rank <= 0) {
        return {};
    }
    if (rank < kSmallestJudgeRankForExRank) {
        // Why not #DEFEXRANK にする: beatoraja は 5 未満の judge_rank を BMS の #RANK として扱うため、同じ判定幅になるよう #RANK で書く
        return {JudgeHeaderKind::Rank, rank};
    }
    // Why not judge_rank をそのまま #DEFEXRANK に書く: beatoraja は #DEFEXRANK を「NORMAL の判定幅 (75%) に対する割合」として読み、
    // bmson の judge_rank は判定幅の割合そのものとして読むため、そのまま書くと判定が 3/4 に狭くなる
    return {JudgeHeaderKind::DefExRank, (rank * 100 + kNormalWindowPercent - 1) / kNormalWindowPercent};
}

double defaultTotalFor(int totalNotes) {
    const double notes = static_cast<double>(std::max(0, totalNotes));
    return std::max(kDefaultTotalFloor, 7.605 * notes / (0.01 * notes + 6.5));
}

std::optional<double> bmsTotalFor(double bmsonTotal, int totalNotes) {
    if (!(bmsonTotal > 0.0) || bmsonTotal == kStandardTotal) {
        return std::nullopt;
    }
    return bmsonTotal / kStandardTotal * defaultTotalFor(totalNotes);
}

std::string formatDecimal(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    std::string text(buffer);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    if (text == "-0") {
        return "0";
    }
    return text;
}

}  // namespace b2b::core
