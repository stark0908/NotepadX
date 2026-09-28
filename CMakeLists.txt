cmake_minimum_required(VERSION 3.20)
project(NotepadX VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Find system dependencies ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

# --- Third-party paths ---
set(SCINTILLA_DIR ${CMAKE_SOURCE_DIR}/third_party/scintilla)
set(LEXILLA_DIR ${CMAKE_SOURCE_DIR}/third_party/lexilla)
set(JSON_DIR ${CMAKE_SOURCE_DIR}/third_party/nlohmann)

# --- Build Scintilla static library via its native makefile ---
set(SCINTILLA_LIB ${SCINTILLA_DIR}/bin/scintilla.a)
add_custom_command(
    OUTPUT ${SCINTILLA_LIB}
    COMMAND make -C ${SCINTILLA_DIR}/gtk GTK3=1 static -j$$(nproc)
    COMMENT "Building Scintilla (static)"
    USES_TERMINAL
)
add_custom_target(scintilla_build DEPENDS ${SCINTILLA_LIB})

# --- Build Lexilla static library via its native makefile ---
set(LEXILLA_LIB ${LEXILLA_DIR}/bin/liblexilla.a)
add_custom_command(
    OUTPUT ${LEXILLA_LIB}
    COMMAND make -C ${LEXILLA_DIR}/src -j$$(nproc)
    COMMENT "Building Lexilla (static)"
    USES_TERMINAL
)
add_custom_target(lexilla_build DEPENDS ${LEXILLA_LIB})

# --- NotepadX sources ---
set(NOTEPADX_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/document/Document.cpp
    src/document/DocumentManager.cpp
    src/editor/Editor.cpp
    src/editor/EditorConfig.cpp
    src/editor/ScintillaAdapter.cpp
    src/ui/TabBar.cpp
)

add_executable(notepadx ${NOTEPADX_SOURCES})

target_include_directories(notepadx PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GTK3_INCLUDE_DIRS}
    ${SCINTILLA_DIR}/include
    ${LEXILLA_DIR}/include
    ${JSON_DIR}
)

target_compile_definitions(notepadx PRIVATE GTK)

target_link_libraries(notepadx PRIVATE
    ${SCINTILLA_LIB}
    ${LEXILLA_LIB}
    ${GTK3_LIBRARIES}
    ${PCRE2_LIBRARIES}
)

target_link_directories(notepadx PRIVATE
    ${GTK3_LIBRARY_DIRS}
    ${PCRE2_LIBRARY_DIRS}
)

target_compile_options(notepadx PRIVATE
    -Wall -Wextra -Wpedantic
    ${GTK3_CFLAGS_OTHER}
)

add_dependencies(notepadx scintilla_build lexilla_build)

# --- Unit Testing ---
find_package(GTest CONFIG REQUIRED)
enable_testing()

add_executable(notepadx_tests
    tests/test_document.cpp
    src/document/Document.cpp
    src/document/DocumentManager.cpp
)

target_include_directories(notepadx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${JSON_DIR}
)

target_link_libraries(notepadx_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

target_compile_options(notepadx_tests PRIVATE
    -Wall -Wextra -Wpedantic
)

include(GoogleTest)
gtest_discover_tests(notepadx_tests)
