#include "search/Search.h"

namespace notepadx {

std::vector<MatchResult> SearchEngine::findAll(std::string_view text,
                                              const SearchOptions& options) {
    if (options.query.empty() || text.empty()) {
        return {};
    }

    if (!regexEngine_.compile(options.query, options.caseSensitive, options.wholeWord, options.isRegex)) {
        return {};
    }

    return regexEngine_.matchAll(text);
}

SearchResult SearchEngine::findNext(std::string_view text,
                                   size_t currentPos,
                                   const SearchOptions& options) {
    auto matches = findAll(text, options);
    if (matches.empty()) {
        return SearchResult{.found = false, .statusText = "No matches found"};
    }

    for (size_t i = 0; i < matches.size(); ++i) {
        if (matches[i].start >= currentPos) {
            return SearchResult{
                .found = true,
                .matchIndex = i + 1,
                .totalMatches = matches.size(),
                .startPos = matches[i].start,
                .endPos = matches[i].end,
                .statusText = std::to_string(i + 1) + " of " + std::to_string(matches.size())
            };
        }
    }

    if (options.wrapAround) {
        return SearchResult{
            .found = true,
            .matchIndex = 1,
            .totalMatches = matches.size(),
            .startPos = matches[0].start,
            .endPos = matches[0].end,
            .statusText = "1 of " + std::to_string(matches.size()) + " (wrapped)"
        };
    }

    return SearchResult{.found = false, .statusText = "No further matches"};
}

SearchResult SearchEngine::findPrevious(std::string_view text,
                                       size_t currentPos,
                                       const SearchOptions& options) {
    auto matches = findAll(text, options);
    if (matches.empty()) {
        return SearchResult{.found = false, .statusText = "No matches found"};
    }

    for (size_t i = matches.size(); i > 0; --i) {
        const size_t idx = i - 1;
        if (matches[idx].start < currentPos) {
            return SearchResult{
                .found = true,
                .matchIndex = idx + 1,
                .totalMatches = matches.size(),
                .startPos = matches[idx].start,
                .endPos = matches[idx].end,
                .statusText = std::to_string(idx + 1) + " of " + std::to_string(matches.size())
            };
        }
    }

    if (options.wrapAround) {
        const size_t lastIdx = matches.size() - 1;
        return SearchResult{
            .found = true,
            .matchIndex = matches.size(),
            .totalMatches = matches.size(),
            .startPos = matches[lastIdx].start,
            .endPos = matches[lastIdx].end,
            .statusText = std::to_string(matches.size()) + " of " + std::to_string(matches.size()) + " (wrapped)"
        };
    }

    return SearchResult{.found = false, .statusText = "No earlier matches"};
}

std::pair<std::string, size_t> SearchEngine::replaceAll(std::string_view text,
                                                       const SearchOptions& options) {
    auto matches = findAll(text, options);
    if (matches.empty()) {
        return {std::string(text), 0};
    }

    std::string result;
    result.reserve(text.size());
    size_t lastPos = 0;

    for (const auto& m : matches) {
        if (m.start > lastPos) {
            result.append(text.substr(lastPos, m.start - lastPos));
        }
        result.append(regexEngine_.expandReplacement(text, m, options.replacement));
        lastPos = m.end;
    }

    if (lastPos < text.size()) {
        result.append(text.substr(lastPos));
    }

    return {result, matches.size()};
}

} // namespace notepadx
