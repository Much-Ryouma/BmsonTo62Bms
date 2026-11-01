#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <tuple>

#include "BmsReader.hpp"
#include "ChartBuilder.hpp"
#include "Fakes.hpp"
#include "b2b/app/ConvertService.hpp"

using namespace b2b;
using test::channel;
using test::makeChart;
using test::note;

namespace {

constexpr double kToleranceSeconds = 1e-4;

struct Expected {
    double seconds;
    std::string channel;
    std::string file;
};

struct Actual {
    std::vector<Expected> notes;
    std::vector<std::tuple<double, double, std::string, std::string>> longNotes;
};

app::Conversion convert(const core::bmson::Chart& chart, test::FakeFileSink& files, test::ReadBms& read) {
    test::FakeChartSource charts;
    charts.chart = chart;
    test::FakeAudioSource audio;
    for (const auto& soundChannel : chart.soundChannels) {
        audio.files[soundChannel.name] = core::AudioInfo{44100, 44100 * 600};
    }
    test::FakeAudioCodec codec;
    const app::ConvertService service(charts, audio, codec, files);
    auto conversion = service.prepare("song.bmson", app::ConvertOptions{});
    EXPECT_FALSE(conversion.hasErrors());
    const auto outcome = service.writeBms(conversion, "song.bme", infra::TextEncoding::Utf8);
    EXPECT_TRUE(outcome.written);
    read = test::readBms(files.written.at("song.bme"));
    return conversion;
}

void expectSameNotes(const core::bmson::Chart& chart, const app::Conversion& conversion, const test::ReadBms& read) {
    const auto timing = core::bmson::timingOf(chart);
    std::vector<Expected> expectedNotes;
    std::vector<std::tuple<double, double, std::string, std::string>> expectedLong;
    for (const auto& placed : conversion.plan.notes) {
        const std::string& file = conversion.soundFiles[placed.slice];
        if (placed.lane == 0) {
            expectedNotes.push_back({timing.secondsAt(placed.y), "01", file});
        } else if (placed.length > 0) {
            expectedLong.emplace_back(timing.secondsAt(placed.y), timing.secondsAt(placed.y + placed.length),
                                      core::channelForLane(placed.lane, true), file);
        } else {
            expectedNotes.push_back({timing.secondsAt(placed.y), core::channelForLane(placed.lane, false), file});
        }
    }

    std::vector<Expected> actualNotes;
    for (const auto& readNote : read.notes) {
        actualNotes.push_back({readNote.seconds, readNote.channel, read.wav.at(readNote.id)});
    }
    std::vector<std::tuple<double, double, std::string, std::string>> actualLong;
    for (const auto& longNote : read.longNotes) {
        actualLong.emplace_back(longNote.startSeconds, longNote.endSeconds, longNote.channel, read.wav.at(longNote.id));
    }

    auto byKey = [](const Expected& a, const Expected& b) {
        return std::tie(a.channel, a.file, a.seconds) < std::tie(b.channel, b.file, b.seconds);
    };
    std::sort(expectedNotes.begin(), expectedNotes.end(), byKey);
    std::sort(actualNotes.begin(), actualNotes.end(), byKey);
    ASSERT_EQ(actualNotes.size(), expectedNotes.size());
    for (std::size_t index = 0; index < expectedNotes.size(); ++index) {
        EXPECT_EQ(actualNotes[index].channel, expectedNotes[index].channel);
        EXPECT_EQ(actualNotes[index].file, expectedNotes[index].file);
        EXPECT_NEAR(actualNotes[index].seconds, expectedNotes[index].seconds, kToleranceSeconds);
    }

    std::sort(expectedLong.begin(), expectedLong.end());
    std::sort(actualLong.begin(), actualLong.end());
    ASSERT_EQ(actualLong.size(), expectedLong.size());
    for (std::size_t index = 0; index < expectedLong.size(); ++index) {
        EXPECT_NEAR(std::get<0>(actualLong[index]), std::get<0>(expectedLong[index]), kToleranceSeconds);
        EXPECT_NEAR(std::get<1>(actualLong[index]), std::get<1>(expectedLong[index]), kToleranceSeconds);
        EXPECT_EQ(std::get<2>(actualLong[index]), std::get<2>(expectedLong[index]));
        EXPECT_EQ(std::get<3>(actualLong[index]), std::get<3>(expectedLong[index]));
    }
}

}  // namespace

TEST(RoundTrip, RTP001_EveryNotePlaysAtTheSameTimeWithTheSameSound) {
    auto chart = makeChart(128.0, 240);
    chart.lines = {0, 960, 1680, 2640, 3600};
    chart.bpmEvents = {{1200, 173.25}, {2640, 96.0}};
    chart.stopEvents = {{1680, 120}, {3000, 37}};
    chart.soundChannels = {
        channel("drums.wav", {note(1, 0), note(2, 0, true), note(0, 160, true), note(3, 240, true),
                              note(8, 720, true, 600), note(4, 1680, true), note(5, 1700, true), note(6, 2640, true),
                              note(7, 3000, true), note(1, 3050, true, 400)}),
        channel("bass.wav", {note(0, 0), note(0, 320, true), note(0, 1300), note(0, 1680, true), note(6, 3600, true)}),
        channel("pad.wav", {note(0, 0), note(0, 960, true), note(0, 2000, true)})};

    test::FakeFileSink files;
    test::ReadBms read;
    const auto conversion = convert(chart, files, read);
    ASSERT_TRUE(conversion.document);
    EXPECT_EQ(read.base, core::IdBase::Base36);
    expectSameNotes(chart, conversion, read);
}

TEST(RoundTrip, RTP002_Base62ChartReadsBackWithDistinctSounds) {
    auto chart = makeChart();
    std::vector<core::bmson::Note> notes;
    for (int index = 0; index < 1300; ++index) {
        notes.push_back(note(index % 7 + 1, static_cast<core::Pulse>(index) * 120, index != 0));
    }
    chart.soundChannels = {channel("long.wav", notes)};

    test::FakeFileSink files;
    test::ReadBms read;
    const auto conversion = convert(chart, files, read);
    ASSERT_TRUE(conversion.document);
    EXPECT_EQ(read.base, core::IdBase::Base62);
    EXPECT_EQ(read.wav.size(), 1300u);
    expectSameNotes(chart, conversion, read);
}
