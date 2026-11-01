#pragma once

#include <QDialog>

#include "b2b/app/PreviewService.hpp"

class QLineEdit;

namespace b2b::gui {

class PlayerSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    PlayerSettingsDialog(const app::PlayerSettings& settings, QWidget* parent = nullptr);

    [[nodiscard]] app::PlayerSettings settings() const;

private:
    QLineEdit* executable_;
    QLineEdit* commandLine_;
    QLineEdit* workingDirectory_;
};

}  // namespace b2b::gui
