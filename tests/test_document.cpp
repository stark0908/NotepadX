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

TEST(DocumentManagerTest, SlotRecyclingReclaimsGaps) {
    DocumentManager mgr;
    // Simulate open tabs: Untitled 1 and Untitled 4 (gap at 2, 3)
    auto doc1 = std::make_unique<Document>("id1", "Untitled 1", "");
    auto doc4 = std::make_unique<Document>("id4", "Untitled 4", "");
    mgr.addDocument(std::move(doc1));
    mgr.addDocument(std::move(doc4));

    // Next created untitled document reclaims slot 2
    auto nextDoc1 = mgr.createUntitledDocument();
    EXPECT_EQ(nextDoc1->title(), "Untitled 2");

    // Add Untitled 2; next slot should reclaim slot 3
    mgr.addDocument(std::move(nextDoc1));
    auto nextDoc2 = mgr.createUntitledDocument();
    EXPECT_EQ(nextDoc2->title(), "Untitled 3");

    // Add Untitled 3; now 1, 2, 3, 4 are filled -> next is Untitled 5
    mgr.addDocument(std::move(nextDoc2));
    auto nextDoc3 = mgr.createUntitledDocument();
    EXPECT_EQ(nextDoc3->title(), "Untitled 5");
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
