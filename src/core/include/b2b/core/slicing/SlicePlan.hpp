#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "b2b/core/bmson/Bmson.hpp"

namespace b2b::core {

struct AudioInfo {
    int sampleRate = 0;
    std::int64_t frames = 0;

    friend bool operator==(const AudioInfo&, const AudioInfo&) = default;
};

enum class SliceKind { Key, Bgm };

struct Slice {
    std::string source;
    std::int64_t startFrame = 0;
    std::int64_t endFrame = 0;
    SliceKind kind = SliceKind::Bgm;
    Pulse firstPulse = 0;
    int firstLane = 0;
    int indexInKind = 0;
    int sampleRate = 0;

    [[nodiscard]] std::int64_t frameCount() const noexcept { return endFrame - startFrame; }
    [[nodiscard]] double startSeconds() const noexcept;
    [[nodiscard]] double lengthSeconds() const noexcept;
};

struct PlacedNote {
    std::size_t slice = 0;
    int lane = 0;
    Pulse y = 0;
    Pulse length = 0;

    friend bool operator==(const PlacedNote&, const PlacedNote&) = default;
};

struct SlicePlanStats {
    int canceledBgmNotes = 0;
    int ignoredLongNoteEndSounds = 0;
    int notesPastAudioEnd = 0;
    int lanesOutsideMode = 0;
};

struct SlicePlan {
    std::vector<Slice> slices;
    std::vector<PlacedNote> notes;
    SlicePlanStats stats;

    [[nodiscard]] int countOf(SliceKind kind) const;
};

inline constexpr int kBeat7kLanes = 8;

[[nodiscard]] SlicePlan planSlices(const bmson::Chart& chart, const TimingMap& timing,
                                   const std::vector<AudioInfo>& channelAudio, int keyLanes = kBeat7kLanes);

}  // namespace b2b::core
