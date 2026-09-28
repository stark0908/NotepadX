#include <gtest/gtest.h>
#include "document/DocumentStore.h"
#include "session/Autosave.h"
#include "session/RecentlyClosed.h"
#include "session/SessionManager.h"

#include <filesystem>

using namespace notepadx;

class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "notepadx_test_persistence";
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

TEST_F(PersistenceTest, DocumentStoreSaveLoadDelete) {
    DocumentStore store(testDir_ / "documents");

    const std::string docId = "test-doc-1234";
    const std::string content = "Hello, world!\nLine 2 with special chars: 🚀 123";

    EXPECT_FALSE(store.hasDocumentContent(docId));
    EXPECT_TRUE(store.saveDocumentContent(docId, content));
    EXPECT_TRUE(store.hasDocumentContent(docId));

    EXPECT_EQ(store.loadDocumentContent(docId), content);

    EXPECT_TRUE(store.deleteDocumentContent(docId));
    EXPECT_FALSE(store.hasDocumentContent(docId));
    EXPECT_EQ(store.loadDocumentContent(docId), "");
}

TEST_F(PersistenceTest, SessionManagerRoundTrip) {
    const auto sessionFile = testDir_ / "session.json";

    SessionState original;
    original.activeTabIndex = 1;

    SessionTab tab1;
    tab1.id = "id-1";
    tab1.title = "Tab One";
    tab1.filePath = "";
    tab1.isModified = true;
    tab1.cursorPosition = 42;
    tab1.scrollLine = 5;
    original.tabs.push_back(tab1);

    SessionTab tab2;
    tab2.id = "id-2";
    tab2.title = "file.cpp";
    tab2.filePath = "/path/to/file.cpp";
    tab2.isModified = false;
    tab2.cursorPosition = 100;
    tab2.scrollLine = 12;
    original.tabs.push_back(tab2);

    EXPECT_TRUE(SessionManager::saveSession(original, sessionFile));
    EXPECT_TRUE(std::filesystem::exists(sessionFile));

    SessionState loaded = SessionManager::loadSession(sessionFile);
    EXPECT_EQ(loaded.activeTabIndex, 1);
    ASSERT_EQ(loaded.tabs.size(), 2);

    EXPECT_EQ(loaded.tabs[0].id, "id-1");
    EXPECT_EQ(loaded.tabs[0].title, "Tab One");
    EXPECT_EQ(loaded.tabs[0].filePath, "");
    EXPECT_TRUE(loaded.tabs[0].isModified);
    EXPECT_EQ(loaded.tabs[0].cursorPosition, 42);
    EXPECT_EQ(loaded.tabs[0].scrollLine, 5);

    EXPECT_EQ(loaded.tabs[1].id, "id-2");
    EXPECT_EQ(loaded.tabs[1].title, "file.cpp");
    EXPECT_EQ(loaded.tabs[1].filePath, "/path/to/file.cpp");
    EXPECT_FALSE(loaded.tabs[1].isModified);
    EXPECT_EQ(loaded.tabs[1].cursorPosition, 100);
    EXPECT_EQ(loaded.tabs[1].scrollLine, 12);
}

TEST_F(PersistenceTest, RecentlyClosedFifoAndMaxLimit) {
    RecentlyClosed rc(3); // Small limit for testing
    EXPECT_TRUE(rc.empty());
    EXPECT_EQ(rc.size(), 0);

    ClosedTabEntry e1{.id = "1", .title = "One", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "c1", .closedTimestamp = 100};
    ClosedTabEntry e2{.id = "2", .title = "Two", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "c2", .closedTimestamp = 200};
    ClosedTabEntry e3{.id = "3", .title = "Three", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "c3", .closedTimestamp = 300};
    ClosedTabEntry e4{.id = "4", .title = "Four", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "c4", .closedTimestamp = 400};

    rc.push(e1);
    rc.push(e2);
    rc.push(e3);
    EXPECT_EQ(rc.size(), 3);

    // Push 4th: should purge oldest (e1)
    rc.push(e4);
    EXPECT_EQ(rc.size(), 3);

    // Pop order is LIFO for reopen (most recently closed first)
    auto pop1 = rc.popLatest();
    ASSERT_TRUE(pop1.has_value());
    EXPECT_EQ(pop1->id, "4");

    auto pop2 = rc.popLatest();
    ASSERT_TRUE(pop2.has_value());
    EXPECT_EQ(pop2->id, "3");

    auto pop3 = rc.popLatest();
    ASSERT_TRUE(pop3.has_value());
    EXPECT_EQ(pop3->id, "2");

    // e1 was purged
    auto pop4 = rc.popLatest();
    EXPECT_FALSE(pop4.has_value());
    EXPECT_TRUE(rc.empty());
}

TEST_F(PersistenceTest, RecentlyClosedSaveLoad) {
    const auto trashFile = testDir_ / "trash.json";

    RecentlyClosed rc(50);
    rc.push(ClosedTabEntry{.id = "a", .title = "Tab A", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "alpha", .closedTimestamp = 100});
    rc.push(ClosedTabEntry{.id = "b", .title = "Tab B", .filePath = "", .cursorPosition = 0, .scrollLine = 0, .content = "beta", .closedTimestamp = 200});

    EXPECT_TRUE(rc.saveToFile(trashFile));

    RecentlyClosed loaded(50);
    EXPECT_TRUE(loaded.loadFromFile(trashFile));
    EXPECT_EQ(loaded.size(), 2);

    auto pop = loaded.popLatest();
    ASSERT_TRUE(pop.has_value());
    EXPECT_EQ(pop->id, "b");
    EXPECT_EQ(pop->title, "Tab B");
    EXPECT_EQ(pop->content, "beta");
}

TEST_F(PersistenceTest, AutosaveDebounceLogic) {
    std::vector<std::string> savedDocs;
    std::function<void()> scheduledCb;
    uint32_t scheduledDelay = 0;

    Autosave autosave(
        [&savedDocs](const std::string& docId) {
            savedDocs.push_back(docId);
        },
        2000,
        [&scheduledCb, &scheduledDelay](uint32_t delayMs, std::function<void()> cb) {
            scheduledDelay = delayMs;
            scheduledCb = std::move(cb);
        }
    );

    EXPECT_FALSE(autosave.hasPending());
    autosave.markDirty("doc-1");
    EXPECT_TRUE(autosave.isPending("doc-1"));
    EXPECT_TRUE(autosave.hasPending());
    EXPECT_EQ(scheduledDelay, 2000);
    ASSERT_TRUE(scheduledCb != nullptr);

    // Save shouldn't have fired yet
    EXPECT_TRUE(savedDocs.empty());

    // Mark another document dirty before timeout fires
    autosave.markDirty("doc-2");
    EXPECT_TRUE(autosave.isPending("doc-2"));

    // Fire simulated timeout
    scheduledCb();

    // Both documents should be flushed
    EXPECT_EQ(savedDocs.size(), 2);
    EXPECT_FALSE(autosave.hasPending());
    EXPECT_FALSE(autosave.isPending("doc-1"));
    EXPECT_FALSE(autosave.isPending("doc-2"));
}
