#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace notepadx {

struct SessionTab {
    std::string id;
    std::string title;
    std::string filePath;
    bool isModified{false};
    int64_t cursorPosition{0};
    int64_t scrollLine{0};
    std::string language{"Plain Text"};
};

struct SessionState {
    int activeTabIndex{0};
    std::vector<SessionTab> tabs;

    [[nodiscard]] bool empty() const noexcept { return tabs.empty(); }
};

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    static bool saveSession(const SessionState& state, const std::filesystem::path& sessionFile = defaultSessionFile());
    static SessionState loadSession(const std::filesystem::path& sessionFile = defaultSessionFile());

    static std::filesystem::path defaultSessionFile();
    static std::filesystem::path defaultTrashFile();
};

} // namespace notepadx
