#pragma once

#include <optional>
#include <string>
#include <vector>

#include "b2b/core/bms/ObjectId.hpp"
#include "b2b/core/bmson/Bmson.hpp"
#include "b2b/core/slicing/SlicePlan.hpp"

namespace b2b::core {

enum class BmsLineKind { Comment, Header, Path, Data, Blank };

struct BmsLine {
    BmsLineKind kind = BmsLineKind::Blank;
    std::string text;

    friend bool operator==(const BmsLine&, const BmsLine&) = default;
};

struct IdUsage {
    int wav = 0;
    int bmp = 0;
    int bpm = 0;
    int stop = 0;

    [[nodiscard]] int largest() const noexcept;
};

struct BmsDocument {
    IdBase base = IdBase::Base36;
    IdUsage usage;
    std::vector<BmsLine> lines;
};

struct BmsWriteOptions {
    std::optional<int> difficulty;
};

struct BmsWriteResult {
    std::optional<BmsDocument> document;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

inline constexpr int kMaxMeasure = 999;

[[nodiscard]] bool isSupportedMode(const std::string& modeHint);

[[nodiscard]] std::string channelForLane(int lane, bool longNote);

[[nodiscard]] BmsWriteResult writeBms(const bmson::Chart& chart, const SlicePlan& plan,
                                      const std::vector<std::string>& soundFiles, const BmsWriteOptions& options);

}  // namespace b2b::core
