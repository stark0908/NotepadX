#include "document/DocumentStore.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace notepadx {

std::filesystem::path DocumentStore::defaultStorageDir() {
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    std::filesystem::path basePath;
    if (xdgData && *xdgData) {
        basePath = std::filesystem::path(xdgData) / "NotepadX";
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            basePath = std::filesystem::path(home) / ".local" / "share" / "NotepadX";
        } else {
            basePath = std::filesystem::current_path() / ".notepadx";
        }
    }
    return basePath / "documents";
}

DocumentStore::DocumentStore(std::filesystem::path storageDir)
    : storageDir_(std::move(storageDir)) {
}

std::filesystem::path DocumentStore::documentPath(const std::string& id) const {
    return storageDir_ / id;
}

void DocumentStore::ensureDirectoryExists() const {
    std::error_code ec;
    std::filesystem::create_directories(storageDir_, ec);
}

bool DocumentStore::saveDocumentContent(const std::string& id, std::string_view content) const {
    if (id.empty()) {
        return false;
    }
    ensureDirectoryExists();

    const auto targetPath = documentPath(id);
    const auto tmpPath = storageDir_ / (id + ".tmp");

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, targetPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

std::string DocumentStore::loadDocumentContent(const std::string& id) const {
    const auto path = documentPath(id);
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool DocumentStore::hasDocumentContent(const std::string& id) const {
    std::error_code ec;
    return std::filesystem::is_regular_file(documentPath(id), ec);
}

bool DocumentStore::deleteDocumentContent(const std::string& id) const {
    std::error_code ec;
    return std::filesystem::remove(documentPath(id), ec);
}

} // namespace notepadx
