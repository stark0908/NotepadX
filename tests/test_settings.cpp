#include <gtest/gtest.h>
#include "config/Settings.h"

#include <filesystem>

using namespace notepadx;

class SettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "notepadx_test_settings";
        std::error_code ec;
        std::filesystem::remove_all(testDir_, ec);
        std::filesystem::create_directories(testDir_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(testDir_, ec);
    }

    std::filesystem::path testDir_;
};

TEST_F(SettingsTest, DefaultValuesValid) {
    Settings s;
    EXPECT_FALSE(s.fontName().empty());
    EXPECT_GE(s.fontSizePt(), 8);
    EXPECT_EQ(s.tabWidth(), 4);
    EXPECT_FALSE(s.useTabs());
    EXPECT_TRUE(s.showLineNumbers());
    EXPECT_FALSE(s.wordWrap());
    EXPECT_EQ(s.theme(), "dark");
    EXPECT_TRUE(s.isDarkTheme());
}

TEST_F(SettingsTest, SaveAndLoadRoundTrip) {
    const auto file = testDir_ / "settings.json";

    Settings s1;
    s1.setFontName("Fira Code");
    s1.setFontSizePt(14);
    s1.setTabWidth(8);
    s1.setUseTabs(true);
    s1.setShowLineNumbers(false);
    s1.setWordWrap(true);
    s1.setTheme("light");

    EXPECT_TRUE(s1.saveToFile(file));
    EXPECT_TRUE(std::filesystem::exists(file));

    Settings s2;
    EXPECT_TRUE(s2.loadFromFile(file));

    EXPECT_EQ(s2.fontName(), "Fira Code");
    EXPECT_EQ(s2.fontSizePt(), 14);
    EXPECT_EQ(s2.tabWidth(), 8);
    EXPECT_TRUE(s2.useTabs());
    EXPECT_FALSE(s2.showLineNumbers());
    EXPECT_TRUE(s2.wordWrap());
    EXPECT_EQ(s2.theme(), "light");
    EXPECT_FALSE(s2.isDarkTheme());
}
