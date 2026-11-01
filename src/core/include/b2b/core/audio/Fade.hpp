#pragma once

#include <optional>
#include <string>

#include "b2b/core/audio/Pcm.hpp"

namespace b2b::core {

enum class EdgeMode { Cut, Fade };

struct FadeSettings {
    EdgeMode mode = EdgeMode::Cut;
    double fadeOutMs = 2.0;
    bool fadeIn = false;
    double fadeInMs = 2.0;

    friend bool operator==(const FadeSettings&, const FadeSettings&) = default;
};

inline constexpr double kMaxFadeMs = 50.0;

[[nodiscard]] std::optional<std::string> fadeSettingsError(const FadeSettings& settings);

void applyFade(PcmBuffer& buffer, const FadeSettings& settings, bool startsAtFileStart);

}  // namespace b2b::core
