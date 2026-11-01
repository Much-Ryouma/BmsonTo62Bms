#include "MainWindow.hpp"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "AboutDialog.hpp"
#include "PlayerSettingsDialog.hpp"
#include "QtText.hpp"
#include "Settings.hpp"
#include "b2b/core/BuildInfo.hpp"
#include "b2b/core/timing/MeasureLayout.hpp"
#include "b2b/infra/fs/Utf8Path.hpp"

namespace b2b::gui {

namespace {

constexpr int kRefreshDelayMs = 250;

enum SliceColumn {
    ColumnId,
    ColumnKind,
    ColumnFile,
    ColumnSource,
    ColumnStart,
    ColumnLength,
    ColumnMeasure,
    ColumnCharts,
    ColumnCount
};

const QColor kOutsideBaseBackground(0xfd, 0xeb, 0xd0);

class WaitCursor {
public:
    WaitCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursor() { QApplication::restoreOverrideCursor(); }
    WaitCursor(const WaitCursor&) = delete;
    WaitCursor& operator=(const WaitCursor&) = delete;
};

QString seconds(double value) {
    return QString::number(value, 'f', 3);
}

QLabel* exampleLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void setExample(QLabel* label, const QString& text, bool error) {
    label->setText(text);
    label->setStyleSheet(error ? QStringLiteral("color: #c0392b;") : QStringLiteral("color: palette(mid);"));
}

}  // namespace

MainWindow::MainWindow(MainWindowPorts ports, QWidget* parent)
    : QMainWindow(parent),
      session_(ports.session),
      player_(ports.player),
      launcher_(ports.launcher),
      exportWatcher_(new QFutureWatcher<app::ExportAllOutcome>(this)),
      refreshTimer_(new QTimer(this)) {
    setWindowTitle(QStringLiteral("bmsonto62bms"));
    setAcceptDrops(true);
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(kRefreshDelayMs);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshNow);
    connect(exportWatcher_, &QFutureWatcher<app::ExportAllOutcome>::finished, this, &MainWindow::finishExport);

    buildMenus();
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(buildFileRow());
    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setObjectName(QStringLiteral("mainSplitter"));
    splitter->addWidget(buildOptionsPanel());
    splitter->addWidget(buildSlicePanel());
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);
    layout->addWidget(buildActionRow());
    messages_ = new QListWidget(central);
    messages_->setObjectName(QStringLiteral("messages"));
    messages_->setMaximumHeight(120);
    layout->addWidget(messages_);
    setCentralWidget(central);

    loadSettings();
    resize(1180, 760);
    updateView();
}

MainWindow::~MainWindow() {
    if (exporting_) {
        exportWatcher_->waitForFinished();
    }
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(QStringLiteral("ファイル(&F)"));
    auto* open = file->addAction(QStringLiteral("bmson を開く…（複数選ぶと譜面セット）"), this, &MainWindow::chooseChart);
    open->setShortcut(QKeySequence::Open);
    auto* exportAction = file->addAction(QStringLiteral("書き出す"), this, &MainWindow::startExport);
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    auto* launch = file->addAction(QStringLiteral("外部プレイヤーで開く"), this, &MainWindow::launchPlayer);
    launch->setShortcut(QKeySequence(Qt::Key_F5));
    file->addSeparator();
    file->addAction(QStringLiteral("終了"), this, &QWidget::close);

    auto* settings = menuBar()->addMenu(QStringLiteral("設定(&S)"));
    settings->addAction(QStringLiteral("外部プレイヤー…"), this, &MainWindow::editPlayerSettings);

    auto* help = menuBar()->addMenu(QStringLiteral("ヘルプ(&H)"));
    help->addAction(QStringLiteral("使い方（GitHub）を開く"), this, [] {
        QDesktopServices::openUrl(QUrl(toQString(core::kHomepageUrl)));
    });
    help->addAction(QStringLiteral("バージョン情報…"), this, [this] { showAboutDialog(this); });
}

QWidget* MainWindow::buildFileRow() {
    auto* row = new QWidget(this);
    auto* layout = new QGridLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    chartEdit_ = new QLineEdit(row);
    chartEdit_->setObjectName(QStringLiteral("chartPath"));
    chartEdit_->setReadOnly(true);
    chartEdit_->setPlaceholderText(QStringLiteral("bmson を開くか、ウィンドウにドロップしてください"));
    auto* openButton = new QPushButton(QStringLiteral("開く…"), row);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::chooseChart);
    outputEdit_ = new QLineEdit(row);
    outputEdit_->setObjectName(QStringLiteral("outputPath"));
    connect(outputEdit_, &QLineEdit::editingFinished, this, [this] {
        if (toPath(outputEdit_->text()) != session_.outputPath()) {
            session_.setOutputPath(toPath(outputEdit_->text()));
            updateView();
        }
    });
    auto* outputButton = new QPushButton(QStringLiteral("参照…"), row);
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::chooseOutput);
    layout->addWidget(new QLabel(QStringLiteral("bmson"), row), 0, 0);
    layout->addWidget(chartEdit_, 0, 1);
    layout->addWidget(openButton, 0, 2);

    chartCombo_ = new QComboBox(row);
    chartCombo_->setObjectName(QStringLiteral("chartSelector"));
    chartCombo_->setToolTip(QStringLiteral("表示・試聴する譜面。書き出しでは、譜面セットのすべての譜面を書き出します"));
    connect(chartCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!applyingOptions_ && index >= 0) {
            selectChart(index);
        }
    });
    difficultyCombo_ = new QComboBox(row);
    difficultyCombo_->setObjectName(QStringLiteral("difficulty"));
    fillDifficultyCombo(*difficultyCombo_);
    connect(difficultyCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (applyingOptions_ || !session_.hasChart() || exporting_) {
            return;
        }
        const WaitCursor wait;
        session_.setDifficulty(index > 0 ? std::optional<int>(index) : std::nullopt);
        updateView();
    });
    auto* chartRow = new QHBoxLayout;
    chartRow->addWidget(chartCombo_, 1);
    chartRow->addWidget(new QLabel(QStringLiteral("#DIFFICULTY"), row));
    chartRow->addWidget(difficultyCombo_);
    layout->addWidget(new QLabel(QStringLiteral("譜面"), row), 1, 0);
    layout->addLayout(chartRow, 1, 1, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("書き出す BMS"), row), 2, 0);
    layout->addWidget(outputEdit_, 2, 1);
    layout->addWidget(outputButton, 2, 2);
    return row;
}

QWidget* MainWindow::buildOptionsPanel() {
    optionsPanel_ = new QGroupBox(QStringLiteral("書き出しの設定"), this);
    auto* form = new QFormLayout(optionsPanel_);

    formatCombo_ = new QComboBox(optionsPanel_);
    formatCombo_->setObjectName(QStringLiteral("soundFormat"));
    formatCombo_->addItems({QStringLiteral("WAV（元と同じビット深度）"), QStringLiteral("OGG")});
    qualitySpin_ = new QSpinBox(optionsPanel_);
    qualitySpin_->setObjectName(QStringLiteral("oggQuality"));
    qualitySpin_->setRange(0, 10);
    form->addRow(QStringLiteral("音声の形式"), formatCombo_);
    form->addRow(QStringLiteral("OGG の品質"), qualitySpin_);

    auto addPattern = [&](const QString& label, const char* name, QLineEdit*& edit, QLabel*& example) {
        edit = new QLineEdit(optionsPanel_);
        edit->setObjectName(QString::fromLatin1(name));
        example = exampleLabel(optionsPanel_);
        example->setObjectName(QString::fromLatin1(name) + QStringLiteral("Example"));
        auto* box = new QWidget(optionsPanel_);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(2);
        boxLayout->addWidget(edit);
        boxLayout->addWidget(example);
        form->addRow(label, box);
        connect(edit, &QLineEdit::textChanged, this, &MainWindow::scheduleRefresh);
    };
    addPattern(QStringLiteral("サブフォルダ"), "folderPattern", folderEdit_, folderExample_);
    addPattern(QStringLiteral("パラ音の名前"), "keyPattern", keyEdit_, keyExample_);
    addPattern(QStringLiteral("BGM 音の名前"), "bgmPattern", bgmEdit_, bgmExample_);
    auto* variables = new QLabel(QStringLiteral(
        "変数: {song} 曲名、{src} 元の音声、{lane} レーン、{measure} 小節、{index} 連番、{id} BMS の ID\n"
        "書式: {index:04} で 4 桁にそろえる、{song:.16} で 16 文字まで"), optionsPanel_);
    variables->setWordWrap(true);
    variables->setStyleSheet(QStringLiteral("color: palette(mid);"));
    form->addRow(variables);

    edgeCombo_ = new QComboBox(optionsPanel_);
    edgeCombo_->setObjectName(QStringLiteral("edgeMode"));
    edgeCombo_->addItems({QStringLiteral("切りっぱなし（元の音とサンプル単位で同じ）"), QStringLiteral("フェード")});
    fadeOutSpin_ = new QDoubleSpinBox(optionsPanel_);
    fadeOutSpin_->setObjectName(QStringLiteral("fadeOut"));
    fadeOutSpin_->setRange(0.0, core::kMaxFadeMs);
    fadeOutSpin_->setDecimals(1);
    fadeOutSpin_->setSuffix(QStringLiteral(" ms"));
    fadeInCheck_ = new QCheckBox(QStringLiteral("フェードイン"), optionsPanel_);
    fadeInCheck_->setObjectName(QStringLiteral("fadeIn"));
    fadeInSpin_ = new QDoubleSpinBox(optionsPanel_);
    fadeInSpin_->setObjectName(QStringLiteral("fadeInLength"));
    fadeInSpin_->setRange(0.0, core::kMaxFadeMs);
    fadeInSpin_->setDecimals(1);
    fadeInSpin_->setSuffix(QStringLiteral(" ms"));
    auto* fadeInRow = new QWidget(optionsPanel_);
    auto* fadeInLayout = new QHBoxLayout(fadeInRow);
    fadeInLayout->setContentsMargins(0, 0, 0, 0);
    fadeInLayout->addWidget(fadeInCheck_);
    fadeInLayout->addWidget(fadeInSpin_, 1);
    form->addRow(QStringLiteral("切り口"), edgeCombo_);
    form->addRow(QStringLiteral("フェードアウト"), fadeOutSpin_);
    form->addRow(QString(), fadeInRow);

    encodingCombo_ = new QComboBox(optionsPanel_);
    encodingCombo_->setObjectName(QStringLiteral("encoding"));
    encodingCombo_->addItems({QStringLiteral("Shift_JIS"), QStringLiteral("UTF-8")});
    form->addRow(QStringLiteral("BMS の文字コード"), encodingCombo_);

    allowExtraCheck_ = new QCheckBox(QStringLiteral("基準の譜面にないスライスを追加で切り出す"), optionsPanel_);
    allowExtraCheck_->setObjectName(QStringLiteral("allowExtraSlices"));
    allowExtraCheck_->setToolTip(QStringLiteral(
        "譜面セットで、ほかの難易度にだけある切り出し位置の音も書き出します。\n"
        "チェックしないときは、そのような譜面があるとエラーになります"));
    form->addRow(QStringLiteral("譜面セット"), allowExtraCheck_);
    connect(allowExtraCheck_, &QCheckBox::toggled, this, &MainWindow::scheduleRefresh);

    for (QComboBox* combo : {formatCombo_, edgeCombo_, encodingCombo_}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &MainWindow::scheduleRefresh);
    }
    connect(qualitySpin_, &QSpinBox::valueChanged, this, &MainWindow::scheduleRefresh);
    connect(fadeOutSpin_, &QDoubleSpinBox::valueChanged, this, &MainWindow::scheduleRefresh);
    connect(fadeInSpin_, &QDoubleSpinBox::valueChanged, this, &MainWindow::scheduleRefresh);
    connect(fadeInCheck_, &QCheckBox::toggled, this, &MainWindow::scheduleRefresh);
    optionsPanel_->setMinimumWidth(360);
    return optionsPanel_;
}

QWidget* MainWindow::buildSlicePanel() {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    summaryLabel_ = new QLabel(panel);
    summaryLabel_->setObjectName(QStringLiteral("summary"));
    layout->addWidget(summaryLabel_);

    sliceTable_ = new QTableWidget(0, ColumnCount, panel);
    sliceTable_->setObjectName(QStringLiteral("slices"));
    sliceTable_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("種類"), QStringLiteral("ファイル名"),
                                            QStringLiteral("元の音声"), QStringLiteral("開始（秒）"),
                                            QStringLiteral("長さ（秒）"), QStringLiteral("最初の小節"), QStringLiteral("使う譜面")});
    sliceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sliceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    sliceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sliceTable_->verticalHeader()->setVisible(false);
    sliceTable_->horizontalHeader()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    connect(sliceTable_, &QTableWidget::cellDoubleClicked, this, [this] { playSelectedSlice(); });
    layout->addWidget(sliceTable_, 1);

    auto* controls = new QHBoxLayout;
    previewCombo_ = new QComboBox(panel);
    previewCombo_->setObjectName(QStringLiteral("previewSource"));
    previewCombo_->addItems({QStringLiteral("変換前の音（切り出し＋切り口の処理）"), QStringLiteral("書き出したファイル")});
    playSliceButton_ = new QPushButton(QStringLiteral("選んだスライスを再生"), panel);
    playSliceButton_->setObjectName(QStringLiteral("playSlice"));
    playSongButton_ = new QPushButton(QStringLiteral("曲全体を再生"), panel);
    playSongButton_->setObjectName(QStringLiteral("playSong"));
    stopButton_ = new QPushButton(QStringLiteral("停止"), panel);
    stopButton_->setObjectName(QStringLiteral("stop"));
    connect(playSliceButton_, &QPushButton::clicked, this, &MainWindow::playSelectedSlice);
    connect(playSongButton_, &QPushButton::clicked, this, &MainWindow::playSong);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopPlayback);
    controls->addWidget(new QLabel(QStringLiteral("試聴する音"), panel));
    controls->addWidget(previewCombo_);
    controls->addStretch(1);
    controls->addWidget(playSliceButton_);
    controls->addWidget(playSongButton_);
    controls->addWidget(stopButton_);
    layout->addLayout(controls);
    return panel;
}

QWidget* MainWindow::buildActionRow() {
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    exportButton_ = new QPushButton(QStringLiteral("書き出す"), row);
    exportButton_->setObjectName(QStringLiteral("export"));
    exportButton_->setDefault(true);
    launchButton_ = new QPushButton(QStringLiteral("外部プレイヤーで開く"), row);
    launchButton_->setObjectName(QStringLiteral("launchPlayer"));
    progress_ = new QProgressBar(row);
    progress_->setObjectName(QStringLiteral("progress"));
    progress_->setVisible(false);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::startExport);
    connect(launchButton_, &QPushButton::clicked, this, &MainWindow::launchPlayer);
    for (QPushButton* button : {exportButton_, launchButton_}) {
        button->setMinimumWidth(180);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    layout->addWidget(exportButton_);
    layout->addWidget(launchButton_);
    layout->addWidget(progress_, 1);
    layout->addStretch();
    return row;
}

void MainWindow::loadSettings() {
    auto settings = openSettings();
    playerSettings_ = loadPlayerSettings(*settings);
    const app::ConvertOptions options = loadConvertOptions(*settings);
    applyOptionsToWidgets(options);
    session_.setOptions(options);
    restoreGeometry(settings->value(QStringLiteral("window/geometry")).toByteArray());
}

void MainWindow::saveSettings() const {
    auto settings = openSettings();
    saveConvertOptions(*settings, optionsFromWidgets());
    savePlayerSettings(*settings, playerSettings_);
    settings->setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::applyOptionsToWidgets(const app::ConvertOptions& options) {
    applyingOptions_ = true;
    formatCombo_->setCurrentIndex(options.soundFormat == app::SoundFormat::Ogg ? 1 : 0);
    qualitySpin_->setValue(options.oggQuality);
    folderEdit_->setText(toQString(options.naming.folderPattern));
    keyEdit_->setText(toQString(options.naming.keyPattern));
    bgmEdit_->setText(toQString(options.naming.bgmPattern));
    edgeCombo_->setCurrentIndex(options.fade.mode == core::EdgeMode::Fade ? 1 : 0);
    fadeOutSpin_->setValue(options.fade.fadeOutMs);
    fadeInCheck_->setChecked(options.fade.fadeIn);
    fadeInSpin_->setValue(options.fade.fadeInMs);
    encodingCombo_->setCurrentIndex(options.encoding == infra::TextEncoding::Utf8 ? 1 : 0);
    allowExtraCheck_->setChecked(options.allowExtraSlices);
    applyingOptions_ = false;
}

app::ConvertOptions MainWindow::optionsFromWidgets() const {
    app::ConvertOptions options;
    options.soundFormat = formatCombo_->currentIndex() == 1 ? app::SoundFormat::Ogg : app::SoundFormat::Wav;
    options.oggQuality = qualitySpin_->value();
    options.naming.folderPattern = toUtf8(folderEdit_->text());
    options.naming.keyPattern = toUtf8(keyEdit_->text());
    options.naming.bgmPattern = toUtf8(bgmEdit_->text());
    options.fade.mode = edgeCombo_->currentIndex() == 1 ? core::EdgeMode::Fade : core::EdgeMode::Cut;
    options.fade.fadeOutMs = fadeOutSpin_->value();
    options.fade.fadeIn = fadeInCheck_->isChecked();
    options.fade.fadeInMs = fadeInSpin_->value();
    options.encoding = encodingCombo_->currentIndex() == 1 ? infra::TextEncoding::Utf8 : infra::TextEncoding::ShiftJis;
    options.allowExtraSlices = allowExtraCheck_->isChecked();
    return options;
}

void MainWindow::scheduleRefresh() {
    if (applyingOptions_) {
        return;
    }
    refreshTimer_->start();
}

void MainWindow::refreshNow() {
    refreshTimer_->stop();
    if (exporting_) {
        return;
    }
    const WaitCursor wait;
    session_.setOptions(optionsFromWidgets());
    updateView();
}

void MainWindow::openChart(const std::filesystem::path& path) {
    openChartSet({app::ChartRequest{path, std::nullopt}}, 0);
}

void MainWindow::openCharts(const std::vector<std::filesystem::path>& paths) {
    if (exporting_ || paths.empty()) {
        return;
    }
    if (paths.size() == 1) {
        openChart(paths.front());
        return;
    }
    std::vector<app::ChartSummary> summaries;
    {
        const WaitCursor wait;
        for (const auto& path : paths) {
            summaries.push_back(session_.summarize(path));
        }
    }
    std::optional<ChartSetChoice> choice;
    if (chooseChartSet_) {
        choice = chooseChartSet_(summaries);
    } else {
        ChartSetDialog dialog(summaries, this);
        if (dialog.exec() == QDialog::Accepted) {
            choice = dialog.choice();
        }
    }
    if (!choice) {
        return;
    }
    std::vector<app::ChartRequest> requests;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        requests.push_back({paths[index], index < choice->difficulties.size() ? choice->difficulties[index] : std::nullopt});
    }
    openChartSet(requests, choice->baseIndex);
}

void MainWindow::openChartSet(const std::vector<app::ChartRequest>& requests, std::size_t baseIndex) {
    if (exporting_ || requests.empty()) {
        return;
    }
    {
        const WaitCursor wait;
        refreshTimer_->stop();
        session_.setOptions(optionsFromWidgets());
        session_.openCharts(requests, baseIndex);
    }
    const auto& base = requests[session_.baseIndex()].path;
    setWindowTitle(requests.size() == 1
                       ? QStringLiteral("%1 - bmsonto62bms").arg(toQString(base.filename()))
                       : QStringLiteral("%1 ほか %2 譜面 - bmsonto62bms").arg(toQString(base.filename())).arg(requests.size() - 1));
    auto settings = openSettings();
    settings->setValue(QStringLiteral("paths/lastDirectory"), QDir::fromNativeSeparators(toQString(base.parent_path())));
    updateChartSelector();
    updateView();
}

void MainWindow::selectChart(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= session_.chartCount()) {
        return;
    }
    player_.stop();
    session_.selectChart(static_cast<std::size_t>(index));
    updateChartSelector();
    updateView();
}

void MainWindow::updateChartSelector() {
    applyingOptions_ = true;
    chartCombo_->clear();
    for (std::size_t index = 0; index < session_.chartCount(); ++index) {
        const auto& conversion = session_.conversionSet().charts.at(index);
        QString label = toQString(conversion.chartPath.filename());
        if (!conversion.chart.info.chartName.empty()) {
            label += QStringLiteral("　%1").arg(toQString(conversion.chart.info.chartName));
        }
        if (session_.chartCount() > 1 && index == session_.baseIndex()) {
            label += QStringLiteral("（基準）");
        }
        chartCombo_->addItem(label);
    }
    chartCombo_->setCurrentIndex(static_cast<int>(session_.selectedIndex()));
    chartCombo_->setEnabled(session_.chartCount() > 1);
    difficultyCombo_->setCurrentIndex(session_.difficulty().value_or(0));
    difficultyCombo_->setEnabled(session_.hasChart());
    chartEdit_->setText(session_.hasChart() ? QDir::toNativeSeparators(toQString(session_.chartPath())) : QString());
    outputEdit_->setText(session_.hasChart() ? QDir::toNativeSeparators(toQString(session_.outputPath())) : QString());
    applyingOptions_ = false;
}

std::vector<app::Message> MainWindow::allMessages() const {
    const app::ConversionSet& set = session_.conversionSet();
    std::vector<app::Message> messages = set.messages;
    for (const auto& conversion : set.charts) {
        for (const auto& message : conversion.messages) {
            app::Message shown = message;
            if (set.charts.size() > 1) {
                shown.text = "[" + infra::pathToUtf8(conversion.chartPath.filename()) + "] " + shown.text;
            }
            messages.push_back(std::move(shown));
        }
    }
    return messages;
}

void MainWindow::updateView() {
    const app::Conversion& conversion = session_.conversion();
    if (!session_.hasChart()) {
        messages_->clear();
        appendMessage(app::Severity::Info,
                      QStringLiteral("bmson を開いてください（ファイル → bmson を開く、またはウィンドウにドロップ）"));
    } else {
        showMessages(allMessages());
    }

    if (conversion.document) {
        const auto& usage = conversion.document->usage;
        const core::KeyMode* mode = core::findKeyMode(conversion.chart.info.modeHint);
        const QString wav = usage.wavDefined == usage.wav
                                ? QString::number(usage.wav)
                                : QStringLiteral("%1（ID は %2 まで）").arg(usage.wavDefined).arg(toQString(core::formatObjectId(usage.wav, conversion.document->base)));
        summaryLabel_->setText(QStringLiteral("%9　スライス %1 個（パラ音 %2 / BGM 音 %3）　%4進数　#WAV %5・#BMP %6・#BPM %7・#STOP %8")
                                   .arg(conversion.plan.slices.size())
                                   .arg(conversion.plan.countOf(core::SliceKind::Key))
                                   .arg(conversion.plan.countOf(core::SliceKind::Bgm))
                                   .arg(conversion.document->base == core::IdBase::Base62 ? 62 : 36)
                                   .arg(wav)
                                   .arg(usage.bmp)
                                   .arg(usage.bpm)
                                   .arg(usage.stop)
                                   .arg(mode != nullptr ? toQString(mode->name) : QString()));
    } else {
        summaryLabel_->setText(session_.hasChart() ? QStringLiteral("変換できません（下のメッセージを見てください）")
                                                   : QString());
    }
    updateSliceTable();
    updatePatternExamples();

    const bool hasSlices = conversion.document && !conversion.plan.slices.empty();
    const bool fade = edgeCombo_->currentIndex() == 1;
    qualitySpin_->setEnabled(formatCombo_->currentIndex() == 1);
    fadeOutSpin_->setEnabled(fade);
    fadeInCheck_->setEnabled(fade);
    fadeInSpin_->setEnabled(fade && fadeInCheck_->isChecked());
    exportButton_->setEnabled(!exporting_ && session_.canExport());
    launchButton_->setEnabled(!exporting_ && session_.exported());
    playSliceButton_->setEnabled(!exporting_ && hasSlices);
    playSongButton_->setEnabled(!exporting_ && hasSlices);
    if (!session_.exported() && previewCombo_->currentIndex() == 1) {
        previewCombo_->setCurrentIndex(0);
    }
    if (auto* model = qobject_cast<QStandardItemModel*>(previewCombo_->model())) {
        model->item(1)->setEnabled(session_.exported());
    }
}

void MainWindow::updatePatternExamples() {
    const app::NamingResult checked = app::checkNamingPatterns(optionsFromWidgets().naming);
    const app::Conversion& conversion = session_.conversion();
    struct Target {
        QLabel* label;
        const char* keyword;
        int kind;
    };
    const Target targets[] = {{folderExample_, "サブフォルダ", -1},
                              {keyExample_, "パラ音のファイル名", static_cast<int>(core::SliceKind::Key)},
                              {bgmExample_, "BGM 音のファイル名", static_cast<int>(core::SliceKind::Bgm)}};
    for (const auto& target : targets) {
        const auto error = std::find_if(checked.messages.begin(), checked.messages.end(), [&](const app::Message& message) {
            return message.text.find(target.keyword) != std::string::npos;
        });
        if (error != checked.messages.end()) {
            setExample(target.label, toQString(error->text), true);
            continue;
        }
        QString example;
        if (target.kind < 0) {
            if (conversion.document) {
                example = conversion.soundFolder.empty()
                              ? QStringLiteral("（BMS と同じフォルダ）")
                              : toQString(conversion.soundFolder) + QStringLiteral("/（#WAV にはフォルダを付けない）");
            }
            setExample(target.label, example.isEmpty() ? QString() : QStringLiteral("例: ") + example, false);
            continue;
        }
        for (std::size_t index = 0; index < conversion.soundFiles.size(); ++index) {
            if (static_cast<int>(conversion.plan.slices[index].kind) == target.kind) {
                example = toQString(conversion.soundFiles[index]);
                break;
            }
        }
        setExample(target.label, example.isEmpty() ? QString() : QStringLiteral("例: ") + example, false);
    }
}

void MainWindow::updateSliceTable() {
    const app::Conversion& conversion = session_.conversion();
    const int previous = sliceTable_->currentRow();
    sliceTable_->setUpdatesEnabled(false);
    sliceTable_->clearContents();
    if (!conversion.document) {
        sliceTable_->setRowCount(0);
        sliceTable_->setUpdatesEnabled(true);
        return;
    }
    const auto& slices = conversion.plan.slices;
    const app::ConversionSet& set = session_.conversionSet();
    const bool several = set.charts.size() > 1;
    const core::MeasureLayout layout(conversion.chart.info.resolution, conversion.chart.lines);
    sliceTable_->setColumnHidden(ColumnCharts, !several);
    sliceTable_->setRowCount(static_cast<int>(slices.size()));
    for (std::size_t index = 0; index < slices.size(); ++index) {
        const core::Slice& slice = slices[index];
        const auto row = static_cast<int>(index);
        QStringList cells = {
            toQString(core::formatObjectId(row + 1, conversion.document->base)),
            slice.kind == core::SliceKind::Key ? QStringLiteral("パラ音") : QStringLiteral("BGM"),
            toQString(app::soundFileOnDisk(conversion, index)),
            toQString(slice.source),
            seconds(slice.startSeconds()),
            seconds(slice.lengthSeconds()),
            QString::number(layout.locate(slice.firstPulse).measure),
            QString()};
        const std::vector<std::size_t> noUsers;
        const auto& users = index < set.sliceUsers.size() ? set.sliceUsers[index] : noUsers;
        QStringList chartNumbers;
        for (const std::size_t chart : users) {
            chartNumbers << QString::number(chart + 1);
        }
        cells[ColumnCharts] = chartNumbers.join(QLatin1Char(','));
        const bool outsideBase = several && std::find(users.begin(), users.end(), set.baseIndex) == users.end();
        const bool unused = several && std::find(users.begin(), users.end(), session_.selectedIndex()) == users.end();
        for (int column = 0; column < ColumnCount; ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            if (column == ColumnStart || column == ColumnLength || column == ColumnMeasure) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            // Why not 選んだ譜面のスライスだけを並べる: ID は譜面セットで共通なので、全体を並べたほうが ID と行が対応して見やすい
            if (outsideBase) {
                item->setBackground(kOutsideBaseBackground);
                item->setToolTip(QStringLiteral("基準の譜面にないスライス（追加で切り出す）"));
            }
            if (unused) {
                item->setForeground(QColor(0x99, 0x99, 0x99));
            }
            sliceTable_->setItem(row, column, item);
        }
    }
    sliceTable_->resizeColumnToContents(ColumnId);
    sliceTable_->resizeColumnToContents(ColumnKind);
    if (previous >= 0 && previous < sliceTable_->rowCount()) {
        sliceTable_->selectRow(previous);
    }
    sliceTable_->setUpdatesEnabled(true);
}

void MainWindow::showMessages(const std::vector<app::Message>& messages) {
    messages_->clear();
    for (const auto& message : messages) {
        appendMessage(message.severity, toQString(message.text));
    }
}

void MainWindow::appendMessage(app::Severity severity, const QString& text) {
    static const char* const kLabels[] = {"エラー", "警告", "情報"};
    auto* item = new QListWidgetItem(QStringLiteral("%1: %2").arg(QString::fromUtf8(kLabels[static_cast<int>(severity)]), text),
                                     messages_);
    if (severity == app::Severity::Error) {
        item->setForeground(QColor(0xc0, 0x39, 0x2b));
    } else if (severity == app::Severity::Warning) {
        item->setForeground(QColor(0xb9, 0x77, 0x0e));
    }
    messages_->scrollToItem(item);
}

void MainWindow::setBusy(bool busy) {
    exporting_ = busy;
    optionsPanel_->setEnabled(!busy);
    menuBar()->setEnabled(!busy);
    chartEdit_->parentWidget()->setEnabled(!busy);
    progress_->setVisible(busy);
    updateView();
}

void MainWindow::chooseChart() {
    auto settings = openSettings();
    const QString start = settings->value(QStringLiteral("paths/lastDirectory")).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("bmson を開く（複数選ぶと譜面セット）"), start,
                                                            QStringLiteral("bmson (*.bmson);;すべてのファイル (*)"));
    std::vector<std::filesystem::path> paths;
    for (const QString& file : files) {
        paths.push_back(toPath(file));
    }
    openCharts(paths);
}

void MainWindow::chooseOutput() {
    const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("書き出す BMS"), outputEdit_->text(),
                                                      QStringLiteral("BMS (*.bme *.bms *.pms)"));
    if (!file.isEmpty()) {
        outputEdit_->setText(QDir::toNativeSeparators(file));
        session_.setOutputPath(toPath(file));
        updateView();
    }
}

void MainWindow::setOverwriteConfirmation(std::function<bool(const QString&)> confirm) {
    confirmOverwrite_ = std::move(confirm);
}

void MainWindow::setChartSetChooser(ChartSetChooser chooser) {
    chooseChartSet_ = std::move(chooser);
}

void MainWindow::startExport() {
    refreshNow();
    if (exporting_ || !session_.canExport()) {
        return;
    }
    QStringList existing;
    for (const auto& output : session_.outputPaths()) {
        if (QFileInfo::exists(toQString(output))) {
            existing << QDir::toNativeSeparators(toQString(output));
        }
    }
    if (!existing.isEmpty()) {
        const QString question = QStringLiteral("%1 はすでにあります。上書きしますか？\n（音声ファイルも同じ名前のものは上書きされます）")
                                     .arg(existing.join(QStringLiteral("\n")));
        const bool overwrite = confirmOverwrite_ ? confirmOverwrite_(question)
                                                 : QMessageBox::question(this, QStringLiteral("上書きの確認"), question) ==
                                                       QMessageBox::Yes;
        if (!overwrite) {
            return;
        }
    }
    player_.stop();
    progress_->setRange(0, static_cast<int>(session_.conversion().plan.slices.size()));
    progress_->setValue(0);
    setBusy(true);
    QProgressBar* bar = progress_;
    // Why not GUI のスレッドで書き出す: 数千ファイルを書くと数十秒かかり、その間ウィンドウが固まって進み具合も見えないため
    exportWatcher_->setFuture(QtConcurrent::run([this, bar] {
        return session_.exportAll([bar](std::size_t done, std::size_t) {
            QMetaObject::invokeMethod(bar, [bar, done] { bar->setValue(static_cast<int>(done)); }, Qt::QueuedConnection);
        });
    }));
}

bool MainWindow::isExporting() const {
    return exporting_;
}

void MainWindow::waitForExport() {
    exportWatcher_->waitForFinished();
    while (exporting_) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

void MainWindow::finishExport() {
    const app::ExportAllOutcome outcome = exportWatcher_->result();
    setBusy(false);
    for (const auto& message : outcome.messages) {
        appendMessage(message.severity, toQString(message.text));
    }
    if (outcome.succeeded) {
        for (const auto& output : session_.outputPaths()) {
            appendMessage(app::Severity::Info,
                          QStringLiteral("BMS を書き出しました: %1").arg(QDir::toNativeSeparators(toQString(output))));
        }
        previewCombo_->setCurrentIndex(1);
        statusBar()->showMessage(QStringLiteral("書き出しました: %1").arg(QDir::toNativeSeparators(toQString(session_.outputPath()))));
    }
}

app::PreviewSource MainWindow::previewSource() const {
    return previewCombo_->currentIndex() == 1 ? app::PreviewSource::Exported : app::PreviewSource::Planned;
}

void MainWindow::play(const app::PreviewResult& result) {
    if (!result.audio) {
        appendMessage(app::Severity::Error, QStringLiteral("試聴できません: ") + toQString(result.error));
        return;
    }
    if (!player_.start(*result.audio)) {
        appendMessage(app::Severity::Error, QStringLiteral("音を再生できません（音声出力デバイスを開けません）"));
    }
}

void MainWindow::playSelectedSlice() {
    const int row = sliceTable_->currentRow();
    if (row < 0) {
        statusBar()->showMessage(QStringLiteral("一覧からスライスを選んでください"), 3000);
        return;
    }
    play(session_.previewSlice(static_cast<std::size_t>(row), previewSource()));
}

void MainWindow::playSong() {
    const WaitCursor wait;
    play(session_.previewSong(previewSource(), player_.sampleRate()));
}

void MainWindow::stopPlayback() {
    player_.stop();
}

void MainWindow::launchPlayer() {
    const app::LaunchOutcome outcome = session_.launchPlayer(playerSettings_, launcher_);
    if (!outcome.launched) {
        appendMessage(app::Severity::Error, toQString(outcome.error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("外部プレイヤーを起動しました: %1").arg(toQString(outcome.command.program)), 5000);
}

void MainWindow::editPlayerSettings() {
    PlayerSettingsDialog dialog(playerSettings_, this);
    if (dialog.exec() == QDialog::Accepted) {
        playerSettings_ = dialog.settings();
        auto settings = openSettings();
        savePlayerSettings(*settings, playerSettings_);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (exporting_) {
        statusBar()->showMessage(QStringLiteral("書き出しが終わるまでお待ちください"), 3000);
        event->ignore();
        return;
    }
    player_.stop();
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const auto urls = event->mimeData()->urls();
    const bool allCharts = std::all_of(urls.begin(), urls.end(), [](const QUrl& url) {
        return url.toLocalFile().endsWith(QStringLiteral(".bmson"), Qt::CaseInsensitive);
    });
    if (!exporting_ && !urls.isEmpty() && allCharts) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    std::vector<std::filesystem::path> paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        paths.push_back(toPath(url.toLocalFile()));
    }
    openCharts(paths);
}

}  // namespace b2b::gui
