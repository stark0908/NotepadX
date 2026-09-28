#include "editor/EditorConfig.h"

#include <pango/pangocairo.h>
#include <string_view>
#include <unordered_set>

namespace notepadx {

std::string EditorConfig::detectBestFont() {
    static const std::string bestFont = []() -> std::string {
        const std::vector<std::string> preferredFonts = {
            "JetBrains Mono",
            "Fira Code",
            "Source Code Pro",
            "Monospace"
        };

        PangoFontMap* fontMap = pango_cairo_font_map_get_default();
        if (!fontMap) {
            return "Monospace";
        }

        PangoFontFamily** families = nullptr;
        int nFamilies = 0;
        pango_font_map_list_families(fontMap, &families, &nFamilies);

        std::unordered_set<std::string> available;
        available.reserve(static_cast<size_t>(nFamilies));
        for (int i = 0; i < nFamilies; ++i) {
            const char* name = pango_font_family_get_name(families[i]);
            if (name) {
                available.insert(name);
            }
        }
        g_free(families);

        for (const auto& font : preferredFonts) {
            if (available.contains(font)) {
                return font;
            }
        }

        return "Monospace";
    }();
    return bestFont;
}

EditorConfig EditorConfig::createDefault() {
    EditorConfig config;
    config.fontName = detectBestFont();
    return config;
}

} // namespace notepadx
