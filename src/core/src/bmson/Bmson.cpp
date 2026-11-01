#include "b2b/core/bmson/Bmson.hpp"

namespace b2b::core::bmson {

TimingMap timingOf(const Chart& chart) {
    return TimingMap(chart.info.initBpm, chart.info.resolution, chart.bpmEvents, chart.stopEvents);
}

}  // namespace b2b::core::bmson
