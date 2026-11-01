#include "b2b/core/timing/TimingMap.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

namespace b2b::core {

namespace {

constexpr double kSecondsPerMinute = 60.0;

void requirePositiveBpm(double bpm, const char* source) {
    if (!(bpm > 0.0)) {
        throw std::invalid_argument(std::string(source) + ": bpm must be positive, got " + std::to_string(bpm));
    }
}

void requireNonNegativePulse(Pulse pulse, const char* source) {
    if (pulse < 0) {
        throw std::invalid_argument(std::string(source) + ": pulse must not be negative, got " + std::to_string(pulse));
    }
}

}  // namespace

TimingMap::TimingMap(double initialBpm, int resolution,
                     std::vector<BpmEvent> bpmEvents,
                     std::vector<StopEvent> stopEvents)
    : initialBpm_(initialBpm), resolution_(resolution) {
    requirePositiveBpm(initialBpm, "init_bpm");
    if (resolution <= 0) {
        throw std::invalid_argument("resolution must be positive, got " + std::to_string(resolution));
    }
    for (const auto& event : bpmEvents) {
        requirePositiveBpm(event.bpm, "bpm_events");
        requireNonNegativePulse(event.y, "bpm_events");
    }
    for (const auto& event : stopEvents) {
        requireNonNegativePulse(event.y, "stop_events");
        requireNonNegativePulse(event.durationPulses, "stop_events.duration");
    }

    std::stable_sort(bpmEvents.begin(), bpmEvents.end(),
                     [](const BpmEvent& a, const BpmEvent& b) { return a.y < b.y; });
    std::stable_sort(stopEvents.begin(), stopEvents.end(),
                     [](const StopEvent& a, const StopEvent& b) { return a.y < b.y; });

    std::vector<Pulse> eventPulses;
    eventPulses.reserve(bpmEvents.size() + stopEvents.size() + 1);
    eventPulses.push_back(0);
    for (const auto& event : bpmEvents) { eventPulses.push_back(event.y); }
    for (const auto& event : stopEvents) { eventPulses.push_back(event.y); }
    std::sort(eventPulses.begin(), eventPulses.end());
    eventPulses.erase(std::unique(eventPulses.begin(), eventPulses.end()), eventPulses.end());

    auto nextBpm = bpmEvents.cbegin();
    auto nextStop = stopEvents.cbegin();
    double currentBpm = initialBpm;
    double secondsAtPreviousResume = 0.0;
    Pulse previousPulse = 0;

    for (const Pulse pulse : eventPulses) {
        const double reachSeconds =
            secondsAtPreviousResume + static_cast<double>(pulse - previousPulse) * secondsPerPulseFor(currentBpm);

        for (; nextBpm != bpmEvents.cend() && nextBpm->y == pulse; ++nextBpm) {
            currentBpm = nextBpm->bpm;
        }

        Pulse stoppedPulses = 0;
        for (; nextStop != stopEvents.cend() && nextStop->y == pulse; ++nextStop) {
            stoppedPulses += nextStop->durationPulses;
        }

        const double spp = secondsPerPulseFor(currentBpm);
        const double resumeSeconds = reachSeconds + static_cast<double>(stoppedPulses) * spp;
        segments_.push_back(Segment{pulse, reachSeconds, resumeSeconds, spp, currentBpm});

        secondsAtPreviousResume = resumeSeconds;
        previousPulse = pulse;
    }
}

double TimingMap::secondsPerPulseFor(double bpm) const {
    return kSecondsPerMinute / bpm / static_cast<double>(resolution_);
}

const TimingMap::Segment& TimingMap::segmentContaining(Pulse y) const {
    const auto firstAfter = std::upper_bound(
        segments_.cbegin(), segments_.cend(), y,
        [](Pulse value, const Segment& segment) { return value < segment.startPulse; });
    if (firstAfter == segments_.cbegin()) {
        return segments_.front();
    }
    return *std::prev(firstAfter);
}

double TimingMap::secondsAt(Pulse y) const {
    const Segment& segment = segmentContaining(y);
    if (y == segment.startPulse) {
        // Why not resumeSeconds を返す: bmson 仕様では STOP と同じ位置のノーツは「停止の開始時」に判定されるため
        return segment.reachSeconds;
    }
    return segment.resumeSeconds + static_cast<double>(y - segment.startPulse) * segment.secondsPerPulse;
}

double TimingMap::pulseAt(double seconds) const {
    const auto firstAfter = std::upper_bound(
        segments_.cbegin(), segments_.cend(), seconds,
        [](double value, const Segment& segment) { return value < segment.reachSeconds; });
    const Segment& segment = firstAfter == segments_.cbegin() ? segments_.front() : *std::prev(firstAfter);
    if (seconds <= segment.resumeSeconds) {
        // Why not 停止中の経過時間を比例配分する: 停止中は譜面が進まないので、その間の時刻はすべて停止位置に対応する
        return static_cast<double>(segment.startPulse) +
               std::min(0.0, (seconds - segment.reachSeconds) / segment.secondsPerPulse);
    }
    return static_cast<double>(segment.startPulse) + (seconds - segment.resumeSeconds) / segment.secondsPerPulse;
}

double TimingMap::bpmAt(Pulse y) const {
    return segmentContaining(y).bpm;
}

}  // namespace b2b::core
