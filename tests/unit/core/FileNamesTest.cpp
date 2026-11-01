#include <gtest/gtest.h>

#include "b2b/core/naming/FileNames.hpp"

using namespace b2b::core;

TEST(FileNames, FNM001_ForbiddenCharactersBecomeUnderscores) {
    EXPECT_EQ(sanitizeFileName("a\\b/c:d*e?f\"g<h>i|j"), "a_b_c_d_e_f_g_h_i_j");
}

TEST(FileNames, FNM002_TrailingDotsAndReservedNames) {
    EXPECT_EQ(sanitizeFileName("name. ."), "name");
    EXPECT_EQ(sanitizeFileName("CON"), "CON_");
    EXPECT_EQ(sanitizeFileName("nul"), "nul_");
}

TEST(FileNames, FNM003_LongNamesAreTruncatedWithAWarning) {
    const auto names = finishSoundNames({std::string(70, 'a')}, "", "wav");
    EXPECT_EQ(names.paths.front(), std::string(64, 'a') + ".wav");
    EXPECT_EQ(names.warnings.size(), 1u);
}

TEST(FileNames, FNM004_CollisionsIgnoreCase) {
    const auto names = finishSoundNames({"Aa", "aA", "Aa"}, "", "wav");
    EXPECT_EQ(names.paths, (std::vector<std::string>{"Aa.wav", "aA_2.wav", "Aa_3.wav"}));
    EXPECT_EQ(names.warnings.size(), 1u);
}

TEST(FileNames, FNM005_EmptyNameIsAnError) {
    EXPECT_FALSE(finishSoundNames({"..."}, "", "wav").errors.empty());
}

TEST(FileNames, FNM006_FolderAndExtension) {
    EXPECT_EQ(finishSoundNames({"x"}, "wav", "ogg").paths.front(), "wav/x.ogg");
    EXPECT_EQ(finishSoundNames({"x"}, "", "ogg").paths.front(), "x.ogg");
}

TEST(FileNames, FNM007_StemDropsFoldersAndTheLastExtension) {
    EXPECT_EQ(fileStemOf("sounds/drums.final.wav"), "drums.final");
    EXPECT_EQ(fileStemOf("C:\\a\\b.flac"), "b");
    EXPECT_EQ(fileStemOf(".hidden"), ".hidden");
}
