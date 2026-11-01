#include <gtest/gtest.h>

#include "b2b/core/player/LaunchCommand.hpp"

using namespace b2b::core;

TEST(LaunchCommand, CMD001_QuotesKeepSpaces) {
    EXPECT_EQ(splitCommandLine(R"(a "b c" d)"), (std::vector<std::string>{"a", "b c", "d"}));
}

TEST(LaunchCommand, CMD002_DefaultTemplate) {
    const auto command = buildLaunchCommand(kDefaultLaunchTemplate, "C:/My Player/play.exe", "D:/曲 A/song.bme");
    EXPECT_EQ(command.program, "C:/My Player/play.exe");
    EXPECT_EQ(command.arguments, (std::vector<std::string>{"D:/曲 A/song.bme"}));
}

TEST(LaunchCommand, CMD003_UnquotedFileWithSpacesStaysOneArgument) {
    const auto command = buildLaunchCommand(R"("{exe}" -a -s {file})", "play", "D:/a b/song.bme");
    EXPECT_EQ(command.arguments, (std::vector<std::string>{"-a", "-s", "D:/a b/song.bme"}));
}

TEST(LaunchCommand, CMD004_ExecutableCanBeAnArgument) {
    const auto command = buildLaunchCommand(R"(java -jar "{exe}" {file})", "C:/b/beatoraja.jar", "x.bme");
    EXPECT_EQ(command.program, "java");
    EXPECT_EQ(command.arguments, (std::vector<std::string>{"-jar", "C:/b/beatoraja.jar", "x.bme"}));
}

TEST(LaunchCommand, CMD005_EmptyTemplateHasNoProgram) {
    EXPECT_TRUE(buildLaunchCommand("", "play", "x.bme").program.empty());
}
