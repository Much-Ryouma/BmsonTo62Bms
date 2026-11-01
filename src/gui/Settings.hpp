#pragma once

#include <memory>

#include <QSettings>
#include <QString>

#include "b2b/app/ConvertService.hpp"
#include "b2b/app/PreviewService.hpp"

namespace b2b::gui {

inline const QString kSettingsFileName = QStringLiteral("bmsonto62bms.ini");

[[nodiscard]] QString settingsFile();
void setSettingsFile(const QString& file);
[[nodiscard]] std::unique_ptr<QSettings> openSettings();

[[nodiscard]] app::ConvertOptions loadConvertOptions(QSettings& settings);
void saveConvertOptions(QSettings& settings, const app::ConvertOptions& options);

[[nodiscard]] app::PlayerSettings loadPlayerSettings(QSettings& settings);
void savePlayerSettings(QSettings& settings, const app::PlayerSettings& player);

}  // namespace b2b::gui
