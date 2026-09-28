#pragma once

#include <gtk/gtk.h>
#include "document/DocumentManager.h"
#include "document/DocumentStore.h"
#include "editor/Editor.h"
#include "editor/EditorConfig.h"
#include "session/Autosave.h"
#include "session/RecentlyClosed.h"
#include "session/SessionManager.h"
#include "ui/TabBar.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace notepadx {

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    MainWindow(MainWindow&&) noexcept = default;
    MainWindow& operator=(MainWindow&&) noexcept = default;

    [[nodiscard]] GtkWidget* window() const noexcept { return window_; }
    [[nodiscard]] Editor* activeEditor() const;
    [[nodiscard]] Document* activeDocument() const;

    Document* newTab();
    Document* openDocument(std::unique_ptr<Document> doc, std::string_view content);
    bool closeTab(int pageIndex);
    bool closeActiveTab();
    void nextTab();
    void prevTab();
    void switchToTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();
    bool reopenClosedTab();

    void saveCurrentSession();
    void restoreSession();

    void show();

private:
    void setupShortcuts();
    void updateWindowTitle();
    void saveDocumentToStore(const std::string& docId);

    static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);
    static gboolean onDeleteEvent(GtkWidget* widget, GdkEvent* event, gpointer userData);

    GtkWidget* window_{nullptr};
    GtkWidget* mainBox_{nullptr};

    TabBar tabBar_;
    DocumentManager docManager_;
    DocumentStore docStore_;
    RecentlyClosed recentlyClosed_;
    Autosave autosave_;
    guint autosaveTimeoutId_{0};
    EditorConfig editorConfig_;
    std::unordered_map<std::string, std::unique_ptr<Editor>> editors_;
};

} // namespace notepadx
