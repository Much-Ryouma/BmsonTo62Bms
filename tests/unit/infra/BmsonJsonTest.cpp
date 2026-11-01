#include <gtest/gtest.h>

#include "b2b/infra/bmson/BmsonJson.hpp"

using namespace b2b;
using infra::parseBmson;

namespace {

std::string minimal(const std::string& infoExtra) {
    return R"({"version":"1.0.0","info":{"title":"T","init_bpm":120)" + infoExtra + R"(},"sound_channels":[]})";
}

}  // namespace

TEST(BmsonJson, BJS001_ReadsEveryFieldUsedForConversion) {
    const std::string text = R"({
      "version": "1.0.0",
      "info": {"title": "曲", "subtitle": "sub", "artist": "A", "subartists": ["x", "y"], "genre": "G",
               "mode_hint": "beat-7k", "chart_name": "HYPER", "level": 9, "init_bpm": 150.5, "judge_rank": 80,
               "total": 130, "back_image": "back.png", "eyecatch_image": "eye.png", "title_image": "title.png",
               "banner_image": "banner.png", "preview_music": "pre.ogg", "resolution": 480},
      "lines": [{"y": 0}, {"y": 1920}],
      "bpm_events": [{"y": 1920, "bpm": 75}],
      "stop_events": [{"y": 960, "duration": 240}],
      "sound_channels": [{"name": "a.wav", "notes": [
          {"x": null, "y": 0, "l": 0, "c": false},
          {"x": 3, "y": 480, "l": 240, "c": true, "up": true}]}],
      "bga": {"bga_header": [{"id": 1, "name": "bg.mp4"}], "bga_events": [{"y": 0, "id": 1}],
              "layer_events": [{"y": 960, "id": 1}], "poor_events": [{"y": 1920, "id": 1}]}
    })";
    const auto parsed = parseBmson(text);
    const auto& chart = parsed.chart;
    EXPECT_EQ(chart.info.title, "曲");
    EXPECT_EQ(chart.info.subtitle, "sub");
    EXPECT_EQ(chart.info.subartists, (std::vector<std::string>{"x", "y"}));
    EXPECT_EQ(chart.info.chartName, "HYPER");
    EXPECT_EQ(chart.info.level, 9);
    EXPECT_DOUBLE_EQ(chart.info.initBpm, 150.5);
    EXPECT_DOUBLE_EQ(chart.info.judgeRank, 80.0);
    EXPECT_DOUBLE_EQ(chart.info.total, 130.0);
    EXPECT_EQ(chart.info.backImage, "back.png");
    EXPECT_EQ(chart.info.eyecatchImage, "eye.png");
    EXPECT_EQ(chart.info.titleImage, "title.png");
    EXPECT_EQ(chart.info.bannerImage, "banner.png");
    EXPECT_EQ(chart.info.previewMusic, "pre.ogg");
    EXPECT_EQ(chart.info.resolution, 480);
    EXPECT_EQ(chart.lines, (std::vector<core::Pulse>{0, 1920}));
    ASSERT_EQ(chart.bpmEvents.size(), 1u);
    EXPECT_DOUBLE_EQ(chart.bpmEvents[0].bpm, 75.0);
    ASSERT_EQ(chart.stopEvents.size(), 1u);
    EXPECT_EQ(chart.stopEvents[0].durationPulses, 240);
    ASSERT_EQ(chart.soundChannels.size(), 1u);
    const auto& notes = chart.soundChannels[0].notes;
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].x, 0);
    EXPECT_FALSE(notes[0].c);
    EXPECT_EQ(notes[1].x, 3);
    EXPECT_EQ(notes[1].l, 240);
    EXPECT_TRUE(notes[1].c);
    EXPECT_TRUE(notes[1].up);
    ASSERT_EQ(chart.bga.headers.size(), 1u);
    EXPECT_EQ(chart.bga.headers[0].name, "bg.mp4");
    EXPECT_EQ(chart.bga.bgaEvents.size(), 1u);
    EXPECT_EQ(chart.bga.layerEvents.size(), 1u);
    EXPECT_EQ(chart.bga.poorEvents.size(), 1u);
}

TEST(BmsonJson, BJS002_MissingInitialTempoIsAnError) {
    EXPECT_THROW(static_cast<void>(parseBmson(R"({"info":{"title":"T"},"sound_channels":[]})")), infra::BmsonFormatError);
}

TEST(BmsonJson, BJS003_ResolutionDefaultsAndNegativeValues) {
    EXPECT_EQ(parseBmson(minimal("")).chart.info.resolution, 240);
    EXPECT_EQ(parseBmson(minimal(R"(,"resolution":0)")).chart.info.resolution, 240);
    const auto negative = parseBmson(minimal(R"(,"resolution":-480)"));
    EXPECT_EQ(negative.chart.info.resolution, 480);
    EXPECT_FALSE(negative.warnings.empty());
}

TEST(BmsonJson, BJS004_ByteOrderMarkIsAccepted) {
    EXPECT_EQ(parseBmson("\xEF\xBB\xBF" + minimal("")).chart.info.title, "T");
}

TEST(BmsonJson, BJS005_BrokenJsonIsAnError) {
    EXPECT_THROW(static_cast<void>(parseBmson("{\"info\":")), infra::BmsonFormatError);
}

TEST(BmsonJson, BJS006_MissingModeHintMeansBeat7k) {
    EXPECT_EQ(parseBmson(minimal("")).chart.info.modeHint, "beat-7k");
}
