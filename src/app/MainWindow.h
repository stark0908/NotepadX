#pragma once

#include <gtk/gtk.h>
#include "document/DocumentManager.h"
#include "document/DocumentStore.h"
#include "editor/Editor.h"
#include "editor/EditorConfig.h"
#include "platform/FileWatcher.h"
#include "search/Search.h"
#include "session/Autosave.h"
#include "session/RecentlyClosed.h"
#include "session/SessionManager.h"
#include "syntax/LexerManager.h"
#include "ui/SearchBar.h"
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
    Document* openFile(const std::string& filePath);
    bool openFileDialog();

    bool saveActiveDocument();
    bool saveActiveDocumentAs();
    bool saveDocument(Document* doc, const std::string& targetPath);

    bool closeTab(int pageIndex);
    bool closeActiveTab();
    void nextTab();
    void prevTab();
    void switchToTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();
    bool reopenClosedTab();

    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void findNextOrPrev(const SearchOptions& opt, bool backward);
    void highlightMatches(const SearchOptions& opt);
    void replaceCurrent(const SearchOptions& opt);
    void replaceAllMatches(const SearchOptions& opt);
    void clearSearchHighlights();

    void saveCurrentSession();
    void restoreSession();

    void show();

private:
    void setupShortcuts();
    void setupDragAndDrop();
    void setupSearch();
    void updateWindowTitle();
    void saveDocumentToStore(const std::string& docId);
    bool promptToSaveIfModified(Document* doc);
    void onExternalFileChanged(const std::string& path);

    static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);
    static gboolean onDeleteEvent(GtkWidget* widget, GdkEvent* event, gpointer userData);
    static void onDragDataReceived(GtkWidget* widget, GdkDragContext* context,
                                  gint x, gint y, GtkSelectionData* data,
                                  guint info, guint time, gpointer userData);

    GtkWidget* window_{nullptr};
    GtkWidget* mainBox_{nullptr};

    TabBar tabBar_;
    SearchBar searchBar_;
    SearchEngine searchEngine_;
    DocumentManager docManager_;
    DocumentStore docStore_;
    RecentlyClosed recentlyClosed_;
    Autosave autosave_;
    guint autosaveTimeoutId_{0};
    FileWatcher fileWatcher_;
    LexerManager lexerManager_;
    EditorConfig editorConfig_;
    std::unordered_map<std::string, std::unique_ptr<Editor>> editors_;
};

} // namespace notepadx
