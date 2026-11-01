#pragma once

#include <cstddef>
#include <vector>

#include "b2b/core/slicing/SlicePlan.hpp"
#include "b2b/core/timing/TimingMap.hpp"

namespace b2b::core {

struct MergedSlices {
    std::vector<Slice> slices;
    std::vector<std::vector<std::size_t>> globalIndexOf;
    std::vector<std::vector<std::size_t>> outsideBase;

    [[nodiscard]] SlicePlan planFor(const SlicePlan& plan, std::size_t chart) const;
};

struct CutAlignment {
    int shortenedNotes = 0;
    int addedNotes = 0;
};

[[nodiscard]] std::vector<CutAlignment> alignSliceCuts(std::vector<SlicePlan>& plans, const std::vector<TimingMap>& timings);

[[nodiscard]] MergedSlices mergeSlicePlans(const std::vector<SlicePlan>& plans, std::size_t baseIndex);

}  // namespace b2b::core
