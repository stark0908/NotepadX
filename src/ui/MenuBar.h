#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <string>
#include <vector>

namespace notepadx {

class MenuBar {
public:
    struct Callbacks {
        std::function<void()> onNewTab;
        std::function<void()> onOpenFile;
        std::function<void()> onSaveFile;
        std::function<void()> onSaveFileAs;
        std::function<void()> onCloseTab;
        std::function<void()> onReopenTab;
        std::function<void()> onQuit;

        std::function<void()> onUndo;
        std::function<void()> onRedo;
        std::function<void()> onCut;
        std::function<void()> onCopy;
        std::function<void()> onPaste;
        std::function<void()> onSelectAll;
        std::function<void()> onDuplicateLine;
        std::function<void()> onDeleteLine;
        std::function<void()> onMoveLineUp;
        std::function<void()> onMoveLineDown;

        std::function<void()> onFind;
        std::function<void()> onReplace;
        std::function<void()> onFindNext;
        std::function<void()> onFindPrev;
        std::function<void()> onGoToLine;

        std::function<void()> onToggleWordWrap;
        std::function<void()> onToggleLineNumbers;
        std::function<void()> onZoomIn;
        std::function<void()> onZoomOut;
        std::function<void()> onResetZoom;
        std::function<void()> onToggleTheme;

        std::function<void(const std::string& lang)> onSelectLanguage;
        std::function<void()> onAbout;
    };

    explicit MenuBar(const std::vector<std::string>& languages, Callbacks cbs);
    ~MenuBar() = default;

    MenuBar(const MenuBar&) = delete;
    MenuBar& operator=(const MenuBar&) = delete;

    [[nodiscard]] GtkWidget* widget() const noexcept { return menuBar_; }
    void showLanguageMenu(GdkEventButton* event = nullptr);

private:
    void buildMenus(const std::vector<std::string>& languages);

    GtkWidget* menuBar_{nullptr};
    GtkWidget* languageMenu_{nullptr};
    Callbacks cbs_;
};

} // namespace notepadx
