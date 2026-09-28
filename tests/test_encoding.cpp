#include <gtest/gtest.h>
#include "platform/Encoding.h"

using namespace notepadx;

TEST(EncodingTest, DetectValidUtf8) {
    const std::string text = "Hello, World! 🚀 Привет, мир!";
    auto det = Encoding::detect(text);
    EXPECT_EQ(det.type, EncodingType::Utf8);
    EXPECT_EQ(det.name, "UTF-8");
    EXPECT_EQ(det.bomLength, 0);
    EXPECT_TRUE(Encoding::isValidUtf8(text));
}

TEST(EncodingTest, DetectUtf8Bom) {
    const std::string text = "\xEF\xBB\xBFHello with BOM";
    auto det = Encoding::detect(text);
    EXPECT_EQ(det.type, EncodingType::Utf8Bom);
    EXPECT_EQ(det.name, "UTF-8 BOM");
    EXPECT_EQ(det.bomLength, 3);

    std::string stripped = Encoding::toUtf8(text, det.type);
    EXPECT_EQ(stripped, "Hello with BOM");

    std::string restored = Encoding::fromUtf8(stripped, det.type);
    EXPECT_EQ(restored, text);
}

TEST(EncodingTest, DetectUtf16Le) {
    // "Hi" in UTF-16LE with BOM: FF FE 48 00 69 00
    const std::string raw = std::string("\xFF\xFE\x48\x00\x69\x00", 6);
    auto det = Encoding::detect(raw);
    EXPECT_EQ(det.type, EncodingType::Utf16Le);
    EXPECT_EQ(det.name, "UTF-16 LE");

    std::string utf8 = Encoding::toUtf8(raw, det.type);
    EXPECT_EQ(utf8, "Hi");
}

TEST(EncodingTest, DetectUtf16Be) {
    // "Hi" in UTF-16BE with BOM: FE FF 00 48 00 69
    const std::string raw = std::string("\xFE\xFF\x00\x48\x00\x69", 6);
    auto det = Encoding::detect(raw);
    EXPECT_EQ(det.type, EncodingType::Utf16Be);
    EXPECT_EQ(det.name, "UTF-16 BE");

    std::string utf8 = Encoding::toUtf8(raw, det.type);
    EXPECT_EQ(utf8, "Hi");
}

TEST(EncodingTest, DetectLatin1FallbackAndConvert) {
    // Byte 0xE9 is 'é' in ISO-8859-1, but invalid lone continuation byte in UTF-8
    const std::string latin1 = "Caf\xE9";
    auto det = Encoding::detect(latin1);
    EXPECT_EQ(det.type, EncodingType::Latin1);
    EXPECT_EQ(det.name, "ISO-8859-1");

    std::string utf8 = Encoding::toUtf8(latin1, det.type);
    EXPECT_EQ(utf8, "Café");

    std::string back = Encoding::fromUtf8(utf8, det.type);
    EXPECT_EQ(back, latin1);
}
