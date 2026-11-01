#pragma once

#include <cstdint>
#include <vector>

namespace b2b::core {

using Pulse = std::int64_t;

struct BpmEvent {
    Pulse y = 0;
    double bpm = 0.0;

    friend bool operator==(const BpmEvent&, const BpmEvent&) = default;
};

struct StopEvent {
    Pulse y = 0;
    Pulse durationPulses = 0;

    friend bool operator==(const StopEvent&, const StopEvent&) = default;
};

class TimingMap {
public:
    TimingMap(double initialBpm, int resolution,
              std::vector<BpmEvent> bpmEvents = {},
              std::vector<StopEvent> stopEvents = {});

    [[nodiscard]] int resolution() const noexcept { return resolution_; }
    [[nodiscard]] double initialBpm() const noexcept { return initialBpm_; }

    [[nodiscard]] double secondsAt(Pulse y) const;
    [[nodiscard]] double pulseAt(double seconds) const;
    [[nodiscard]] double bpmAt(Pulse y) const;

private:
    struct Segment {
        Pulse startPulse = 0;
        double reachSeconds = 0.0;
        double resumeSeconds = 0.0;
        double secondsPerPulse = 0.0;
        double bpm = 0.0;
    };

    [[nodiscard]] const Segment& segmentContaining(Pulse y) const;
    [[nodiscard]] double secondsPerPulseFor(double bpm) const;

    double initialBpm_;
    int resolution_;
    std::vector<Segment> segments_;
};

}  // namespace b2b::core
