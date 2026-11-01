#pragma once

#include <optional>
#include <vector>

#include <QDialog>

#include "b2b/app/ConvertService.hpp"

class QButtonGroup;
class QComboBox;
class QTableWidget;

namespace b2b::gui {

struct ChartSetChoice {
    std::size_t baseIndex = 0;
    std::vector<std::optional<int>> difficulties;
};

[[nodiscard]] ChartSetChoice defaultChartSetChoice(const std::vector<app::ChartSummary>& charts);

class ChartSetDialog final : public QDialog {
    Q_OBJECT

public:
    ChartSetDialog(const std::vector<app::ChartSummary>& charts, QWidget* parent = nullptr);

    [[nodiscard]] ChartSetChoice choice() const;

private:
    QTableWidget* table_ = nullptr;
    QButtonGroup* baseGroup_ = nullptr;
    std::vector<QComboBox*> difficultyCombos_;
};

void fillDifficultyCombo(QComboBox& combo);

}  // namespace b2b::gui
