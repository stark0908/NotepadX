#include "session/RecentlyClosed.h"

#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

namespace notepadx {

RecentlyClosed::RecentlyClosed(size_t maxEntries)
    : maxEntries_(maxEntries) {
}

void RecentlyClosed::push(ClosedTabEntry entry) {
    if (entry.closedTimestamp == 0) {
        entry.closedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    entries_.push_front(std::move(entry));
    while (entries_.size() > maxEntries_) {
        entries_.pop_back();
    }
}

std::optional<ClosedTabEntry> RecentlyClosed::popLatest() {
    if (entries_.empty()) {
        return std::nullopt;
    }
    ClosedTabEntry entry = std::move(entries_.front());
    entries_.pop_front();
    return entry;
}

bool RecentlyClosed::saveToFile(const std::filesystem::path& filePath) const {
    nlohmann::json jArray = nlohmann::json::array();
    for (const auto& e : entries_) {
        jArray.push_back({
            {"id", e.id},
            {"title", e.title},
            {"filePath", e.filePath},
            {"cursorPosition", e.cursorPosition},
            {"scrollLine", e.scrollLine},
            {"content", e.content},
            {"closedTimestamp", e.closedTimestamp}
        });
    }

    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    const auto tmpPath = filePath.string() + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << jArray.dump(2);
        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::filesystem::rename(tmpPath, filePath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

bool RecentlyClosed::loadFromFile(const std::filesystem::path& filePath) {
    entries_.clear();
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        in >> j;
        if (!j.is_array()) {
            return false;
        }

        for (const auto& item : j) {
            ClosedTabEntry e;
            e.id = item.value("id", "");
            e.title = item.value("title", "Untitled");
            e.filePath = item.value("filePath", "");
            e.cursorPosition = item.value("cursorPosition", int64_t{0});
            e.scrollLine = item.value("scrollLine", int64_t{0});
            e.content = item.value("content", "");
            e.closedTimestamp = item.value("closedTimestamp", int64_t{0});

            if (!e.id.empty()) {
                entries_.push_back(std::move(e));
                if (entries_.size() >= maxEntries_) {
                    break;
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace notepadx
