#include <gtest/gtest.h>

#include "b2b/infra/text/TextEncoding.hpp"

using namespace b2b::infra;

TEST(TextEncoding, ENC001_AsciiIsUnchanged) {
    const auto encoded = encodeText("#TITLE abc", TextEncoding::ShiftJis);
    EXPECT_EQ(encoded.bytes, "#TITLE abc");
    EXPECT_TRUE(encoded.unrepresentable.empty());
}

TEST(TextEncoding, ENC002_JapaneseBecomesShiftJis) {
    const auto encoded = encodeText("テスト", TextEncoding::ShiftJis);
    EXPECT_EQ(encoded.bytes, "\x83\x65\x83\x58\x83\x67");
    EXPECT_TRUE(encoded.unrepresentable.empty());
}

TEST(TextEncoding, ENC003_UnrepresentableCharacterIsReplacedAndReported) {
    const auto encoded = encodeText("A🎵B", TextEncoding::ShiftJis);
    EXPECT_EQ(encoded.bytes, "A?B");
    EXPECT_EQ(encoded.unrepresentable, (std::vector<std::string>{"🎵"}));
}

TEST(TextEncoding, ENC004_Utf8IsWrittenAsIs) {
    const auto encoded = encodeText("テスト🎵", TextEncoding::Utf8);
    EXPECT_EQ(encoded.bytes, "テスト🎵");
    EXPECT_TRUE(encoded.unrepresentable.empty());
}

TEST(TextEncoding, ENC005_BrokenUtf8IsReplacedAndReported) {
    const auto encoded = encodeText("A\xFF" "B", TextEncoding::ShiftJis);
    EXPECT_EQ(encoded.bytes, "A?B");
    EXPECT_EQ(encoded.unrepresentable.size(), 1u);
}

TEST(TextEncoding, ENC006_UnrepresentableCharactersCanBeReplacedInPlace) {
    const auto shiftJis = replaceUnrepresentable("曲🎵名", TextEncoding::ShiftJis, '_');
    EXPECT_EQ(shiftJis.text, "曲_名");
    EXPECT_EQ(shiftJis.replaced, 1);
    const auto utf8 = replaceUnrepresentable("曲🎵名", TextEncoding::Utf8, '_');
    EXPECT_EQ(utf8.text, "曲🎵名");
    EXPECT_EQ(utf8.replaced, 0);
}
