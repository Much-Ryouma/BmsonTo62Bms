#include <QApplication>
#include <QStringList>

#include "AppIcon.hpp"
#include "MainWindow.hpp"
#include "QtProcessLauncher.hpp"
#include "QtText.hpp"
#include "QtTranslations.hpp"
#include "b2b/app/ConverterSession.hpp"
#include "b2b/core/BuildInfo.hpp"
#include "b2b/infra/audio/MiniaudioPlayer.hpp"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("bmsonto62bms"));
    QApplication::setApplicationVersion(b2b::gui::toQString(b2b::core::kVersion));
    QApplication::setWindowIcon(b2b::gui::appIcon());
    b2b::gui::installQtJapaneseTranslation(application);

    const b2b::app::LocalChartSource charts;
    const b2b::app::LocalAudioSource audio;
    const b2b::app::LocalAudioCodec codec;
    const b2b::app::LocalFileSink files;
    b2b::app::ConverterSession session(charts, audio, codec, files);
    b2b::infra::MiniaudioPlayer player;
    b2b::gui::QtProcessLauncher launcher;

    b2b::gui::MainWindow window(b2b::gui::MainWindowPorts{session, player, launcher});
    window.show();
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) {
        window.openChart(b2b::gui::toPath(arguments.at(1)));
    }
    return QApplication::exec();
}
