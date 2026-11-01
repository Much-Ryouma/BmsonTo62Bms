#include "b2b/core/slicing/SliceMerge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace b2b::core {

namespace {

using SliceKey = std::tuple<std::string, std::int64_t, std::int64_t>;

SliceKey keyOf(const Slice& slice) {
    return {slice.source, slice.startFrame, slice.endFrame};
}

struct CutPoint {
    std::int64_t frame = 0;
    Pulse y = 0;
    std::size_t chart = 0;
};

struct Use {
    Slice slice;
    PlacedNote note;
};

// 丸めの差（1 フレーム）までは、同じ位置で切ったものとみなす
constexpr std::int64_t kCutTolerance = 1;

std::map<std::string, std::vector<CutPoint>> cutPointsOf(const std::vector<SlicePlan>& plans) {
    std::map<std::string, std::vector<CutPoint>> cuts;
    for (std::size_t chart = 0; chart < plans.size(); ++chart) {
        for (const PlacedNote& note : plans[chart].notes) {
            const Slice& slice = plans[chart].slices[note.slice];
            if (slice.frameCount() > 0) {
                cuts[slice.source].push_back(CutPoint{slice.startFrame, note.y, chart});
            }
        }
    }
    return cuts;
}

SlicePlan rebuildPlan(const std::vector<Use>& uses, const SlicePlanStats& stats) {
    struct Draft {
        Slice slice;
        bool key = false;
        std::tuple<Pulse, int> firstAny{0, 0};
        std::tuple<Pulse, int> firstKey{0, 0};
        bool anySet = false;
    };
    std::vector<Draft> drafts;
    std::map<SliceKey, std::size_t> indexOf;
    SlicePlan plan;
    plan.stats = stats;
    for (const Use& use : uses) {
        auto found = indexOf.find(keyOf(use.slice));
        if (found == indexOf.end()) {
            found = indexOf.emplace(keyOf(use.slice), drafts.size()).first;
            drafts.push_back(Draft{use.slice});
        }
        Draft& draft = drafts[found->second];
        const std::tuple<Pulse, int> at{use.note.y, use.note.lane};
        if (!draft.anySet || at < draft.firstAny) {
            draft.firstAny = at;
            draft.anySet = true;
        }
        if (use.note.lane != 0 && (!draft.key || at < draft.firstKey)) {
            draft.firstKey = at;
            draft.key = true;
        }
        PlacedNote note = use.note;
        note.slice = found->second;
        plan.notes.push_back(note);
    }
    for (Draft& draft : drafts) {
        const auto& first = draft.key ? draft.firstKey : draft.firstAny;
        draft.slice.kind = draft.key ? SliceKind::Key : SliceKind::Bgm;
        draft.slice.firstPulse = std::get<0>(first);
        draft.slice.firstLane = std::get<1>(first);
    }
    std::vector<std::size_t> order(drafts.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const Slice& left = drafts[a].slice;
        const Slice& right = drafts[b].slice;
        return std::make_tuple(left.firstPulse, left.kind != SliceKind::Key, left.firstLane, left.source, left.startFrame) <
               std::make_tuple(right.firstPulse, right.kind != SliceKind::Key, right.firstLane, right.source, right.startFrame);
    });
    std::vector<std::size_t> newIndexOf(drafts.size());
    int keyCount = 0;
    int bgmCount = 0;
    for (std::size_t position = 0; position < order.size(); ++position) {
        Slice slice = drafts[order[position]].slice;
        slice.indexInKind = slice.kind == SliceKind::Key ? ++keyCount : ++bgmCount;
        newIndexOf[order[position]] = position;
        plan.slices.push_back(std::move(slice));
    }
    for (auto& note : plan.notes) {
        note.slice = newIndexOf[note.slice];
    }
    std::stable_sort(plan.notes.begin(), plan.notes.end(), [](const PlacedNote& a, const PlacedNote& b) {
        return std::tie(a.y, a.lane, a.slice) < std::tie(b.y, b.lane, b.slice);
    });
    return plan;
}

}  // namespace

std::vector<CutAlignment> alignSliceCuts(std::vector<SlicePlan>& plans, const std::vector<TimingMap>& timings) {
    if (timings.size() != plans.size()) {
        throw std::invalid_argument("a timing map is required for every chart");
    }
    const auto cuts = cutPointsOf(plans);
    std::vector<CutAlignment> alignments(plans.size());
    for (std::size_t chart = 0; chart < plans.size(); ++chart) {
        const SlicePlan& plan = plans[chart];
        const TimingMap& timing = timings[chart];
        std::vector<Use> uses;
        std::set<std::tuple<std::string, std::int64_t, std::int64_t, Pulse>> added;
        CutAlignment& alignment = alignments[chart];
        for (const PlacedNote& note : plan.notes) {
            const Slice& slice = plan.slices[note.slice];
            std::map<std::int64_t, Pulse> inside;
            const auto found = cuts.find(slice.source);
            if (found != cuts.end() && slice.frameCount() > 0) {
                const double noteSeconds = timing.secondsAt(note.y);
                for (const CutPoint& cut : found->second) {
                    if (cut.chart == chart || cut.y <= note.y || cut.frame <= slice.startFrame || cut.frame >= slice.endFrame) {
                        continue;
                    }
                    // Why not 区間の中にある切断位置をすべて使う: c=false でやり直した後の切断位置は、同じフレーム番号でも
                    // 音声の別の時刻を指す。このノーツの位置から数えて、同じ時刻に同じフレームを鳴らすものだけを使う
                    const std::int64_t frameHere =
                        slice.startFrame + std::llround((timing.secondsAt(cut.y) - noteSeconds) * slice.sampleRate);
                    if (std::llabs(frameHere - cut.frame) <= kCutTolerance) {
                        inside.emplace(cut.frame, cut.y);
                    }
                }
            }
            if (inside.empty()) {
                uses.push_back(Use{slice, note});
                continue;
            }
            ++alignment.shortenedNotes;
            std::int64_t start = slice.startFrame;
            Pulse y = note.y;
            bool first = true;
            for (auto cut = inside.begin();; ++cut) {
                const std::int64_t end = cut == inside.end() ? slice.endFrame : cut->first;
                Slice piece = slice;
                piece.startFrame = start;
                piece.endFrame = end;
                if (first) {
                    uses.push_back(Use{piece, note});
                    first = false;
                } else if (added.emplace(piece.source, start, end, y).second) {
                    // Why not ノーツと同じレーンに置く: 続きは元のノーツの音の残りなので、鍵盤を叩かなくても鳴るよう BGM にする
                    uses.push_back(Use{piece, PlacedNote{0, 0, y, 0}});
                    ++alignment.addedNotes;
                }
                if (cut == inside.end()) {
                    break;
                }
                start = cut->first;
                y = cut->second;
            }
        }
        if (alignment.shortenedNotes > 0) {
            plans[chart] = rebuildPlan(uses, plan.stats);
        }
    }
    return alignments;
}

SlicePlan MergedSlices::planFor(const SlicePlan& plan, std::size_t chart) const {
    SlicePlan merged;
    merged.slices = slices;
    merged.stats = plan.stats;
    merged.notes = plan.notes;
    for (auto& note : merged.notes) {
        note.slice = globalIndexOf.at(chart).at(note.slice);
    }
    return merged;
}

MergedSlices mergeSlicePlans(const std::vector<SlicePlan>& plans, std::size_t baseIndex) {
    if (baseIndex >= plans.size()) {
        throw std::invalid_argument("base chart index out of range");
    }
    MergedSlices merged;
    merged.globalIndexOf.resize(plans.size());
    merged.outsideBase.resize(plans.size());
    std::map<SliceKey, std::size_t> indexOf;

    auto addChart = [&](std::size_t chart) {
        for (const Slice& slice : plans[chart].slices) {
            auto found = indexOf.find(keyOf(slice));
            if (found == indexOf.end()) {
                found = indexOf.emplace(keyOf(slice), merged.slices.size()).first;
                merged.slices.push_back(slice);
            } else if (slice.kind == SliceKind::Key && merged.slices[found->second].kind == SliceKind::Bgm) {
                // Why not 最初に使った譜面のままにする: どれか 1 つの譜面で鍵盤に置かれていればパラ音として名前を付けるため、
                // パラ音として最初に使った位置とレーンに合わせる
                Slice& shared = merged.slices[found->second];
                shared.kind = SliceKind::Key;
                shared.firstPulse = slice.firstPulse;
                shared.firstLane = slice.firstLane;
            }
            merged.globalIndexOf[chart].push_back(found->second);
        }
    };
    addChart(baseIndex);
    const std::size_t baseCount = merged.slices.size();
    for (std::size_t chart = 0; chart < plans.size(); ++chart) {
        if (chart != baseIndex) {
            addChart(chart);
        }
    }

    for (std::size_t chart = 0; chart < plans.size(); ++chart) {
        std::set<std::size_t> outside;
        for (const std::size_t global : merged.globalIndexOf[chart]) {
            if (global >= baseCount) {
                outside.insert(global);
            }
        }
        merged.outsideBase[chart].assign(outside.begin(), outside.end());
    }

    int keyCount = 0;
    int bgmCount = 0;
    for (Slice& slice : merged.slices) {
        slice.indexInKind = slice.kind == SliceKind::Key ? ++keyCount : ++bgmCount;
    }
    return merged;
}

}  // namespace b2b::core
