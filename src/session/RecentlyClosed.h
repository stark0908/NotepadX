#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace notepadx {

struct ClosedTabEntry {
    std::string id;
    std::string title;
    std::string filePath;
    int64_t cursorPosition{0};
    int64_t scrollLine{0};
    std::string content;
    int64_t closedTimestamp{0};
};

class RecentlyClosed {
public:
    static constexpr size_t kMaxEntries = 50;

    explicit RecentlyClosed(size_t maxEntries = kMaxEntries);
    ~RecentlyClosed() = default;

    RecentlyClosed(const RecentlyClosed&) = delete;
    RecentlyClosed& operator=(const RecentlyClosed&) = delete;

    RecentlyClosed(RecentlyClosed&&) noexcept = default;
    RecentlyClosed& operator=(RecentlyClosed&&) noexcept = default;

    void push(ClosedTabEntry entry);
    std::optional<ClosedTabEntry> popLatest();

    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] size_t maxEntries() const noexcept { return maxEntries_; }
    void clear() noexcept { entries_.clear(); }

    [[nodiscard]] const std::deque<ClosedTabEntry>& entries() const noexcept { return entries_; }

    bool saveToFile(const std::filesystem::path& filePath) const;
    bool loadFromFile(const std::filesystem::path& filePath);

private:
    size_t maxEntries_{kMaxEntries};
    std::deque<ClosedTabEntry> entries_; // front is most recent, back is oldest
};

} // namespace notepadx
