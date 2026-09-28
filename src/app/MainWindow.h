#pragma once

#include <gtk/gtk.h>
#include "document/DocumentManager.h"
#include "editor/Editor.h"
#include "editor/EditorConfig.h"
#include "ui/TabBar.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace notepadx {

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow() = default;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    MainWindow(MainWindow&&) noexcept = default;
    MainWindow& operator=(MainWindow&&) noexcept = default;

    [[nodiscard]] GtkWidget* window() const noexcept { return window_; }
    [[nodiscard]] Editor* activeEditor() const;
    [[nodiscard]] Document* activeDocument() const;

    Document* newTab();
    bool closeTab(int pageIndex);
    bool closeActiveTab();
    void nextTab();
    void prevTab();
    void switchToTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();

    void show();

private:
    void setupShortcuts();
    void updateWindowTitle();
    static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);

    GtkWidget* window_{nullptr};
    GtkWidget* mainBox_{nullptr};

    TabBar tabBar_;
    DocumentManager docManager_;
    EditorConfig editorConfig_;
    std::unordered_map<std::string, std::unique_ptr<Editor>> editors_;
};

} // namespace notepadx
