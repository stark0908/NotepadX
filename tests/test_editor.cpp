#include <gtest/gtest.h>
#include "editor/Editor.h"
#include "session/Autosave.h"

#include <gtk/gtk.h>

using namespace notepadx;

class EditorTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "GTK could not be initialized (no display available)";
        }
    }
};

TEST_F(EditorTest, LineNumberMarginIsSlimForSingleDigit) {
    EditorConfig cfg = EditorConfig::createDefault();
    cfg.showLineNumbers = true;
    Editor ed(cfg);

    // Initial 1 line document should have a slim margin (<= 20px, not the old 35-40px)
    const sptr_t width1 = ed.adapter().send(SCI_GETMARGINWIDTHN, 0);
    EXPECT_GT(width1, 0);
    EXPECT_LE(width1, 20);

    // Adding 15 lines expands the margin to accommodate 2 digits
    std::string multiLines;
    for (int i = 1; i <= 15; ++i) {
        multiLines += "Line " + std::to_string(i) + "\n";
    }
    ed.adapter().setText(multiLines);
    const sptr_t width15 = ed.adapter().send(SCI_GETMARGINWIDTHN, 0);
    EXPECT_GT(width15, width1);
}

TEST_F(EditorTest, SealUndoActionCreatesAtomicSteps) {
    EditorConfig cfg = EditorConfig::createDefault();
    Editor ed(cfg);
    ed.adapter().setText("");

    // Simulate typing first token and sealing action (like pressing Enter or pause)
    const std::string first = "hello ";
    ed.adapter().send(SCI_ADDTEXT, static_cast<uptr_t>(first.size()), reinterpret_cast<sptr_t>(first.data()));
    ed.sealUndoAction();

    // Type second token
    const std::string second = "world";
    ed.adapter().send(SCI_ADDTEXT, static_cast<uptr_t>(second.size()), reinterpret_cast<sptr_t>(second.data()));

    EXPECT_EQ(ed.adapter().getText(), "hello world");

    // First undo removes "world", leaves "hello "
    ed.undo();
    EXPECT_EQ(ed.adapter().getText(), "hello ");

    // Second undo removes "hello "
    ed.undo();
    EXPECT_EQ(ed.adapter().getText(), "");

    // Redo restores "hello "
    ed.redo();
    EXPECT_EQ(ed.adapter().getText(), "hello ");

    // Redo restores "world"
    ed.redo();
    EXPECT_EQ(ed.adapter().getText(), "hello world");
}

TEST(AutosaveTest, DefaultDelayIs500ms) {
    Autosave autosave([](const std::string&) {});
    EXPECT_EQ(autosave.delayMs(), 500);
}
