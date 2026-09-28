#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace notepadx {

class DocumentStore {
public:
    explicit DocumentStore(std::filesystem::path storageDir = defaultStorageDir());
    ~DocumentStore() = default;

    DocumentStore(const DocumentStore&) = delete;
    DocumentStore& operator=(const DocumentStore&) = delete;

    DocumentStore(DocumentStore&&) noexcept = default;
    DocumentStore& operator=(DocumentStore&&) noexcept = default;

    [[nodiscard]] const std::filesystem::path& storageDir() const noexcept { return storageDir_; }

    bool saveDocumentContent(const std::string& id, std::string_view content) const;
    [[nodiscard]] std::string loadDocumentContent(const std::string& id) const;
    [[nodiscard]] bool hasDocumentContent(const std::string& id) const;
    bool deleteDocumentContent(const std::string& id) const;

    static std::filesystem::path defaultStorageDir();

private:
    [[nodiscard]] std::filesystem::path documentPath(const std::string& id) const;
    void ensureDirectoryExists() const;

    std::filesystem::path storageDir_;
};

} // namespace notepadx
