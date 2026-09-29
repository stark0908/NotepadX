#pragma once

#include "document/Document.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace notepadx {

class DocumentManager {
public:
    DocumentManager() = default;
    ~DocumentManager() = default;

    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;

    DocumentManager(DocumentManager&&) noexcept = default;
    DocumentManager& operator=(DocumentManager&&) noexcept = default;

    Document* createUntitled();
    std::unique_ptr<Document> createUntitledDocument();
    Document* addDocument(std::unique_ptr<Document> doc);

    [[nodiscard]] Document* findById(std::string_view id) const;
    [[nodiscard]] Document* findByPath(std::string_view path) const;

    std::unique_ptr<Document> removeDocument(std::string_view id);

    [[nodiscard]] size_t count() const noexcept { return documents_.size(); }
    [[nodiscard]] bool empty() const noexcept { return documents_.empty(); }

    [[nodiscard]] const std::vector<std::unique_ptr<Document>>& documents() const noexcept {
        return documents_;
    }

    [[nodiscard]] Document* activeDocument() const noexcept { return activeDocument_; }
    void setActiveDocument(Document* doc) noexcept { activeDocument_ = doc; }
    bool setActiveDocument(std::string_view id);

    [[nodiscard]] int indexOf(std::string_view id) const;
    [[nodiscard]] Document* at(size_t index) const;

    void syncUntitledCounterWithExisting();
    [[nodiscard]] std::string displayName(const Document* doc) const;

private:
    std::vector<std::unique_ptr<Document>> documents_;
    Document* activeDocument_{nullptr};
    int untitledCounter_{0};
};

} // namespace notepadx
