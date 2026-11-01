#include "QtTranslations.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace b2b::gui {

// Why not OS の言語に合わせる: アプリの文言はすべて日本語なので、OK / Cancel などの Qt 標準のボタンだけが
// 英語で混ざらないよう、常に日本語の訳を読む
bool installQtJapaneseTranslation(QCoreApplication& application) {
    auto* translator = new QTranslator(&application);
    const QLocale japanese(QLocale::Japanese, QLocale::Japan);
    for (const QString& directory : {QLibraryInfo::path(QLibraryInfo::TranslationsPath),
                                     QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"))}) {
        if (translator->load(japanese, QStringLiteral("qtbase"), QStringLiteral("_"), directory)) {
            return QCoreApplication::installTranslator(translator);
        }
    }
    delete translator;
    return false;
}

}  // namespace b2b::gui
