#include <gtest/gtest.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>

#include <filesystem>
#include <fstream>

#include "Fakes.hpp"
#include "MainWindow.hpp"
#include "QtText.hpp"
#include "Settings.hpp"
#include "WavWriter.hpp"

using namespace b2b;

namespace {

class FakePlayer final : public core::AudioPlayer {
public:
    std::optional<core::StereoAudio> last;
    [[nodiscard]] int sampleRate() const override { return 48000; }
    bool start(core::StereoAudio audio) override {
        last = std::move(audio);
        return true;
    }
    void stop() override {}
    [[nodiscard]] bool isPlaying() const override { return false; }
    [[nodiscard]] std::int64_t playedFrames() const override { return 0; }
};

struct Fixture {
    std::filesystem::path directory;
    app::LocalChartSource charts;
    app::LocalAudioSource audio;
    app::LocalAudioCodec codec;
    app::LocalFileSink files;
    app::ConverterSession session{charts, audio, codec, files};
    FakePlayer player;
    test::FakeLauncher launcher;

    explicit Fixture(const char* name) {
        directory = std::filesystem::temp_directory_path() / "b2b_gui_tests" / name;
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        gui::setSettingsFile(gui::toQString(directory / "bmsonto62bms.ini"));
        test::writePcmWav(directory / "drums.wav", 44100, 2, 16, test::countingBytes(44100 * 2 * 2 * 2));
        test::writePcmWav(directory / "bass.wav", 44100, 1, 16, test::countingBytes(44100 * 2 * 3));
        std::ofstream(directory / "song.bmson")
            << R"({"version":"1.0.0","info":{"title":"Song","init_bpm":120,"resolution":240},"sound_channels":[)"
            << R"({"name":"drums.wav","notes":[{"x":1,"y":0},{"x":2,"y":240,"c":true},{"x":3,"y":480,"c":true}]},)"
            << R"({"name":"bass.wav","notes":[{"x":0,"y":0},{"x":0,"y":960,"c":true}]}]})";
    }

    std::unique_ptr<gui::MainWindow> window() { return std::make_unique<gui::MainWindow>(gui::MainWindowPorts{session, player, launcher}); }
    std::filesystem::path chart() const { return directory / "song.bmson"; }
};

template <typename Widget>
Widget* child(QWidget& window, const char* name) {
    auto* widget = window.findChild<Widget*>(QString::fromLatin1(name));
    EXPECT_NE(widget, nullptr) << name;
    return widget;
}

bool messagesContain(QWidget& window, const QString& part) {
    auto* list = child<QListWidget>(window, "messages");
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->text().contains(part)) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(MainWindowGui, GUI001_OpeningAChartListsTheSlices) {
    Fixture fixture("gui001");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    EXPECT_EQ(child<QTableWidget>(*window, "slices")->rowCount(), 5);
    EXPECT_TRUE(child<QLabel>(*window, "summary")->text().contains(QStringLiteral("スライス 5 個")));
    EXPECT_EQ(gui::toPath(child<QLineEdit>(*window, "outputPath")->text()), fixture.directory / "song.bme");
}

TEST(MainWindowGui, GUI002_EditingANamePatternRenamesTheSlices) {
    Fixture fixture("gui002");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    child<QLineEdit>(*window, "keyPattern")->setText(QStringLiteral("{src}_{index:02}"));
    window->refreshNow();
    EXPECT_EQ(child<QTableWidget>(*window, "slices")->item(0, 2)->text(), QStringLiteral("wav/drums_01.wav"));
    EXPECT_TRUE(child<QLabel>(*window, "keyPatternExample")->text().contains(QStringLiteral("drums_01.wav")));
}

TEST(MainWindowGui, GUI003_InvalidPatternShowsTheErrorAndBlocksExport) {
    Fixture fixture("gui003");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    child<QLineEdit>(*window, "keyPattern")->setText(QStringLiteral("{oops}"));
    window->refreshNow();
    EXPECT_TRUE(child<QLabel>(*window, "keyPatternExample")->text().contains(QStringLiteral("oops")));
    EXPECT_FALSE(child<QPushButton>(*window, "export")->isEnabled());
}

TEST(MainWindowGui, GUI004_ExportWritesFilesAndEnablesThePlayer) {
    Fixture fixture("gui004");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    window->startExport();
    window->waitForExport();
    EXPECT_TRUE(std::filesystem::exists(fixture.directory / "song.bme"));
    EXPECT_TRUE(std::filesystem::exists(fixture.directory / "wav" / "Song_k0001.wav"));
    EXPECT_TRUE(child<QPushButton>(*window, "launchPlayer")->isEnabled());
    EXPECT_EQ(child<QComboBox>(*window, "previewSource")->currentIndex(), 1);
}

TEST(MainWindowGui, GUI005_DecliningTheOverwriteKeepsTheFiles) {
    Fixture fixture("gui005");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    window->startExport();
    window->waitForExport();
    QString asked;
    window->setOverwriteConfirmation([&asked](const QString& question) {
        asked = question;
        return false;
    });
    window->startExport();
    EXPECT_FALSE(window->isExporting());
    EXPECT_TRUE(asked.contains(QStringLiteral("song.bme")));
}

TEST(MainWindowGui, GUI006_PlayingASliceSendsItToTheOutput) {
    Fixture fixture("gui006");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    child<QTableWidget>(*window, "slices")->selectRow(0);
    window->playSelectedSlice();
    ASSERT_TRUE(fixture.player.last);
    EXPECT_EQ(fixture.player.last->frames(), fixture.session.conversion().plan.slices[0].frameCount());
}

TEST(MainWindowGui, GUI007_PlayingTheSongCoversEveryNote) {
    Fixture fixture("gui007");
    auto window = fixture.window();
    window->openChart(fixture.chart());
    window->playSong();
    ASSERT_TRUE(fixture.player.last);
    EXPECT_GT(fixture.player.last->seconds(), 2.0);
}

TEST(MainWindowGui, GUI008_PlayerStartsOnlyAfterExport) {
    Fixture fixture("gui008");
    {
        QSettings settings(gui::settingsFile(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("player/executable"), QStringLiteral("/opt/player/play"));
    }
    auto window = fixture.window();
    window->openChart(fixture.chart());
    window->launchPlayer();
    EXPECT_EQ(fixture.launcher.launches, 0);
    EXPECT_TRUE(messagesContain(*window, QStringLiteral("書き出されていません")));
    window->startExport();
    window->waitForExport();
    window->launchPlayer();
    EXPECT_EQ(fixture.launcher.launches, 1);
    EXPECT_EQ(fixture.launcher.program, "/opt/player/play");
    ASSERT_EQ(fixture.launcher.arguments.size(), 1u);
    EXPECT_EQ(std::filesystem::path(fixture.launcher.arguments[0]), std::filesystem::absolute(fixture.directory / "song.bme"));
}

TEST(MainWindowGui, GUI009_SettingsSurviveARestart) {
    Fixture fixture("gui009");
    {
        auto window = fixture.window();
        child<QComboBox>(*window, "soundFormat")->setCurrentIndex(1);
        child<QSpinBox>(*window, "oggQuality")->setValue(7);
        child<QComboBox>(*window, "edgeMode")->setCurrentIndex(1);
        window->close();
    }
    auto window = fixture.window();
    EXPECT_EQ(child<QComboBox>(*window, "soundFormat")->currentIndex(), 1);
    EXPECT_EQ(child<QSpinBox>(*window, "oggQuality")->value(), 7);
    EXPECT_EQ(child<QComboBox>(*window, "edgeMode")->currentIndex(), 1);
}

TEST(MainWindowGui, GUI010_OnlyRelevantSettingsAreEditable) {
    Fixture fixture("gui010");
    auto window = fixture.window();
    child<QComboBox>(*window, "edgeMode")->setCurrentIndex(0);
    child<QComboBox>(*window, "soundFormat")->setCurrentIndex(0);
    window->refreshNow();
    EXPECT_FALSE(child<QDoubleSpinBox>(*window, "fadeOut")->isEnabled());
    EXPECT_FALSE(child<QSpinBox>(*window, "oggQuality")->isEnabled());
    child<QComboBox>(*window, "edgeMode")->setCurrentIndex(1);
    child<QComboBox>(*window, "soundFormat")->setCurrentIndex(1);
    window->refreshNow();
    EXPECT_TRUE(child<QDoubleSpinBox>(*window, "fadeOut")->isEnabled());
    EXPECT_TRUE(child<QSpinBox>(*window, "oggQuality")->isEnabled());
}
