#pragma once

#include <string>
#include <vector>

namespace notepadx {

struct EditorConfig {
    std::string fontName{"Monospace"};
    int fontSizePt{11};
    int tabWidth{4};
    bool useTabs{false};
    bool showLineNumbers{true};
    bool wordWrap{false};
    int eolMode{2}; // SC_EOL_LF

    static std::string detectBestFont();
    static EditorConfig createDefault();
};

} // namespace notepadx
