#pragma once

#include "editor/ScintillaAdapter.h"

#include <string>
#include <string_view>
#include <vector>

namespace notepadx {

struct LanguageDefinition {
    std::string name;
    std::string lexerName;
    std::vector<std::string> extensions;
    std::vector<std::string> filenames;
    std::vector<std::string> keywordSets;
};

class LexerManager {
public:
    LexerManager();
    ~LexerManager() = default;

    LexerManager(const LexerManager&) = delete;
    LexerManager& operator=(const LexerManager&) = delete;

    LexerManager(LexerManager&&) noexcept = default;
    LexerManager& operator=(LexerManager&&) noexcept = default;

    [[nodiscard]] std::string detectLanguage(const std::string& pathOrFilename) const;
    [[nodiscard]] const LanguageDefinition* getLanguage(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> availableLanguages() const;

    bool applyLanguage(ScintillaAdapter& adapter,
                       std::string_view languageName,
                       bool isDarkTheme = true) const;

private:
    void initLanguages();
    void applyThemeStyles(ScintillaAdapter& adapter,
                          const std::string& lexerName,
                          bool isDarkTheme) const;

    std::vector<LanguageDefinition> languages_;
};

} // namespace notepadx
