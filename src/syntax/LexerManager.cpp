#include "syntax/LexerManager.h"

#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>
#include <Scintilla.h>

#include <algorithm>
#include <filesystem>

namespace notepadx {

namespace {

constexpr sptr_t ColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<sptr_t>(r) | (static_cast<sptr_t>(g) << 8) | (static_cast<sptr_t>(b) << 16);
}

} // namespace

LexerManager::LexerManager() {
    initLanguages();
}

void LexerManager::initLanguages() {
    languages_ = {
        LanguageDefinition{
            .name = "Plain Text",
            .lexerName = "",
            .extensions = {".txt", ".log", ".text"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "C/C++",
            .lexerName = "cpp",
            .extensions = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh", ".hxx", ".inl"},
            .filenames = {},
            .keywordSets = {
                // Primary keywords
                "alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t "
                "char16_t char32_t class compl concept const consteval constexpr constinit const_cast "
                "continue co_await co_return co_yield decltype default delete do double dynamic_cast "
                "else enum explicit export extern false float for friend goto if inline int long "
                "mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected "
                "public register reinterpret_cast requires return short signed sizeof static "
                "static_assert static_cast struct switch template this thread_local throw true try "
                "typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq",
                // Types and standard library
                "std string vector map unordered_map set unordered_set unique_ptr shared_ptr weak_ptr "
                "pair tuple array optional variant span string_view int8_t uint8_t int16_t uint16_t "
                "int32_t uint32_t int64_t uint64_t size_t uintptr_t intptr_t ptrdiff_t"
            }
        },
        LanguageDefinition{
            .name = "Python",
            .lexerName = "python",
            .extensions = {".py", ".pyw", ".pyi"},
            .filenames = {},
            .keywordSets = {
                "False None True and as assert async await break class continue def del elif else "
                "except finally for from global if import in is lambda nonlocal not or pass raise "
                "return try while with yield",
                "int float complex list tuple range str bytes bytearray memoryview set frozenset "
                "dict bool print len range open super type isinstance hasattr getattr setattr"
            }
        },
        LanguageDefinition{
            .name = "Rust",
            .lexerName = "rust",
            .extensions = {".rs"},
            .filenames = {},
            .keywordSets = {
                "as async await break const continue crate dyn else enum extern false fn for if "
                "impl in let loop match mod move mut pub ref return self Self static struct super "
                "trait true type unsafe use where while",
                "i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String "
                "Vec Option Some None Result Ok Err Box Rc Arc"
            }
        },
        LanguageDefinition{
            .name = "JavaScript",
            .lexerName = "cpp",
            .extensions = {".js", ".jsx", ".mjs", ".cjs", ".ts", ".tsx"},
            .filenames = {},
            .keywordSets = {
                "async await break case catch class const continue debugger default delete do else "
                "export extends false finally for function if import in instanceof new null return "
                "super switch this throw true try typeof var void while with yield",
                "Array Boolean Date Error Function JSON Math Number Object Promise RegExp String Symbol"
            }
        },
        LanguageDefinition{
            .name = "JSON",
            .lexerName = "json",
            .extensions = {".json"},
            .filenames = {},
            .keywordSets = {
                "true false null"
            }
        },
        LanguageDefinition{
            .name = "HTML",
            .lexerName = "hypertext",
            .extensions = {".html", ".htm", ".xhtml"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "CSS",
            .lexerName = "css",
            .extensions = {".css", ".scss", ".sass"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "Shell",
            .lexerName = "bash",
            .extensions = {".sh", ".bash", ".zsh"},
            .filenames = {},
            .keywordSets = {
                "case do done elif else esac fi for function if in select then until while"
            }
        },
        LanguageDefinition{
            .name = "Markdown",
            .lexerName = "markdown",
            .extensions = {".md", ".markdown"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "XML",
            .lexerName = "xml",
            .extensions = {".xml", ".svg", ".plist", ".ui"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "YAML",
            .lexerName = "yaml",
            .extensions = {".yaml", ".yml"},
            .filenames = {},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "SQL",
            .lexerName = "sql",
            .extensions = {".sql"},
            .filenames = {},
            .keywordSets = {
                "select from where insert into update delete join left right inner outer on group by "
                "order having limit offset create table alter drop index primary key foreign references "
                "values as and or not in is null distinct union all between case when then else end"
            }
        },
        LanguageDefinition{
            .name = "Go",
            .lexerName = "cpp",
            .extensions = {".go"},
            .filenames = {},
            .keywordSets = {
                "break case chan const continue default defer else fallthrough for func go goto if "
                "import interface map package range return select struct switch type var",
                "bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 "
                "rune string uint uint8 uint16 uint32 uint64 uintptr true false iota nil make new len cap"
            }
        },
        LanguageDefinition{
            .name = "Java",
            .lexerName = "cpp",
            .extensions = {".java"},
            .filenames = {},
            .keywordSets = {
                "abstract assert boolean break byte case catch char class const continue default do "
                "double else enum extends final finally float for goto if implements import instanceof "
                "int interface long native new package private protected public return short static "
                "strictfp super switch synchronized this throw throws transient try void volatile while"
            }
        },
        LanguageDefinition{
            .name = "Makefile",
            .lexerName = "makefile",
            .extensions = {".mk"},
            .filenames = {"Makefile", "makefile", "GNUmakefile"},
            .keywordSets = {}
        },
        LanguageDefinition{
            .name = "CMake",
            .lexerName = "cmake",
            .extensions = {".cmake"},
            .filenames = {"CMakeLists.txt"},
            .keywordSets = {}
        }
    };
}

std::string LexerManager::detectLanguage(const std::string& pathOrFilename) const {
    if (pathOrFilename.empty()) {
        return "Plain Text";
    }

    const std::filesystem::path p(pathOrFilename);
    const std::string filename = p.filename().string();
    const std::string ext = p.extension().string();

    // Check exact filenames
    for (const auto& lang : languages_) {
        for (const auto& fn : lang.filenames) {
            if (fn == filename) {
                return lang.name;
            }
        }
    }

    // Check extensions
    if (!ext.empty()) {
        for (const auto& lang : languages_) {
            for (const auto& e : lang.extensions) {
                if (e == ext) {
                    return lang.name;
                }
            }
        }
    }

    return "Plain Text";
}

const LanguageDefinition* LexerManager::getLanguage(std::string_view name) const {
    for (const auto& lang : languages_) {
        if (lang.name == name) {
            return &lang;
        }
    }
    return nullptr;
}

std::vector<std::string> LexerManager::availableLanguages() const {
    std::vector<std::string> names;
    names.reserve(languages_.size());
    for (const auto& lang : languages_) {
        names.push_back(lang.name);
    }
    return names;
}

bool LexerManager::applyLanguage(ScintillaAdapter& adapter,
                                std::string_view languageName,
                                bool isDarkTheme) const {
    const LanguageDefinition* lang = getLanguage(languageName);
    if (!lang) {
        lang = getLanguage("Plain Text");
    }

    if (!lang || lang->lexerName.empty()) {
        adapter.send(SCI_SETILEXER, 0, 0);
        applyThemeStyles(adapter, "", isDarkTheme);
        return true;
    }

    Scintilla::ILexer5* pLexer = CreateLexer(lang->lexerName.c_str());
    if (!pLexer) {
        adapter.send(SCI_SETILEXER, 0, 0);
        applyThemeStyles(adapter, "", isDarkTheme);
        return false;
    }

    adapter.send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(pLexer));

    for (size_t i = 0; i < lang->keywordSets.size(); ++i) {
        adapter.send(SCI_SETKEYWORDS, i, reinterpret_cast<sptr_t>(lang->keywordSets[i].c_str()));
    }

    applyThemeStyles(adapter, lang->lexerName, isDarkTheme);
    return true;
}

void LexerManager::applyThemeStyles(ScintillaAdapter& adapter,
                                   const std::string& lexerName,
                                   bool isDarkTheme) const {
    // Base colors
    const sptr_t bg = isDarkTheme ? ColorRGB(30, 30, 30) : ColorRGB(255, 255, 255);
    const sptr_t fg = isDarkTheme ? ColorRGB(212, 212, 212) : ColorRGB(30, 30, 30);
    const sptr_t marginBg = isDarkTheme ? ColorRGB(37, 37, 38) : ColorRGB(240, 240, 240);
    const sptr_t marginFg = isDarkTheme ? ColorRGB(133, 133, 133) : ColorRGB(120, 120, 120);
    const sptr_t selBg = isDarkTheme ? ColorRGB(38, 79, 120) : ColorRGB(173, 214, 255);
    const sptr_t caretColor = isDarkTheme ? ColorRGB(78, 201, 176) : ColorRGB(0, 128, 90); // Green accent

    // Syntax colors
    const sptr_t keyword = isDarkTheme ? ColorRGB(86, 156, 214) : ColorRGB(0, 0, 255);
    const sptr_t type = isDarkTheme ? ColorRGB(78, 201, 176) : ColorRGB(43, 145, 175);
    const sptr_t comment = isDarkTheme ? ColorRGB(106, 153, 85) : ColorRGB(0, 128, 0);
    const sptr_t string = isDarkTheme ? ColorRGB(206, 145, 120) : ColorRGB(163, 21, 21);
    const sptr_t number = isDarkTheme ? ColorRGB(181, 206, 168) : ColorRGB(9, 134, 88);
    const sptr_t prep = isDarkTheme ? ColorRGB(156, 220, 254) : ColorRGB(121, 94, 38);
    const sptr_t op = fg;

    // Apply base styles
    adapter.send(SCI_STYLESETBACK, STYLE_DEFAULT, bg);
    adapter.send(SCI_STYLESETFORE, STYLE_DEFAULT, fg);
    adapter.send(SCI_STYLECLEARALL);

    adapter.send(SCI_STYLESETBACK, STYLE_LINENUMBER, marginBg);
    adapter.send(SCI_STYLESETFORE, STYLE_LINENUMBER, marginFg);

    adapter.send(SCI_SETSELBACK, 1, selBg);
    adapter.send(SCI_SETCARETFORE, caretColor);

    if (lexerName == "cpp") {
        adapter.send(SCI_STYLESETFORE, SCE_C_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_C_COMMENT, comment);
        adapter.send(SCI_STYLESETFORE, SCE_C_COMMENTLINE, comment);
        adapter.send(SCI_STYLESETFORE, SCE_C_COMMENTDOC, comment);
        adapter.send(SCI_STYLESETFORE, SCE_C_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_C_WORD, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_C_WORD2, type);
        adapter.send(SCI_STYLESETFORE, SCE_C_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_C_CHARACTER, string);
        adapter.send(SCI_STYLESETFORE, SCE_C_PREPROCESSOR, prep);
        adapter.send(SCI_STYLESETFORE, SCE_C_OPERATOR, op);
        adapter.send(SCI_STYLESETFORE, SCE_C_IDENTIFIER, fg);
    } else if (lexerName == "python") {
        adapter.send(SCI_STYLESETFORE, SCE_P_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_P_COMMENTLINE, comment);
        adapter.send(SCI_STYLESETFORE, SCE_P_COMMENTBLOCK, comment);
        adapter.send(SCI_STYLESETFORE, SCE_P_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_P_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_P_CHARACTER, string);
        adapter.send(SCI_STYLESETFORE, SCE_P_WORD, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_P_WORD2, type);
        adapter.send(SCI_STYLESETFORE, SCE_P_TRIPLE, string);
        adapter.send(SCI_STYLESETFORE, SCE_P_TRIPLEDOUBLE, string);
        adapter.send(SCI_STYLESETFORE, SCE_P_CLASSNAME, type);
        adapter.send(SCI_STYLESETFORE, SCE_P_DEFNAME, prep);
        adapter.send(SCI_STYLESETFORE, SCE_P_OPERATOR, op);
        adapter.send(SCI_STYLESETFORE, SCE_P_IDENTIFIER, fg);
        adapter.send(SCI_STYLESETFORE, SCE_P_DECORATOR, prep);
    } else if (lexerName == "rust") {
        adapter.send(SCI_STYLESETFORE, SCE_RUST_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_COMMENTBLOCK, comment);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_COMMENTLINE, comment);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_COMMENTBLOCKDOC, comment);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_COMMENTLINEDOC, comment);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_WORD, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_WORD2, type);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_CHARACTER, string);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_MACRO, prep);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_OPERATOR, op);
        adapter.send(SCI_STYLESETFORE, SCE_RUST_IDENTIFIER, fg);
    } else if (lexerName == "json") {
        adapter.send(SCI_STYLESETFORE, SCE_JSON_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_PROPERTYNAME, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_KEYWORD, type);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_LINECOMMENT, comment);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_BLOCKCOMMENT, comment);
        adapter.send(SCI_STYLESETFORE, SCE_JSON_OPERATOR, op);
    } else if (lexerName == "bash") {
        adapter.send(SCI_STYLESETFORE, SCE_SH_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_SH_COMMENTLINE, comment);
        adapter.send(SCI_STYLESETFORE, SCE_SH_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_SH_WORD, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_SH_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_SH_CHARACTER, string);
        adapter.send(SCI_STYLESETFORE, SCE_SH_OPERATOR, op);
        adapter.send(SCI_STYLESETFORE, SCE_SH_IDENTIFIER, fg);
    } else if (lexerName == "sql") {
        adapter.send(SCI_STYLESETFORE, SCE_SQL_DEFAULT, fg);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_COMMENT, comment);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_COMMENTLINE, comment);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_NUMBER, number);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_WORD, keyword);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_STRING, string);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_CHARACTER, string);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_OPERATOR, op);
        adapter.send(SCI_STYLESETFORE, SCE_SQL_IDENTIFIER, fg);
    }
}

} // namespace notepadx
