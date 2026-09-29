#include "session/SessionManager.h"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace notepadx {

std::filesystem::path SessionManager::defaultSessionFile() {
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
    return basePath / "session.json";
}

std::filesystem::path SessionManager::defaultTrashFile() {
    return defaultSessionFile().parent_path() / "trash.json";
}

bool SessionManager::saveSession(const SessionState& state, const std::filesystem::path& sessionFile) {
    nlohmann::json jTabs = nlohmann::json::array();
    for (const auto& tab : state.tabs) {
        jTabs.push_back({
            {"id", tab.id},
            {"title", tab.title},
            {"filePath", tab.filePath},
            {"isModified", tab.isModified},
            {"cursorPosition", tab.cursorPosition},
            {"scrollLine", tab.scrollLine},
            {"language", tab.language}
        });
    }

    nlohmann::json jRoot = {
        {"version", 1},
        {"activeTabIndex", state.activeTabIndex},
        {"tabs", std::move(jTabs)}
    };

    std::error_code ec;
    std::filesystem::create_directories(sessionFile.parent_path(), ec);

    const auto tmpPath = sessionFile.string() + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << jRoot.dump(2);
        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::filesystem::rename(tmpPath, sessionFile, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

SessionState SessionManager::loadSession(const std::filesystem::path& sessionFile) {
    SessionState state;
    std::ifstream in(sessionFile);
    if (!in.is_open()) {
        return state;
    }

    try {
        nlohmann::json jRoot;
        in >> jRoot;

        state.activeTabIndex = jRoot.value("activeTabIndex", 0);
        if (jRoot.contains("tabs") && jRoot["tabs"].is_array()) {
            for (const auto& jTab : jRoot["tabs"]) {
                SessionTab tab;
                tab.id = jTab.value("id", "");
                tab.title = jTab.value("title", "Untitled");
                tab.filePath = jTab.value("filePath", "");
                tab.isModified = jTab.value("isModified", false);
                tab.cursorPosition = jTab.value("cursorPosition", int64_t{0});
                tab.scrollLine = jTab.value("scrollLine", int64_t{0});
                tab.language = jTab.value("language", "Plain Text");

                if (!tab.id.empty()) {
                    state.tabs.push_back(std::move(tab));
                }
            }
        }
    } catch (...) {
        return SessionState{};
    }

    if (state.activeTabIndex < 0 ||
        (!state.tabs.empty() && static_cast<size_t>(state.activeTabIndex) >= state.tabs.size())) {
        state.activeTabIndex = 0;
    }
    return state;
}

} // namespace notepadx
