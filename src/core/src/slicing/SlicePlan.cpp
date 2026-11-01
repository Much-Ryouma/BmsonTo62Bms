#include "b2b/core/slicing/SlicePlan.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace b2b::core {

namespace {

struct FirstUse {
    Pulse y = 0;
    int lane = 0;
    bool set = false;

    void offer(Pulse noteY, int noteLane) {
        if (!set || std::tie(noteY, noteLane) < std::tie(y, lane)) {
            y = noteY;
            lane = noteLane;
            set = true;
        }
    }
};

struct Draft {
    Slice slice;
    std::size_t channel = 0;
    FirstUse firstAny;
    FirstUse firstKey;
};

using SliceKey = std::tuple<std::string, std::int64_t, std::int64_t>;

std::int64_t frameAt(double seconds, int sampleRate) {
    return std::llround(seconds * sampleRate);
}

class Planner {
public:
    Planner(const TimingMap& timing, SlicePlan& plan, int keyLanes) : timing_(timing), plan_(plan), keyLanes_(keyLanes) {}

    void addChannel(std::size_t channelIndex, const bmson::SoundChannel& channel, const AudioInfo& audio) {
        if (audio.sampleRate <= 0 || audio.frames < 0) {
            throw std::invalid_argument("invalid audio info for sound channel: " + channel.name);
        }
        std::vector<bmson::Note> notes = channel.notes;
        std::stable_sort(notes.begin(), notes.end(), [](const bmson::Note& a, const bmson::Note& b) { return a.y < b.y; });
        std::vector<bool> outsideMode(notes.size(), false);
        for (std::size_t index = 0; index < notes.size(); ++index) {
            if (notes[index].x != 0 && (notes[index].x < 1 || notes[index].x > keyLanes_)) {
                notes[index].x = 0;
                notes[index].l = 0;
                outsideMode[index] = true;
            }
        }

        double anchor = 0.0;
        for (std::size_t first = 0; first < notes.size();) {
            std::size_t last = first;
            bool restart = false;
            bool hasLaneNote = false;
            while (last < notes.size() && notes[last].y == notes[first].y) {
                restart = restart || !notes[last].c;
                hasLaneNote = hasLaneNote || (notes[last].x != 0 && !notes[last].up);
                ++last;
            }

            const double at = timing_.secondsAt(notes[first].y);
            if (restart) {
                anchor = at;
            }
            const std::int64_t start = std::min(frameAt(at - anchor, audio.sampleRate), audio.frames);
            std::int64_t end = audio.frames;
            if (last < notes.size()) {
                end = frameAt(timing_.secondsAt(notes[last].y) - anchor, audio.sampleRate);
            }
            end = std::clamp(end, start, audio.frames);

            for (std::size_t index = first; index < last; ++index) {
                addNote(channelIndex, channel.name, audio.sampleRate, notes[index], start, end, hasLaneNote,
                        start >= audio.frames, outsideMode[index]);
            }
            first = last;
        }
    }

    void finish() {
        std::vector<std::size_t> order(drafts_.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        for (auto& draft : drafts_) {
            const FirstUse& use = draft.firstKey.set ? draft.firstKey : draft.firstAny;
            draft.slice.kind = draft.firstKey.set ? SliceKind::Key : SliceKind::Bgm;
            draft.slice.firstPulse = use.y;
            draft.slice.firstLane = use.lane;
        }
        std::stable_sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
            const Draft& left = drafts_[a];
            const Draft& right = drafts_[b];
            return std::make_tuple(left.slice.firstPulse, left.slice.kind != SliceKind::Key, left.slice.firstLane,
                                   left.channel, left.slice.startFrame) <
                   std::make_tuple(right.slice.firstPulse, right.slice.kind != SliceKind::Key, right.slice.firstLane,
                                   right.channel, right.slice.startFrame);
        });

        std::vector<std::size_t> newIndexOf(drafts_.size());
        int keyCount = 0;
        int bgmCount = 0;
        for (std::size_t position = 0; position < order.size(); ++position) {
            Slice slice = drafts_[order[position]].slice;
            slice.indexInKind = slice.kind == SliceKind::Key ? ++keyCount : ++bgmCount;
            newIndexOf[order[position]] = position;
            plan_.slices.push_back(std::move(slice));
        }
        for (auto& note : plan_.notes) {
            note.slice = newIndexOf[note.slice];
        }
        std::stable_sort(plan_.notes.begin(), plan_.notes.end(), [](const PlacedNote& a, const PlacedNote& b) {
            return std::tie(a.y, a.lane, a.slice) < std::tie(b.y, b.lane, b.slice);
        });
    }

private:
    void addNote(std::size_t channelIndex, const std::string& source, int sampleRate, const bmson::Note& note,
                 std::int64_t start, std::int64_t end, bool hasLaneNote, bool pastAudioEnd, bool outsideMode) {
        if (outsideMode) {
            ++plan_.stats.lanesOutsideMode;
        }
        if (note.x != 0 && note.up) {
            ++plan_.stats.ignoredLongNoteEndSounds;
            return;
        }
        // Why not キーモード外のレーンのノーツも取り消す: もとは鍵盤のノーツとして置かれた音なので、鳴らさないと音が抜けてしまう
        if (note.x == 0 && hasLaneNote && !outsideMode) {
            // Why not BGM も鳴らす: 同じ位置に鍵盤のノーツがあるときの BGM は鳴らさない（IgusaBmsonStudio のプレビューと同じ扱い）。
            // 鳴らすと同じスライスが二重に鳴り、bmson で聴いた音と変わってしまうため
            ++plan_.stats.canceledBgmNotes;
            return;
        }
        if (pastAudioEnd) {
            ++plan_.stats.notesPastAudioEnd;
        }

        const SliceKey key{source, start, end};
        auto found = indexOf_.find(key);
        if (found == indexOf_.end()) {
            Draft draft;
            draft.slice.source = source;
            draft.slice.startFrame = start;
            draft.slice.endFrame = end;
            draft.slice.sampleRate = sampleRate;
            draft.channel = channelIndex;
            found = indexOf_.emplace(key, drafts_.size()).first;
            drafts_.push_back(std::move(draft));
        }
        Draft& draft = drafts_[found->second];
        draft.firstAny.offer(note.y, note.x);
        if (note.x != 0) {
            draft.firstKey.offer(note.y, note.x);
        }
        plan_.notes.push_back(PlacedNote{found->second, note.x, note.y, note.x != 0 ? std::max<Pulse>(0, note.l) : 0});
    }

    const TimingMap& timing_;
    SlicePlan& plan_;
    int keyLanes_;
    std::vector<Draft> drafts_;
    std::map<SliceKey, std::size_t> indexOf_;
};

}  // namespace

double Slice::startSeconds() const noexcept {
    return sampleRate > 0 ? static_cast<double>(startFrame) / sampleRate : 0.0;
}

double Slice::lengthSeconds() const noexcept {
    return sampleRate > 0 ? static_cast<double>(frameCount()) / sampleRate : 0.0;
}

int SlicePlan::countOf(SliceKind kind) const {
    return static_cast<int>(std::count_if(slices.begin(), slices.end(), [kind](const Slice& s) { return s.kind == kind; }));
}

SlicePlan planSlices(const bmson::Chart& chart, const TimingMap& timing, const std::vector<AudioInfo>& channelAudio,
                     int keyLanes) {
    if (channelAudio.size() != chart.soundChannels.size()) {
        throw std::invalid_argument("audio info is required for every sound channel");
    }
    SlicePlan plan;
    Planner planner(timing, plan, keyLanes);
    for (std::size_t index = 0; index < chart.soundChannels.size(); ++index) {
        planner.addChannel(index, chart.soundChannels[index], channelAudio[index]);
    }
    planner.finish();
    return plan;
}

}  // namespace b2b::core
