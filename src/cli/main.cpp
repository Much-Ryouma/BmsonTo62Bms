#include <exception>
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

#include "Commands.hpp"
#include "b2b/core/BuildInfo.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

int main(int argc, char** argv) {
    using b2b::cli::ExitCode;
    using b2b::cli::toInt;

#if defined(_WIN32)
    // Why not コンソールの既定のコードページのまま出力する: メッセージは UTF-8 なので、CP932 のコンソールでは文字化けするため
    SetConsoleOutputCP(CP_UTF8);
#endif

    const std::string versionText = std::string(b2b::core::kProductName) + " " + std::string(b2b::core::kVersion);
    CLI::App app{versionText + " command line tool", "bmsonto62bms-cli"};
    // Why not argv をそのまま使う: Windows ではシステムのコードページで渡され、日本語のパスが壊れるため UTF-8 に直す
    argv = app.ensure_utf8(argv);
    app.set_version_flag("--version", versionText);
    app.require_subcommand(0, 1);

    b2b::cli::CommandResult result;
    b2b::cli::registerSlicesCommand(app, result);
    b2b::cli::registerConvertCommand(app, result);

    try {
        app.parse(argc, argv);
    } catch (const CLI::Success& success) {
        return app.exit(success);
    } catch (const CLI::ParseError& error) {
        app.exit(error);
        return toInt(ExitCode::UsageError);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return toInt(ExitCode::ConversionError);
    }

    if (app.get_subcommands().empty()) {
        std::cout << app.help();
    }
    return result.exitCode;
}
