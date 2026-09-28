#include "document/DocumentManager.h"

#include <algorithm>

namespace notepadx {

Document* DocumentManager::createUntitled() {
    ++untitledCounter_;
    auto doc = std::make_unique<Document>();
    doc->setTitle("Untitled " + std::to_string(untitledCounter_));
    Document* ptr = doc.get();
    documents_.push_back(std::move(doc));
    activeDocument_ = ptr;
    return ptr;
}

Document* DocumentManager::addDocument(std::unique_ptr<Document> doc) {
    if (!doc) {
        return nullptr;
    }
    Document* ptr = doc.get();
    documents_.push_back(std::move(doc));
    activeDocument_ = ptr;
    return ptr;
}

Document* DocumentManager::findById(std::string_view id) const {
    for (const auto& doc : documents_) {
        if (doc->id() == id) {
            return doc.get();
        }
    }
    return nullptr;
}

Document* DocumentManager::findByPath(std::string_view path) const {
    if (path.empty()) {
        return nullptr;
    }
    for (const auto& doc : documents_) {
        if (doc->filePath() == path) {
            return doc.get();
        }
    }
    return nullptr;
}

std::unique_ptr<Document> DocumentManager::removeDocument(std::string_view id) {
    auto it = std::find_if(documents_.begin(), documents_.end(),
                           [&id](const auto& d) { return d->id() == id; });
    if (it == documents_.end()) {
        return nullptr;
    }

    std::unique_ptr<Document> removed = std::move(*it);
    documents_.erase(it);

    if (activeDocument_ == removed.get()) {
        activeDocument_ = documents_.empty() ? nullptr : documents_.back().get();
    }
    return removed;
}

bool DocumentManager::setActiveDocument(std::string_view id) {
    Document* doc = findById(id);
    if (doc) {
        activeDocument_ = doc;
        return true;
    }
    return false;
}

int DocumentManager::indexOf(std::string_view id) const {
    for (size_t i = 0; i < documents_.size(); ++i) {
        if (documents_[i]->id() == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Document* DocumentManager::at(size_t index) const {
    if (index < documents_.size()) {
        return documents_[index].get();
    }
    return nullptr;
}

} // namespace notepadx
