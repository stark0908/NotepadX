#include "config/Settings.h"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace notepadx {

Settings::Settings()
    : config_(EditorConfig::createDefault()) {
}

std::filesystem::path Settings::defaultSettingsFile() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path basePath;
    if (xdgConfig && *xdgConfig) {
        basePath = std::filesystem::path(xdgConfig) / "NotepadX";
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            basePath = std::filesystem::path(home) / ".config" / "NotepadX";
        } else {
            basePath = std::filesystem::current_path() / ".notepadx";
        }
    }
    return basePath / "settings.json";
}

bool Settings::saveToFile(const std::filesystem::path& filePath) const {
    nlohmann::json j = {
        {"fontName", config_.fontName},
        {"fontSizePt", config_.fontSizePt},
        {"tabWidth", config_.tabWidth},
        {"useTabs", config_.useTabs},
        {"showLineNumbers", config_.showLineNumbers},
        {"wordWrap", config_.wordWrap},
        {"eolMode", config_.eolMode},
        {"theme", theme_}
    };

    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    const auto tmpPath = filePath.string() + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << j.dump(2);
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

bool Settings::loadFromFile(const std::filesystem::path& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        in >> j;

        config_.fontName = j.value("fontName", config_.fontName);
        config_.fontSizePt = j.value("fontSizePt", config_.fontSizePt);
        config_.tabWidth = j.value("tabWidth", config_.tabWidth);
        config_.useTabs = j.value("useTabs", config_.useTabs);
        config_.showLineNumbers = j.value("showLineNumbers", config_.showLineNumbers);
        config_.wordWrap = j.value("wordWrap", config_.wordWrap);
        config_.eolMode = j.value("eolMode", config_.eolMode);
        theme_ = j.value("theme", theme_);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace notepadx
