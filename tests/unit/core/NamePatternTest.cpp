#include <gtest/gtest.h>

#include "b2b/core/naming/NamePattern.hpp"

using namespace b2b::core;

namespace {

NameVariables variables() {
    NameVariables values;
    values.song = "とても長い曲のタイトル";
    values.source = "drums";
    values.id = "0a";
    values.lane = 3;
    values.measure = 12;
    values.index = 7;
    return values;
}

std::string render(const char* pattern, NameVariables values = variables()) {
    return NamePattern::parse(pattern, PatternScope::Slice).render(values);
}

}  // namespace

TEST(NamePattern, NMP001_DefaultStylePattern) {
    EXPECT_EQ(render("{song:.4}_k{index:04}"), "とても長_k0007");
}

TEST(NamePattern, NMP002_WidthPadsButNeverTruncates) {
    EXPECT_EQ(render("{index:3}"), "  7");
    EXPECT_EQ(render("{index}"), "7");
    auto big = variables();
    big.index = 123;
    EXPECT_EQ(render("{index:02}", big), "123");
}

TEST(NamePattern, NMP003_PrintfStyleSpecsAreAccepted) {
    EXPECT_EQ(render("{index:%04d}"), "0007");
    EXPECT_EQ(render("{song:%.2s}"), "とて");
}

TEST(NamePattern, NMP004_EveryVariable) {
    EXPECT_EQ(render("{src}-{lane}-{measure:03}-{id}"), "drums-3-012-0a");
}

TEST(NamePattern, NMP005_DoubledBracesAreLiteral) {
    EXPECT_EQ(render("{{x}}"), "{x}");
}

TEST(NamePattern, NMP006_MistakesAreErrors) {
    for (const char* pattern : {"{oops}", "{index", "a}b", "{index:.3}", "{song:04}", "{index:x}", "{index:099}"}) {
        EXPECT_THROW(static_cast<void>(NamePattern::parse(pattern, PatternScope::Slice)), PatternError) << pattern;
    }
}

TEST(NamePattern, NMP007_FolderAcceptsOnlySong) {
    EXPECT_EQ(NamePattern::parse("{song}", PatternScope::Folder).render(variables()), "とても長い曲のタイトル");
    EXPECT_EQ(NamePattern::parse("", PatternScope::Folder).render(variables()), "");
    EXPECT_THROW(static_cast<void>(NamePattern::parse("{index}", PatternScope::Folder)), PatternError);
}

TEST(NamePattern, NMP008_EmptyFileNamePatternIsAnError) {
    EXPECT_THROW(static_cast<void>(NamePattern::parse("", PatternScope::Slice)), PatternError);
}
