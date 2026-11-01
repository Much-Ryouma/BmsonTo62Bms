#include "Settings.hpp"

#include <QCoreApplication>
#include <QDir>

#include "QtText.hpp"

namespace b2b::gui {

namespace {

QString& overriddenFile() {
    static QString file;
    return file;
}

QString text(QSettings& settings, const char* key, const std::string& fallback) {
    return settings.value(QString::fromLatin1(key), toQString(fallback)).toString();
}

}  // namespace

// Why not OS のユーザー設定（レジストリ）に保存する: zip を展開して使うだけのアプリなので、フォルダを消せば
// 何も残らないようにし、困ったときは利用者がテキストエディタで直せるよう exe の横の ini に置く（IgusaBmsonStudio と同じ）
QString settingsFile() {
    if (!overriddenFile().isEmpty()) {
        return overriddenFile();
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(kSettingsFileName);
}

void setSettingsFile(const QString& file) {
    overriddenFile() = file;
}

std::unique_ptr<QSettings> openSettings() {
    return std::make_unique<QSettings>(settingsFile(), QSettings::IniFormat);
}

app::ConvertOptions loadConvertOptions(QSettings& settings) {
    const app::ConvertOptions defaults;
    app::ConvertOptions options;
    options.encoding = settings.value(QStringLiteral("convert/encoding")).toString() == QStringLiteral("utf8")
                           ? infra::TextEncoding::Utf8
                           : infra::TextEncoding::ShiftJis;
    const int difficulty = settings.value(QStringLiteral("convert/difficulty"), 0).toInt();
    if (difficulty >= 1 && difficulty <= 5) {
        options.difficulty = difficulty;
    }
    options.soundFormat = settings.value(QStringLiteral("sound/format")).toString() == QStringLiteral("ogg")
                              ? app::SoundFormat::Ogg
                              : app::SoundFormat::Wav;
    options.oggQuality = std::clamp(settings.value(QStringLiteral("sound/oggQuality"), defaults.oggQuality).toInt(), 0, 10);
    options.naming.folderPattern = toUtf8(text(settings, "naming/folder", defaults.naming.folderPattern));
    options.naming.keyPattern = toUtf8(text(settings, "naming/key", defaults.naming.keyPattern));
    options.naming.bgmPattern = toUtf8(text(settings, "naming/bgm", defaults.naming.bgmPattern));
    options.fade.mode = settings.value(QStringLiteral("edge/mode")).toString() == QStringLiteral("fade")
                            ? core::EdgeMode::Fade
                            : core::EdgeMode::Cut;
    options.fade.fadeOutMs = settings.value(QStringLiteral("edge/fadeOutMs"), defaults.fade.fadeOutMs).toDouble();
    options.fade.fadeIn = settings.value(QStringLiteral("edge/fadeIn"), defaults.fade.fadeIn).toBool();
    options.fade.fadeInMs = settings.value(QStringLiteral("edge/fadeInMs"), defaults.fade.fadeInMs).toDouble();
    return options;
}

void saveConvertOptions(QSettings& settings, const app::ConvertOptions& options) {
    settings.setValue(QStringLiteral("convert/encoding"),
                      options.encoding == infra::TextEncoding::Utf8 ? QStringLiteral("utf8") : QStringLiteral("sjis"));
    settings.setValue(QStringLiteral("convert/difficulty"), options.difficulty.value_or(0));
    settings.setValue(QStringLiteral("sound/format"),
                      options.soundFormat == app::SoundFormat::Ogg ? QStringLiteral("ogg") : QStringLiteral("wav"));
    settings.setValue(QStringLiteral("sound/oggQuality"), options.oggQuality);
    settings.setValue(QStringLiteral("naming/folder"), toQString(options.naming.folderPattern));
    settings.setValue(QStringLiteral("naming/key"), toQString(options.naming.keyPattern));
    settings.setValue(QStringLiteral("naming/bgm"), toQString(options.naming.bgmPattern));
    settings.setValue(QStringLiteral("edge/mode"),
                      options.fade.mode == core::EdgeMode::Fade ? QStringLiteral("fade") : QStringLiteral("cut"));
    settings.setValue(QStringLiteral("edge/fadeOutMs"), options.fade.fadeOutMs);
    settings.setValue(QStringLiteral("edge/fadeIn"), options.fade.fadeIn);
    settings.setValue(QStringLiteral("edge/fadeInMs"), options.fade.fadeInMs);
}

app::PlayerSettings loadPlayerSettings(QSettings& settings) {
    const app::PlayerSettings defaults;
    app::PlayerSettings player;
    player.executable = toUtf8(text(settings, "player/executable", defaults.executable));
    player.commandTemplate = toUtf8(text(settings, "player/commandLine", defaults.commandTemplate));
    player.workingDirectory = toUtf8(text(settings, "player/workingDirectory", defaults.workingDirectory));
    return player;
}

void savePlayerSettings(QSettings& settings, const app::PlayerSettings& player) {
    // Why not \ 区切りのまま保存する: QSettings の ini では \ がエスケープ文字として扱われ、手で直すときに読みにくいため / にそろえる
    settings.setValue(QStringLiteral("player/executable"), QDir::fromNativeSeparators(toQString(player.executable)));
    settings.setValue(QStringLiteral("player/commandLine"), toQString(player.commandTemplate));
    settings.setValue(QStringLiteral("player/workingDirectory"),
                      QDir::fromNativeSeparators(toQString(player.workingDirectory)));
}

}  // namespace b2b::gui
