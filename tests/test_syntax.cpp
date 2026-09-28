#include <gtest/gtest.h>
#include <ILexer.h>
#include <Lexilla.h>
#include "syntax/LexerManager.h"

using namespace notepadx;

TEST(LexerManagerTest, DetectsLanguagesCorrectly) {
    LexerManager lm;

    EXPECT_EQ(lm.detectLanguage("main.cpp"), "C/C++");
    EXPECT_EQ(lm.detectLanguage("/home/user/project/header.hpp"), "C/C++");
    EXPECT_EQ(lm.detectLanguage("script.py"), "Python");
    EXPECT_EQ(lm.detectLanguage("lib.rs"), "Rust");
    EXPECT_EQ(lm.detectLanguage("app.js"), "JavaScript");
    EXPECT_EQ(lm.detectLanguage("types.ts"), "JavaScript");
    EXPECT_EQ(lm.detectLanguage("data.json"), "JSON");
    EXPECT_EQ(lm.detectLanguage("index.html"), "HTML");
    EXPECT_EQ(lm.detectLanguage("styles.css"), "CSS");
    EXPECT_EQ(lm.detectLanguage("deploy.sh"), "Shell");
    EXPECT_EQ(lm.detectLanguage("README.md"), "Markdown");
    EXPECT_EQ(lm.detectLanguage("config.yaml"), "YAML");
    EXPECT_EQ(lm.detectLanguage("schema.sql"), "SQL");
    EXPECT_EQ(lm.detectLanguage("main.go"), "Go");
    EXPECT_EQ(lm.detectLanguage("App.java"), "Java");
    EXPECT_EQ(lm.detectLanguage("Makefile"), "Makefile");
    EXPECT_EQ(lm.detectLanguage("CMakeLists.txt"), "CMake");
    EXPECT_EQ(lm.detectLanguage("notes.unknown"), "Plain Text");
    EXPECT_EQ(lm.detectLanguage(""), "Plain Text");
}

TEST(LexerManagerTest, AvailableLanguagesListNonEmpty) {
    LexerManager lm;
    auto langs = lm.availableLanguages();
    EXPECT_GE(langs.size(), 15);
}

TEST(LexerManagerTest, CreateLexerInstantiatesCoreLexers) {
    EXPECT_NE(CreateLexer("cpp"), nullptr);
    EXPECT_NE(CreateLexer("python"), nullptr);
    EXPECT_NE(CreateLexer("rust"), nullptr);
    EXPECT_NE(CreateLexer("json"), nullptr);
    EXPECT_NE(CreateLexer("bash"), nullptr);
    EXPECT_NE(CreateLexer("sql"), nullptr);
    EXPECT_NE(CreateLexer("hypertext"), nullptr);
    EXPECT_NE(CreateLexer("markdown"), nullptr);
}
