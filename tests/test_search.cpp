#include <gtest/gtest.h>
#include "search/Regex.h"
#include "search/Search.h"

using namespace notepadx;

TEST(RegexEngineTest, PlainTextMatching) {
    RegexEngine re;
    std::string err;
    EXPECT_TRUE(re.compile("hello", false, false, false, &err));

    std::string text = "Hello world, HELLO friend, hello!";
    auto matches = re.matchAll(text);
    ASSERT_EQ(matches.size(), 3);
    EXPECT_EQ(matches[0].start, 0);
    EXPECT_EQ(matches[0].end, 5);
    EXPECT_EQ(matches[1].start, 13);
    EXPECT_EQ(matches[1].end, 18);
    EXPECT_EQ(matches[2].start, 27);
    EXPECT_EQ(matches[2].end, 32);
}

TEST(RegexEngineTest, CaseSensitiveAndWholeWord) {
    RegexEngine re;
    EXPECT_TRUE(re.compile("cat", true, true, false));

    std::string text = "cat Cat concatenate cat.";
    auto matches = re.matchAll(text);
    ASSERT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].start, 0);
    EXPECT_EQ(matches[1].start, 20);
}

TEST(RegexEngineTest, RegexCaptureGroupReplacement) {
    RegexEngine re;
    EXPECT_TRUE(re.compile(R"((\w+)\s*=\s*(\d+))", false, false, true));

    std::string text = "width = 100, height = 200";
    auto matches = re.matchAll(text);
    ASSERT_EQ(matches.size(), 2);

    std::string rep1 = re.expandReplacement(text, matches[0], "$1: [$2 px]");
    EXPECT_EQ(rep1, "width: [100 px]");

    std::string rep2 = re.expandReplacement(text, matches[1], "$1: [$2 px]");
    EXPECT_EQ(rep2, "height: [200 px]");
}

TEST(SearchEngineTest, FindNextAndPreviousWithWrap) {
    SearchEngine se;
    SearchOptions opt{
        .query = "target",
        .replacement = "",
        .caseSensitive = true,
        .wholeWord = false,
        .isRegex = false,
        .wrapAround = true
    };

    std::string text = "target first ... target second ... target third";

    auto r1 = se.findNext(text, 0, opt);
    EXPECT_TRUE(r1.found);
    EXPECT_EQ(r1.startPos, 0);
    EXPECT_EQ(r1.matchIndex, 1);
    EXPECT_EQ(r1.totalMatches, 3);

    auto r2 = se.findNext(text, 10, opt);
    EXPECT_TRUE(r2.found);
    EXPECT_EQ(r2.startPos, 17);
    EXPECT_EQ(r2.matchIndex, 2);

    // After 35, should wrap to index 1
    auto r3 = se.findNext(text, 40, opt);
    EXPECT_TRUE(r3.found);
    EXPECT_EQ(r3.startPos, 0);
    EXPECT_EQ(r3.matchIndex, 1);

    // Find previous from end
    auto p1 = se.findPrevious(text, 40, opt);
    EXPECT_TRUE(p1.found);
    EXPECT_EQ(p1.startPos, 35);
    EXPECT_EQ(p1.matchIndex, 3);

    // Find previous before first match should wrap to last
    auto p2 = se.findPrevious(text, 0, opt);
    EXPECT_TRUE(p2.found);
    EXPECT_EQ(p2.startPos, 35);
    EXPECT_EQ(p2.matchIndex, 3);
}

TEST(SearchEngineTest, ReplaceAll) {
    SearchEngine se;
    SearchOptions opt{
        .query = "foo",
        .replacement = "bar",
        .caseSensitive = true,
        .wholeWord = false,
        .isRegex = false,
        .wrapAround = true
    };

    std::string text = "foo and foo or foo!";
    auto [result, count] = se.replaceAll(text, opt);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(result, "bar and bar or bar!");
}
