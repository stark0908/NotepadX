#include "ui/MenuBar.h"

namespace notepadx {

namespace {

GtkWidget* createMenuItem(const char* label, const char* accel, std::function<void()> cb) {
    std::string text = label;
    if (accel && *accel) {
        text += "\t\t";
        text += accel;
    }
    GtkWidget* item = gtk_menu_item_new_with_label(text.c_str());
    if (cb) {
        auto* cbPtr = new std::function<void()>(std::move(cb));
        g_signal_connect_data(item, "activate", G_CALLBACK(+[](GtkMenuItem* /*i*/, gpointer data) {
            auto* fn = static_cast<std::function<void()>*>(data);
            (*fn)();
        }), cbPtr, +[](gpointer data, GClosure* /*c*/) {
            delete static_cast<std::function<void()>*>(data);
        }, static_cast<GConnectFlags>(0));
    }
    return item;
}

void addSep(GtkWidget* menu) {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

} // namespace

MenuBar::MenuBar(const std::vector<std::string>& languages, Callbacks cbs)
    : menuBar_(gtk_menu_bar_new()),
      cbs_(std::move(cbs)) {
    buildMenus(languages);
}

void MenuBar::buildMenus(const std::vector<std::string>& languages) {
    // --- File Menu ---
    GtkWidget* fileMenu = gtk_menu_new();
    GtkWidget* fileTop = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileTop), fileMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("New Tab", "Ctrl+N", cbs_.onNewTab));
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Open...", "Ctrl+O", cbs_.onOpenFile));
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Save", "Ctrl+S", cbs_.onSaveFile));
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Save As...", "Ctrl+Shift+S", cbs_.onSaveFileAs));
    addSep(fileMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Close Tab", "Ctrl+W", cbs_.onCloseTab));
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Reopen Closed Tab", "Ctrl+Shift+T", cbs_.onReopenTab));
    addSep(fileMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), createMenuItem("Quit", "Ctrl+Q", cbs_.onQuit));

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), fileTop);

    // --- Edit Menu ---
    GtkWidget* editMenu = gtk_menu_new();
    GtkWidget* editTop = gtk_menu_item_new_with_label("Edit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(editTop), editMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Undo", "Ctrl+Z", cbs_.onUndo));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Redo", "Ctrl+Y", cbs_.onRedo));
    addSep(editMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Cut", "Ctrl+X", cbs_.onCut));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Copy", "Ctrl+C", cbs_.onCopy));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Paste", "Ctrl+V", cbs_.onPaste));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Select All", "Ctrl+A", cbs_.onSelectAll));
    addSep(editMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Duplicate Line", "Ctrl+D", cbs_.onDuplicateLine));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Delete Line", "Ctrl+L", cbs_.onDeleteLine));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Move Line Up", "Alt+Up", cbs_.onMoveLineUp));
    gtk_menu_shell_append(GTK_MENU_SHELL(editMenu), createMenuItem("Move Line Down", "Alt+Down", cbs_.onMoveLineDown));

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), editTop);

    // --- Search Menu ---
    GtkWidget* searchMenu = gtk_menu_new();
    GtkWidget* searchTop = gtk_menu_item_new_with_label("Search");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(searchTop), searchMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(searchMenu), createMenuItem("Find...", "Ctrl+F", cbs_.onFind));
    gtk_menu_shell_append(GTK_MENU_SHELL(searchMenu), createMenuItem("Replace...", "Ctrl+H", cbs_.onReplace));
    gtk_menu_shell_append(GTK_MENU_SHELL(searchMenu), createMenuItem("Find Next", "F3", cbs_.onFindNext));
    gtk_menu_shell_append(GTK_MENU_SHELL(searchMenu), createMenuItem("Find Previous", "Shift+F3", cbs_.onFindPrev));
    addSep(searchMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(searchMenu), createMenuItem("Go to Line...", "Ctrl+G", cbs_.onGoToLine));

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), searchTop);

    // --- View Menu ---
    GtkWidget* viewMenu = gtk_menu_new();
    GtkWidget* viewTop = gtk_menu_item_new_with_label("View");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewTop), viewMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Toggle Word Wrap", "Alt+Z", cbs_.onToggleWordWrap));
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Toggle Line Numbers", "", cbs_.onToggleLineNumbers));
    addSep(viewMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Zoom In", "Ctrl++", cbs_.onZoomIn));
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Zoom Out", "Ctrl+-", cbs_.onZoomOut));
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Reset Zoom", "Ctrl+0", cbs_.onResetZoom));
    addSep(viewMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), createMenuItem("Toggle Dark/Light Theme", "", cbs_.onToggleTheme));

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), viewTop);

    // --- Language Menu ---
    languageMenu_ = gtk_menu_new();
    GtkWidget* langTop = gtk_menu_item_new_with_label("Language");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(langTop), languageMenu_);

    struct LangAction {
        MenuBar* self;
        std::string name;
    };

    for (const auto& lang : languages) {
        GtkWidget* lItem = gtk_menu_item_new_with_label(lang.c_str());
        auto* action = new LangAction{this, lang};
        g_signal_connect_data(lItem, "activate", G_CALLBACK(+[](GtkMenuItem* /*i*/, gpointer data) {
            auto* a = static_cast<LangAction*>(data);
            if (a->self->cbs_.onSelectLanguage) {
                a->self->cbs_.onSelectLanguage(a->name);
            }
        }), action, +[](gpointer data, GClosure* /*c*/) {
            delete static_cast<LangAction*>(data);
        }, static_cast<GConnectFlags>(0));

        gtk_menu_shell_append(GTK_MENU_SHELL(languageMenu_), lItem);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), langTop);

    // --- Help Menu ---
    GtkWidget* helpMenu = gtk_menu_new();
    GtkWidget* helpTop = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(helpTop), helpMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(helpMenu), createMenuItem("About NotepadX", "", cbs_.onAbout));

    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar_), helpTop);

    gtk_widget_show_all(menuBar_);
}

void MenuBar::showLanguageMenu(GdkEventButton* event) {
    if (languageMenu_) {
        gtk_widget_show_all(languageMenu_);
        gtk_menu_popup_at_pointer(GTK_MENU(languageMenu_), reinterpret_cast<const GdkEvent*>(event));
    }
}

} // namespace notepadx
