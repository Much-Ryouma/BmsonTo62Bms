#include "b2b/core/timing/MeasureLayout.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

namespace b2b::core {

namespace {

constexpr Pulse kBeatsPerStandardMeasure = 4;

}  // namespace

MeasureLayout::MeasureLayout(int resolution, std::vector<Pulse> barLines)
    : standardLength_(kBeatsPerStandardMeasure * resolution) {
    if (resolution <= 0) {
        throw std::invalid_argument("resolution must be positive, got " + std::to_string(resolution));
    }
    std::erase_if(barLines, [](Pulse y) { return y < 0; });
    barLines.push_back(0);
    std::sort(barLines.begin(), barLines.end());
    barLines.erase(std::unique(barLines.begin(), barLines.end()), barLines.end());
    starts_ = std::move(barLines);
}

MeasurePosition MeasureLayout::locate(Pulse y) const {
    if (y < 0) {
        throw std::invalid_argument("pulse must not be negative, got " + std::to_string(y));
    }
    const auto firstAfter = std::upper_bound(starts_.cbegin(), starts_.cend(), y);
    const auto index = static_cast<int>(std::distance(starts_.cbegin(), firstAfter)) - 1;
    if (firstAfter != starts_.cend()) {
        return MeasurePosition{index, y - starts_[static_cast<std::size_t>(index)]};
    }
    const Pulse sinceLast = y - starts_.back();
    const auto extra = static_cast<int>(sinceLast / standardLength_);
    return MeasurePosition{index + extra, sinceLast % standardLength_};
}

Pulse MeasureLayout::startOf(int measure) const {
    if (measure < 0) {
        throw std::invalid_argument("measure must not be negative, got " + std::to_string(measure));
    }
    const auto index = static_cast<std::size_t>(measure);
    if (index < starts_.size()) {
        return starts_[index];
    }
    return starts_.back() + static_cast<Pulse>(index - (starts_.size() - 1)) * standardLength_;
}

Pulse MeasureLayout::lengthOf(int measure) const {
    return startOf(measure + 1) - startOf(measure);
}

}  // namespace b2b::core
