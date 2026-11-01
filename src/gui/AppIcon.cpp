#include "AppIcon.hpp"

#include <algorithm>

#include <QColor>
#include <QFile>
#include <QFont>
#include <QPainter>

namespace b2b::gui {

namespace {

// Why not アイコンがないときは空にする: アイコンは後から用意するため、それまでもバージョン情報や
// タスクバーで何のアプリか分かるよう、文字だけの仮のアイコンを描く（IgusaBmsonStudio と同じ）
QPixmap placeholder(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x2c, 0x3e, 0x50));
    painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.18, size * 0.18);
    painter.setBrush(QColor(0xe6, 0x7e, 0x22));
    painter.drawRect(QRectF(size * 0.12, size * 0.74, size * 0.76, size * 0.06));
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::max(6, size * 40 / 100));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, size, size * 0.74), Qt::AlignCenter, QStringLiteral("62"));
    return pixmap;
}

}  // namespace

bool hasCustomAppIcon() {
    return QFile::exists(kAppIconResource);
}

QPixmap appIconPixmap(int size) {
    if (hasCustomAppIcon()) {
        const QPixmap custom(kAppIconResource);
        if (!custom.isNull()) {
            return custom.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    return placeholder(size);
}

QIcon appIcon() {
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addPixmap(appIconPixmap(size));
    }
    return icon;
}

}  // namespace b2b::gui
