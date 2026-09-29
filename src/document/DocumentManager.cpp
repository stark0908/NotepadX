#include "document/DocumentManager.h"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace notepadx {

std::unique_ptr<Document> DocumentManager::createUntitledDocument() {
    std::unordered_set<int> usedNumbers;
    for (const auto& doc : documents_) {
        if (doc && doc->isUnnamed()) {
            const std::string& t = doc->title();
            if (t.rfind("Untitled ", 0) == 0) {
                try {
                    usedNumbers.insert(std::stoi(t.substr(9)));
                } catch (...) {}
            }
        }
    }

    int nextSlot = 1;
    while (usedNumbers.contains(nextSlot)) {
        ++nextSlot;
    }

    auto doc = std::make_unique<Document>();
    doc->setTitle("Untitled " + std::to_string(nextSlot));
    return doc;
}

Document* DocumentManager::createUntitled() {
    return addDocument(createUntitledDocument());
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

void DocumentManager::syncUntitledCounterWithExisting() {
    for (const auto& doc : documents_) {
        if (doc && doc->isUnnamed()) {
            const std::string& t = doc->title();
            if (t.rfind("Untitled ", 0) == 0) {
                try {
                    const int num = std::stoi(t.substr(9));
                    if (num > untitledCounter_) {
                        untitledCounter_ = num;
                    }
                } catch (...) {}
            }
        }
    }
}

std::string DocumentManager::displayName(const Document* doc) const {
    if (!doc) {
        return {};
    }
    std::string base = doc->title().empty() ? "Untitled" : doc->title();
    if (!doc->isUnnamed() && !doc->filePath().empty()) {
        bool duplicate = false;
        for (const auto& other : documents_) {
            if (other.get() != doc && other->title() == doc->title()) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            std::filesystem::path p(doc->filePath());
            std::string parentName = p.parent_path().filename().string();
            if (!parentName.empty()) {
                base += " (" + parentName + ")";
            }
        }
    }
    return doc->isModified() ? "• " + base : base;
}

} // namespace notepadx
