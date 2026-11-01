#include "b2b/core/bms/BmsWriter.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>

#include "b2b/core/bms/HeaderValues.hpp"
#include "b2b/core/timing/MeasureLayout.hpp"

namespace b2b::core {

namespace {

constexpr int kStopUnitsPerBeat = 48;

struct Event {
    int measure = 0;
    std::string channel;
    Pulse offset = 0;
    int id = 0;
};

template <typename Value>
class IdTable {
public:
    int idOf(const Value& value) {
        const auto found = ids_.find(value);
        if (found != ids_.end()) {
            return found->second;
        }
        const int id = static_cast<int>(order_.size()) + 1;
        ids_.emplace(value, id);
        order_.push_back(value);
        return id;
    }
    [[nodiscard]] const std::vector<Value>& values() const noexcept { return order_; }
    [[nodiscard]] int size() const noexcept { return static_cast<int>(order_.size()); }

private:
    std::map<Value, int> ids_;
    std::vector<Value> order_;
};

std::string measureLabel(int measure) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%03d", measure);
    return buffer;
}

class Writer {
public:
    Writer(const bmson::Chart& chart, const SlicePlan& plan, const std::vector<std::string>& soundFiles,
           const BmsWriteOptions& options)
        : chart_(chart),
          plan_(plan),
          soundFiles_(soundFiles),
          options_(options),
          layout_(chart.info.resolution, chart.lines) {}

    BmsWriteResult run() {
        checkMode();
        collectBpm();
        collectStops();
        collectBga();
        decideBase();
        if (!result_.errors.empty()) {
            return std::move(result_);
        }
        collectNotes();
        if (!result_.errors.empty()) {
            return std::move(result_);
        }
        BmsDocument document;
        document.base = base_;
        document.usage = usage_;
        writeHeader(document.lines);
        writeDefinitions(document.lines);
        writeData(document.lines);
        if (!result_.errors.empty()) {
            return std::move(result_);
        }
        result_.document = std::move(document);
        return std::move(result_);
    }

private:
    void checkMode() {
        mode_ = findKeyMode(chart_.info.modeHint);
        if (mode_ == nullptr) {
            result_.errors.push_back("未対応のキーモードです: " + chart_.info.modeHint + "（対応しているのは " +
                                     supportedModeNames() + "）");
            return;
        }
        for (const auto& note : plan_.notes) {
            if (note.lane < 0 || note.lane > mode_->lanes) {
                result_.errors.push_back(mode_->name + " にないレーンのノーツがあります: x = " + std::to_string(note.lane));
                return;
            }
        }
    }

    void collectBpm() {
        std::vector<BpmEvent> events = chart_.bpmEvents;
        std::stable_sort(events.begin(), events.end(), [](const BpmEvent& a, const BpmEvent& b) { return a.y < b.y; });
        for (const auto& event : events) {
            bpmEvents_.push_back({event.y, bpmIds_.idOf(event.bpm)});
        }
    }

    void collectStops() {
        std::map<Pulse, Pulse> stoppedAt;
        for (const auto& event : chart_.stopEvents) {
            stoppedAt[event.y] += event.durationPulses;
        }
        for (const auto& [y, duration] : stoppedAt) {
            if (duration <= 0) {
                continue;
            }
            // Why not 整数に丸める: 丸めると停止時間がずれて、その後のノーツがすべて bmson とずれるため、小数のまま書く
            const double value = static_cast<double>(duration) * kStopUnitsPerBeat / chart_.info.resolution;
            stopEvents_.push_back({y, stopIds_.idOf(formatDecimal(value))});
        }
    }

    void collectBga() {
        std::map<std::int64_t, const bmson::BgaHeader*> headers;
        for (const auto& header : chart_.bga.headers) {
            headers.emplace(header.id, &header);
        }
        struct Use {
            Pulse y;
            std::int64_t id;
            const char* channel;
        };
        std::vector<Use> uses;
        for (const auto& event : chart_.bga.bgaEvents) { uses.push_back({event.y, event.id, "04"}); }
        for (const auto& event : chart_.bga.layerEvents) { uses.push_back({event.y, event.id, "07"}); }
        for (const auto& event : chart_.bga.poorEvents) { uses.push_back({event.y, event.id, "06"}); }
        std::stable_sort(uses.begin(), uses.end(), [](const Use& a, const Use& b) { return a.y < b.y; });

        for (const auto& use : uses) {
            if (!headers.contains(use.id)) {
                result_.errors.push_back("BGA の id " + std::to_string(use.id) + " が bga_header にありません");
                continue;
            }
            bgaEvents_.push_back({use.y, bmpIds_.idOf(use.id), use.channel});
        }
        for (const auto& header : chart_.bga.headers) {
            const bool used = std::any_of(uses.begin(), uses.end(), [&](const Use& use) { return use.id == header.id; });
            if (!used) {
                result_.warnings.push_back("使われていない BGA を定義だけ書き出します: " + header.name);
            }
            static_cast<void>(bmpIds_.idOf(header.id));
        }
        for (const auto id : bmpIds_.values()) {
            bmpNames_.push_back(headers.at(id)->name);
        }
    }

    void decideBase() {
        for (const auto& note : plan_.notes) {
            usedSlices_.insert(note.slice);
        }
        const int largestWav = usedSlices_.empty() ? 0 : static_cast<int>(*usedSlices_.rbegin()) + 1;
        usage_ = IdUsage{largestWav, static_cast<int>(usedSlices_.size()), bmpIds_.size(), bpmIds_.size(), stopIds_.size()};
        base_ = usage_.largest() > maxObjectId(IdBase::Base36) ? IdBase::Base62 : IdBase::Base36;
        const std::array<std::pair<const char*, int>, 4> counts = {
            std::pair{"音声 (#WAV) の ID", usage_.wav}, std::pair{"画像 (#BMP)", usage_.bmp},
            std::pair{"BPM (#BPM)", usage_.bpm}, std::pair{"停止 (#STOP)", usage_.stop}};
        for (const auto& [label, count] : counts) {
            if (count > maxObjectId(IdBase::Base62)) {
                result_.errors.push_back(std::string(label) + " の定義が " + std::to_string(count) + " 個あり、上限の " +
                                         std::to_string(maxObjectId(IdBase::Base62)) + " 個を超えています");
            }
        }
        if (base_ == IdBase::Base62 && result_.errors.empty()) {
            result_.warnings.push_back("定義が 1295 個を超えたため、62進数 (#BASE 62) で書き出します。62進数に対応していないプレイヤーでは正しく鳴りません");
        }
    }

    void addEvent(Pulse y, const std::string& channel, int id) {
        const MeasurePosition position = layout_.locate(y);
        events_.push_back({position.measure, channel, position.offset, id});
    }

    void collectNotes() {
        for (const auto& note : plan_.notes) {
            const int id = static_cast<int>(note.slice) + 1;
            if (note.lane == 0) {
                addEvent(note.y, "01", id);
            } else if (note.length > 0) {
                const std::string channel = channelForLane(*mode_, note.lane, true);
                addEvent(note.y, channel, id);
                addEvent(note.y + note.length, channel, id);
            } else {
                addEvent(note.y, channelForLane(*mode_, note.lane, false), id);
            }
        }
        for (const auto& event : bpmEvents_) { addEvent(event.y, "08", event.id); }
        for (const auto& event : stopEvents_) { addEvent(event.y, "09", event.id); }
        for (const auto& event : bgaEvents_) { addEvent(event.y, event.channel, event.id); }

        for (const auto& event : events_) {
            if (event.measure > kMaxMeasure) {
                result_.errors.push_back("小節番号が " + std::to_string(kMaxMeasure) + " を超えます（" +
                                         std::to_string(event.measure) + " 小節目）");
                return;
            }
        }
    }

    [[nodiscard]] std::string id(int value) const { return formatObjectId(value, base_); }

    void writeHeader(std::vector<BmsLine>& lines) const {
        const auto& info = chart_.info;
        lines.push_back({BmsLineKind::Comment, "*---------------------- HEADER FIELD"});
        lines.push_back({BmsLineKind::Blank, ""});
        if (base_ == IdBase::Base62) {
            lines.push_back({BmsLineKind::Header, "#BASE 62"});
        }
        lines.push_back({BmsLineKind::Header, "#PLAYER " + std::to_string(mode_->player)});
        lines.push_back({BmsLineKind::Header, "#GENRE " + info.genre});
        lines.push_back({BmsLineKind::Header, "#TITLE " + info.title});
        if (!info.subtitle.empty()) {
            lines.push_back({BmsLineKind::Header, "#SUBTITLE " + info.subtitle});
        }
        lines.push_back({BmsLineKind::Header, "#ARTIST " + info.artist});
        for (const auto& subartist : info.subartists) {
            lines.push_back({BmsLineKind::Header, "#SUBARTIST " + subartist});
        }
        lines.push_back({BmsLineKind::Header, "#BPM " + formatDecimal(info.initBpm)});
        lines.push_back({BmsLineKind::Header, "#PLAYLEVEL " + std::to_string(info.level)});
        if (options_.difficulty) {
            lines.push_back({BmsLineKind::Header, "#DIFFICULTY " + std::to_string(*options_.difficulty)});
        }
        const JudgeHeader judge = judgeHeaderFor(info.judgeRank, mode_->normalJudgePercent);
        if (judge.kind == JudgeHeaderKind::Rank) {
            lines.push_back({BmsLineKind::Header, "#RANK " + std::to_string(judge.value)});
        } else if (judge.kind == JudgeHeaderKind::DefExRank) {
            lines.push_back({BmsLineKind::Header, "#DEFEXRANK " + std::to_string(judge.value)});
        }
        if (const auto total = bmsTotalFor(info.total, keyNoteCount())) {
            lines.push_back({BmsLineKind::Header, "#TOTAL " + formatDecimal(*total)});
        }
        const std::array<std::pair<const char*, const std::string*>, 4> images = {
            std::pair{"#STAGEFILE ", &info.eyecatchImage}, std::pair{"#BANNER ", &info.bannerImage},
            std::pair{"#BACKBMP ", &info.backImage}, std::pair{"#PREVIEW ", &info.previewMusic}};
        for (const auto& [command, value] : images) {
            if (!value->empty()) {
                lines.push_back({BmsLineKind::Path, command + *value});
            }
        }
        lines.push_back({BmsLineKind::Header, "#LNTYPE 1"});
        lines.push_back({BmsLineKind::Blank, ""});
    }

    [[nodiscard]] int keyNoteCount() const {
        return static_cast<int>(
            std::count_if(plan_.notes.begin(), plan_.notes.end(), [](const PlacedNote& note) { return note.lane != 0; }));
    }

    void writeDefinitions(std::vector<BmsLine>& lines) const {
        // Why not すべてのスライスを定義する: 譜面セットでは全難易度のスライスを 1 つの ID 空間で共有するため、
        // 使わない定義まで書くとプレイヤーが不要な音を読み込み、下位難易度まで 62進数になってしまう
        for (const std::size_t index : usedSlices_) {
            lines.push_back({BmsLineKind::Path, "#WAV" + id(static_cast<int>(index) + 1) + " " + soundFiles_[index]});
        }
        if (!bmpNames_.empty()) {
            lines.push_back({BmsLineKind::Blank, ""});
        }
        for (std::size_t index = 0; index < bmpNames_.size(); ++index) {
            lines.push_back({BmsLineKind::Path, "#BMP" + id(static_cast<int>(index) + 1) + " " + bmpNames_[index]});
        }
        if (bpmIds_.size() > 0 || stopIds_.size() > 0) {
            lines.push_back({BmsLineKind::Blank, ""});
        }
        for (std::size_t index = 0; index < bpmIds_.values().size(); ++index) {
            lines.push_back({BmsLineKind::Header,
                             "#BPM" + id(static_cast<int>(index) + 1) + " " + formatDecimal(bpmIds_.values()[index])});
        }
        for (std::size_t index = 0; index < stopIds_.values().size(); ++index) {
            lines.push_back(
                {BmsLineKind::Header, "#STOP" + id(static_cast<int>(index) + 1) + " " + stopIds_.values()[index]});
        }
        lines.push_back({BmsLineKind::Blank, ""});
    }

    void writeData(std::vector<BmsLine>& lines) {
        lines.push_back({BmsLineKind::Comment, "*---------------------- MAIN DATA FIELD"});
        lines.push_back({BmsLineKind::Blank, ""});

        int lastMeasure = 0;
        for (const auto& event : events_) {
            lastMeasure = std::max(lastMeasure, event.measure);
        }
        std::map<std::tuple<int, std::string>, std::vector<const Event*>> groups;
        for (const auto& event : events_) {
            groups[{event.measure, event.channel}].push_back(&event);
        }

        for (int measure = 0; measure <= lastMeasure; ++measure) {
            const Pulse length = layout_.lengthOf(measure);
            if (length != layout_.standardLength()) {
                lines.push_back({BmsLineKind::Data, "#" + measureLabel(measure) + "02:" +
                                                        formatDecimal(static_cast<double>(length) /
                                                                      static_cast<double>(layout_.standardLength()))});
            }
            for (auto group = groups.lower_bound({measure, std::string()});
                 group != groups.end() && std::get<0>(group->first) == measure; ++group) {
                writeGroup(lines, measure, std::get<1>(group->first), group->second, length);
            }
        }
    }

    void writeGroup(std::vector<BmsLine>& lines, int measure, const std::string& channel,
                    const std::vector<const Event*>& events, Pulse length) {
        std::vector<std::vector<const Event*>> layers;
        for (const Event* event : events) {
            auto layer = std::find_if(layers.begin(), layers.end(), [&](const std::vector<const Event*>& existing) {
                return std::none_of(existing.begin(), existing.end(),
                                    [&](const Event* other) { return other->offset == event->offset; });
            });
            if (layer == layers.end()) {
                if (!layers.empty() && channel != "01") {
                    resolveCollision(measure, channel, event, layers.front());
                    continue;
                }
                layers.emplace_back();
                layer = std::prev(layers.end());
            }
            layer->push_back(event);
        }
        for (const auto& layer : layers) {
            lines.push_back({BmsLineKind::Data, "#" + measureLabel(measure) + channel + ":" + lineBody(layer, length)});
        }
    }

    void resolveCollision(int measure, const std::string& channel, const Event* event, std::vector<const Event*>& layer) {
        const std::string where = std::to_string(measure) + " 小節目のチャンネル " + channel;
        if (channel.front() == '5' || channel.front() == '6') {
            result_.errors.push_back(where + " で、ロングノーツの終点と次のノーツの始点が重なっています");
            return;
        }
        if (channel.front() == '1' || channel.front() == '2') {
            result_.warnings.push_back(where + " で同じ位置にノーツが重なっているため、後のノーツを書き出しません");
            return;
        }
        auto same = std::find_if(layer.begin(), layer.end(), [&](const Event* other) { return other->offset == event->offset; });
        *same = event;
    }

    [[nodiscard]] std::string lineBody(const std::vector<const Event*>& layer, Pulse length) const {
        Pulse step = length;
        for (const Event* event : layer) {
            step = std::gcd(step, event->offset);
        }
        const auto slots = static_cast<std::size_t>(length / step);
        std::string body(slots * 2, '0');
        for (const Event* event : layer) {
            const auto slot = static_cast<std::size_t>(event->offset / step);
            body.replace(slot * 2, 2, id(event->id));
        }
        return body;
    }

    struct BpmUse {
        Pulse y;
        int id;
    };
    struct BgaUse {
        Pulse y;
        int id;
        const char* channel;
    };

    const bmson::Chart& chart_;
    const SlicePlan& plan_;
    const std::vector<std::string>& soundFiles_;
    const BmsWriteOptions& options_;
    MeasureLayout layout_;
    BmsWriteResult result_;
    const KeyMode* mode_ = nullptr;
    std::set<std::size_t> usedSlices_;
    IdBase base_ = IdBase::Base36;
    IdUsage usage_;
    IdTable<double> bpmIds_;
    IdTable<std::string> stopIds_;
    IdTable<std::int64_t> bmpIds_;
    std::vector<std::string> bmpNames_;
    std::vector<BpmUse> bpmEvents_;
    std::vector<BpmUse> stopEvents_;
    std::vector<BgaUse> bgaEvents_;
    std::vector<Event> events_;
};

}  // namespace

int IdUsage::largest() const noexcept {
    return std::max({wav, bmp, bpm, stop});
}

bool isSupportedMode(const std::string& modeHint) {
    return findKeyMode(modeHint) != nullptr;
}

BmsWriteResult writeBms(const bmson::Chart& chart, const SlicePlan& plan, const std::vector<std::string>& soundFiles,
                        const BmsWriteOptions& options) {
    if (soundFiles.size() != plan.slices.size()) {
        throw std::invalid_argument("a sound file name is required for every slice");
    }
    return Writer(chart, plan, soundFiles, options).run();
}

}  // namespace b2b::core
