#pragma once

#include "editor/EditorConfig.h"

#include <filesystem>
#include <string>

namespace notepadx {

class Settings {
public:
    Settings();
    ~Settings() = default;

    Settings(const Settings&) = default;
    Settings& operator=(const Settings&) = default;

    Settings(Settings&&) noexcept = default;
    Settings& operator=(Settings&&) noexcept = default;

    [[nodiscard]] const std::string& fontName() const noexcept { return config_.fontName; }
    void setFontName(std::string font) { config_.fontName = std::move(font); }

    [[nodiscard]] int fontSizePt() const noexcept { return config_.fontSizePt; }
    void setFontSizePt(int pt) noexcept { config_.fontSizePt = pt; }

    [[nodiscard]] int tabWidth() const noexcept { return config_.tabWidth; }
    void setTabWidth(int width) noexcept { config_.tabWidth = width; }

    [[nodiscard]] bool useTabs() const noexcept { return config_.useTabs; }
    void setUseTabs(bool tabs) noexcept { config_.useTabs = tabs; }

    [[nodiscard]] bool showLineNumbers() const noexcept { return config_.showLineNumbers; }
    void setShowLineNumbers(bool show) noexcept { config_.showLineNumbers = show; }

    [[nodiscard]] bool wordWrap() const noexcept { return config_.wordWrap; }
    void setWordWrap(bool wrap) noexcept { config_.wordWrap = wrap; }

    [[nodiscard]] int eolMode() const noexcept { return config_.eolMode; }
    void setEolMode(int mode) noexcept { config_.eolMode = mode; }

    [[nodiscard]] const std::string& theme() const noexcept { return theme_; }
    void setTheme(std::string theme) { theme_ = std::move(theme); }
    [[nodiscard]] bool isDarkTheme() const noexcept { return theme_ == "dark"; }

    [[nodiscard]] const EditorConfig& editorConfig() const noexcept { return config_; }

    bool saveToFile(const std::filesystem::path& filePath = defaultSettingsFile()) const;
    bool loadFromFile(const std::filesystem::path& filePath = defaultSettingsFile());

    static std::filesystem::path defaultSettingsFile();

private:
    EditorConfig config_;
    std::string theme_{"dark"}; // "dark" or "light"
};

} // namespace notepadx
