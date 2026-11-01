#pragma once

#include <optional>
#include <string>

namespace b2b::core {

enum class JudgeHeaderKind { None, Rank, DefExRank };

struct JudgeHeader {
    JudgeHeaderKind kind = JudgeHeaderKind::None;
    int value = 0;

    friend bool operator==(const JudgeHeader&, const JudgeHeader&) = default;
};

[[nodiscard]] JudgeHeader judgeHeaderFor(double bmsonJudgeRank, int normalJudgePercent = 75);

[[nodiscard]] double defaultTotalFor(int totalNotes);

[[nodiscard]] std::optional<double> bmsTotalFor(double bmsonTotal, int totalNotes);

[[nodiscard]] std::string formatDecimal(double value);

}  // namespace b2b::core
