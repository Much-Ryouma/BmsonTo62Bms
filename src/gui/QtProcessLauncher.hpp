#pragma once

#include <QProcess>
#include <QStringList>

#include "QtText.hpp"
#include "b2b/app/Ports.hpp"

namespace b2b::gui {

class QtProcessLauncher final : public app::ProcessLauncher {
public:
    bool launch(const std::string& program, const std::vector<std::string>& arguments,
                const std::filesystem::path& workingDirectory) override {
        QStringList list;
        for (const auto& argument : arguments) {
            list << toQString(argument);
        }
        // Why not QProcess を待つ: プレイヤーは独立したウィンドウとして遊ぶものなので、終了を待たずに切り離して起動する
        return QProcess::startDetached(toQString(program), list, toQString(workingDirectory));
    }
};

}  // namespace b2b::gui
