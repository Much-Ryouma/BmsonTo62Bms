#include <gtest/gtest.h>

#include <algorithm>

#include "ChartBuilder.hpp"
#include "b2b/app/SoundNaming.hpp"

using namespace b2b;
using test::channel;
using test::makeChart;
using test::note;

namespace {

core::SlicePlan planOf(const core::bmson::Chart& chart) {
    return core::planSlices(chart, core::bmson::timingOf(chart),
                            std::vector<core::AudioInfo>(chart.soundChannels.size(), core::AudioInfo{1000, 100000}));
}

app::NamingResult name(const core::bmson::Chart& chart, const app::NamingOptions& options = {},
                       infra::TextEncoding encoding = infra::TextEncoding::ShiftJis,
                       core::IdBase base = core::IdBase::Base36) {
    return app::nameSounds(chart, planOf(chart), base, options, "wav", encoding);
}

bool hasSeverity(const std::vector<app::Message>& messages, app::Severity severity, const std::string& part = "") {
    return std::any_of(messages.begin(), messages.end(), [&](const app::Message& message) {
        return message.severity == severity && message.text.find(part) != std::string::npos;
    });
}

}  // namespace

TEST(SoundNaming, NAM001_DefaultPatternsNumberKeyAndBgmSeparately) {
    auto chart = makeChart();
    chart.info.title = "Song";
    chart.soundChannels = {channel("a.wav", {note(1, 0), note(2, 480, true), note(0, 960, true)})};
    EXPECT_EQ(name(chart).paths,
              (std::vector<std::string>{"Song_k0001.wav", "Song_k0002.wav", "Song_b0001.wav"}));
}

TEST(SoundNaming, NAM002_VariablesComeFromTheSlice) {
    auto chart = makeChart();
    chart.soundChannels = {channel("sounds/drums.wav", {note(3, 0), note(0, 960, true)})};
    app::NamingOptions options;
    options.keyPattern = "{src}_{lane}";
    options.bgmPattern = "{measure:03}_{id}";
    EXPECT_EQ(name(chart, options, infra::TextEncoding::ShiftJis, core::IdBase::Base62).paths,
              (std::vector<std::string>{"drums_3.wav", "001_02.wav"}));
}

TEST(SoundNaming, NAM003_UnrepresentableCharactersAreReplaced) {
    auto chart = makeChart();
    chart.info.title = "曲🎵";
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    const auto shiftJis = name(chart);
    EXPECT_EQ(shiftJis.paths.front(), "曲__k0001.wav");
    EXPECT_TRUE(hasSeverity(shiftJis.messages, app::Severity::Warning));
    EXPECT_EQ(name(chart, {}, infra::TextEncoding::Utf8).paths.front(), "曲🎵_k0001.wav");
}

TEST(SoundNaming, NAM004_FolderNameIsSanitized) {
    auto chart = makeChart();
    chart.info.title = "A:B";
    chart.soundChannels = {channel("a.wav", {note(1, 0)})};
    app::NamingOptions options;
    options.folderPattern = "{song}";
    const auto named = name(chart, options);
    EXPECT_EQ(named.paths.front(), "A_B_k0001.wav");
    EXPECT_EQ(named.folder, "A_B");
}

TEST(SoundNaming, NAM005_PatternErrorsNameThePattern) {
    app::NamingOptions options;
    options.bgmPattern = "{x}";
    const auto checked = app::checkNamingPatterns(options);
    EXPECT_TRUE(checked.hasErrors());
    EXPECT_TRUE(hasSeverity(checked.messages, app::Severity::Error, "BGM 音のファイル名"));
}
