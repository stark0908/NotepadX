#include <gtest/gtest.h>
#include "document/Document.h"
#include "document/DocumentManager.h"

using namespace notepadx;

TEST(DocumentTest, DefaultConstructorGeneratesUuidAndUntitled) {
    Document doc;
    EXPECT_FALSE(doc.id().empty());
    EXPECT_EQ(doc.id().length(), 36); // UUID format: 8-4-4-4-12
    EXPECT_EQ(doc.title(), "Untitled");
    EXPECT_TRUE(doc.filePath().empty());
    EXPECT_TRUE(doc.isUnnamed());
    EXPECT_FALSE(doc.isModified());
    EXPECT_EQ(doc.displayName(), "Untitled");
}

TEST(DocumentTest, ModifiedDisplayNamePrependsBullet) {
    Document doc;
    doc.setTitle("notes.txt");
    EXPECT_EQ(doc.displayName(), "notes.txt");
    doc.setModified(true);
    EXPECT_EQ(doc.displayName(), "• notes.txt");
}

TEST(DocumentTest, SetFilePathUpdatesTitle) {
    Document doc;
    doc.setFilePath("/home/user/code/main.cpp");
    EXPECT_EQ(doc.filePath(), "/home/user/code/main.cpp");
    EXPECT_EQ(doc.title(), "main.cpp");
    EXPECT_FALSE(doc.isUnnamed());
}

TEST(DocumentManagerTest, CreateUntitledIncrementsNumber) {
    DocumentManager mgr;
    EXPECT_TRUE(mgr.empty());
    EXPECT_EQ(mgr.count(), 0);

    Document* doc1 = mgr.createUntitled();
    ASSERT_NE(doc1, nullptr);
    EXPECT_EQ(doc1->title(), "Untitled 1");
    EXPECT_EQ(mgr.count(), 1);
    EXPECT_EQ(mgr.activeDocument(), doc1);

    Document* doc2 = mgr.createUntitled();
    ASSERT_NE(doc2, nullptr);
    EXPECT_EQ(doc2->title(), "Untitled 2");
    EXPECT_EQ(mgr.count(), 2);
    EXPECT_EQ(mgr.activeDocument(), doc2);
}

TEST(DocumentManagerTest, FindByIdAndPath) {
    DocumentManager mgr;
    Document* doc1 = mgr.createUntitled();
    auto doc2 = std::make_unique<Document>("custom-uuid", "foo.txt", "/tmp/foo.txt");
    Document* doc2Ptr = mgr.addDocument(std::move(doc2));

    EXPECT_EQ(mgr.findById(doc1->id()), doc1);
    EXPECT_EQ(mgr.findById("custom-uuid"), doc2Ptr);
    EXPECT_EQ(mgr.findByPath("/tmp/foo.txt"), doc2Ptr);
    EXPECT_EQ(mgr.findByPath("/nonexistent"), nullptr);
}

TEST(DocumentManagerTest, RemoveDocumentUpdatesActive) {
    DocumentManager mgr;
    Document* doc1 = mgr.createUntitled();
    Document* doc2 = mgr.createUntitled();

    EXPECT_EQ(mgr.activeDocument(), doc2);
    std::string id2 = doc2->id();

    auto removed = mgr.removeDocument(id2);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->id(), id2);
    EXPECT_EQ(mgr.count(), 1);
    EXPECT_EQ(mgr.activeDocument(), doc1);
}

TEST(DocumentManagerTest, SyncUntitledCounterAfterRestore) {
    DocumentManager mgr;
    // Simulate restoring existing untitled tabs from a saved session
    auto doc1 = std::make_unique<Document>("id1", "Untitled 1", "");
    auto doc4 = std::make_unique<Document>("id4", "Untitled 4", "");
    mgr.addDocument(std::move(doc1));
    mgr.addDocument(std::move(doc4));

    mgr.syncUntitledCounterWithExisting();

    // Next created untitled document should be Untitled 5, not Untitled 1
    auto nextDoc = mgr.createUntitledDocument();
    EXPECT_EQ(nextDoc->title(), "Untitled 5");
}

TEST(DocumentManagerTest, DuplicateFileNameDisambiguation) {
    DocumentManager mgr;
    auto docA = std::make_unique<Document>("idA", "main.cpp", "/workspace/projectA/main.cpp");
    auto docB = std::make_unique<Document>("idB", "main.cpp", "/workspace/projectB/main.cpp");
    auto docC = std::make_unique<Document>("idC", "utils.h", "/workspace/projectA/utils.h");

    Document* ptrA = mgr.addDocument(std::move(docA));
    Document* ptrB = mgr.addDocument(std::move(docB));
    Document* ptrC = mgr.addDocument(std::move(docC));

    // Non-duplicate file displays normal name
    EXPECT_EQ(mgr.displayName(ptrC), "utils.h");

    // Duplicate files display parent directory suffix
    EXPECT_EQ(mgr.displayName(ptrA), "main.cpp (projectA)");
    EXPECT_EQ(mgr.displayName(ptrB), "main.cpp (projectB)");

    // Modified status prepends bullet
    ptrA->setModified(true);
    EXPECT_EQ(mgr.displayName(ptrA), "• main.cpp (projectA)");
}
