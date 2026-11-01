#pragma once

#include <vector>

#include "b2b/core/timing/TimingMap.hpp"

namespace b2b::core {

struct MeasurePosition {
    int measure = 0;
    Pulse offset = 0;

    friend bool operator==(const MeasurePosition&, const MeasurePosition&) = default;
};

class MeasureLayout {
public:
    MeasureLayout(int resolution, std::vector<Pulse> barLines);

    [[nodiscard]] MeasurePosition locate(Pulse y) const;
    [[nodiscard]] Pulse startOf(int measure) const;
    [[nodiscard]] Pulse lengthOf(int measure) const;
    [[nodiscard]] Pulse standardLength() const noexcept { return standardLength_; }

private:
    Pulse standardLength_;
    std::vector<Pulse> starts_;
};

}  // namespace b2b::core
