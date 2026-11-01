#pragma once

#include <filesystem>
#include <functional>

#include <QFutureWatcher>
#include <QMainWindow>

#include "b2b/app/ConverterSession.hpp"
#include "b2b/core/ports/AudioPlayer.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

namespace b2b::gui {

struct MainWindowPorts {
    app::ConverterSession& session;
    core::AudioPlayer& player;
    app::ProcessLauncher& launcher;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(MainWindowPorts ports, QWidget* parent = nullptr);
    ~MainWindow() override;

    void openChart(const std::filesystem::path& path);
    void startExport();
    [[nodiscard]] bool isExporting() const;
    void waitForExport();
    void playSelectedSlice();
    void playSong();
    void stopPlayback();
    void launchPlayer();
    void refreshNow();

    void setOverwriteConfirmation(std::function<bool(const QString&)> confirm);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void buildMenus();
    QWidget* buildFileRow();
    QWidget* buildOptionsPanel();
    QWidget* buildSlicePanel();
    QWidget* buildActionRow();

    void loadSettings();
    void saveSettings() const;
    void applyOptionsToWidgets(const app::ConvertOptions& options);
    [[nodiscard]] app::ConvertOptions optionsFromWidgets() const;
    void scheduleRefresh();
    void updateView();
    void updatePatternExamples();
    void updateSliceTable();
    void showMessages(const std::vector<app::Message>& messages);
    void appendMessage(app::Severity severity, const QString& text);
    void setBusy(bool busy);
    void chooseChart();
    void chooseOutput();
    void editPlayerSettings();
    void finishExport();
    [[nodiscard]] app::PreviewSource previewSource() const;
    void play(const app::PreviewResult& result);

    app::ConverterSession& session_;
    core::AudioPlayer& player_;
    app::ProcessLauncher& launcher_;
    app::PlayerSettings playerSettings_;
    std::function<bool(const QString&)> confirmOverwrite_;
    QFutureWatcher<app::ExportAllOutcome>* exportWatcher_;
    QTimer* refreshTimer_;
    bool applyingOptions_ = false;
    bool exporting_ = false;

    QLineEdit* chartEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QSpinBox* qualitySpin_ = nullptr;
    QLineEdit* folderEdit_ = nullptr;
    QLineEdit* keyEdit_ = nullptr;
    QLineEdit* bgmEdit_ = nullptr;
    QLabel* folderExample_ = nullptr;
    QLabel* keyExample_ = nullptr;
    QLabel* bgmExample_ = nullptr;
    QComboBox* edgeCombo_ = nullptr;
    QDoubleSpinBox* fadeOutSpin_ = nullptr;
    QCheckBox* fadeInCheck_ = nullptr;
    QDoubleSpinBox* fadeInSpin_ = nullptr;
    QComboBox* encodingCombo_ = nullptr;
    QComboBox* difficultyCombo_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QTableWidget* sliceTable_ = nullptr;
    QComboBox* previewCombo_ = nullptr;
    QPushButton* playSliceButton_ = nullptr;
    QPushButton* playSongButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QListWidget* messages_ = nullptr;
    QWidget* optionsPanel_ = nullptr;
};

}  // namespace b2b::gui
