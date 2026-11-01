#include "ChartSetDialog.hpp"

#include <algorithm>

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "QtText.hpp"

namespace b2b::gui {

namespace {

enum Column { ColumnBase, ColumnFile, ColumnTitle, ColumnChartName, ColumnLevel, ColumnKeyNotes, ColumnDifficulty, ColumnCount };

}  // namespace

void fillDifficultyCombo(QComboBox& combo) {
    combo.addItems({QStringLiteral("書かない"), QStringLiteral("1 BEGINNER"), QStringLiteral("2 NORMAL"),
                    QStringLiteral("3 HYPER"), QStringLiteral("4 ANOTHER"), QStringLiteral("5 INSANE")});
}

ChartSetChoice defaultChartSetChoice(const std::vector<app::ChartSummary>& charts) {
    ChartSetChoice choice;
    choice.baseIndex = app::defaultBaseIndex(charts);
    for (const auto& chart : charts) {
        choice.difficulties.push_back(app::guessDifficulty(chart.chartName));
    }
    return choice;
}

ChartSetDialog::ChartSetDialog(const std::vector<app::ChartSummary>& charts, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("譜面セット"));
    auto* layout = new QVBoxLayout(this);
    auto* explanation = new QLabel(QStringLiteral(
        "同じ曲の複数の難易度として、音声ファイルを共有して書き出します。\n"
        "基準の譜面の切り出し位置に、ほかの譜面を合わせます（ふつうは最も難しい譜面を選びます）。"), this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    const ChartSetChoice initial = defaultChartSetChoice(charts);
    table_ = new QTableWidget(static_cast<int>(charts.size()), ColumnCount, this);
    table_->setObjectName(QStringLiteral("charts"));
    table_->setHorizontalHeaderLabels({QStringLiteral("基準"), QStringLiteral("ファイル"), QStringLiteral("タイトル"),
                                       QStringLiteral("chart_name"), QStringLiteral("レベル"), QStringLiteral("鍵盤のノーツ"),
                                       QStringLiteral("#DIFFICULTY")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    baseGroup_ = new QButtonGroup(this);
    for (std::size_t index = 0; index < charts.size(); ++index) {
        const auto& chart = charts[index];
        const int row = static_cast<int>(index);
        auto* radio = new QRadioButton(table_);
        radio->setObjectName(QStringLiteral("base%1").arg(row));
        radio->setChecked(index == initial.baseIndex);
        baseGroup_->addButton(radio, row);
        auto* radioBox = new QWidget(table_);
        auto* radioLayout = new QHBoxLayout(radioBox);
        radioLayout->setContentsMargins(0, 0, 0, 0);
        radioLayout->addWidget(radio, 0, Qt::AlignCenter);
        table_->setCellWidget(row, ColumnBase, radioBox);

        const QStringList cells = {QString(), toQString(chart.path.filename()),
                                   chart.error ? toQString(*chart.error) : toQString(chart.title), toQString(chart.chartName),
                                   QString::number(chart.level), QString::number(chart.keyNotes)};
        for (int column = ColumnFile; column < ColumnDifficulty; ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            if (column == ColumnLevel || column == ColumnKeyNotes) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            table_->setItem(row, column, item);
        }

        auto* combo = new QComboBox(table_);
        combo->setObjectName(QStringLiteral("difficulty%1").arg(row));
        fillDifficultyCombo(*combo);
        combo->setCurrentIndex(initial.difficulties[index].value_or(0));
        table_->setCellWidget(row, ColumnDifficulty, combo);
        difficultyCombos_.push_back(combo);
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(ColumnTitle, QHeaderView::Stretch);
    layout->addWidget(table_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(820, 120 + 34 * static_cast<int>(charts.size()));
}

ChartSetChoice ChartSetDialog::choice() const {
    ChartSetChoice result;
    result.baseIndex = static_cast<std::size_t>(std::max(0, baseGroup_->checkedId()));
    for (const QComboBox* combo : difficultyCombos_) {
        result.difficulties.push_back(combo->currentIndex() > 0 ? std::optional<int>(combo->currentIndex()) : std::nullopt);
    }
    return result;
}

}  // namespace b2b::gui
