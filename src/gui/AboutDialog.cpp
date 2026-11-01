#include "AboutDialog.hpp"

#include <QMessageBox>

#include "AppIcon.hpp"
#include "QtText.hpp"
#include "b2b/core/BuildInfo.hpp"

namespace b2b::gui {

void showAboutDialog(QWidget* parent) {
    const QString text =
        QStringLiteral("<h3>bmsonto62bms %1</h3>"
                       "<p>bmson の譜面を、62進数に対応した拡張 BMS に変換します。</p>"
                       "<p><a href=\"%2\">%2</a></p>"
                       "<p>ライセンス: MIT<br>"
                       "使用しているライブラリ: Qt (LGPL v3)、miniaudio、stb_vorbis、libogg・libvorbis、nlohmann/json</p>")
            .arg(toQString(core::kVersion), toQString(core::kHomepageUrl));
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("バージョン情報"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.setIconPixmap(appIconPixmap(96));
    box.exec();
}

}  // namespace b2b::gui
