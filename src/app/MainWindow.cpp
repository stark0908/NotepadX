#include "app/MainWindow.h"

#include <gdk/gdkkeysyms.h>

namespace notepadx {

MainWindow::MainWindow(GtkApplication* app)
    : window_(gtk_application_window_new(app)),
      mainBox_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
      editorConfig_(EditorConfig::createDefault()) {
    gtk_window_set_title(GTK_WINDOW(window_), "NotepadX");
    gtk_window_set_default_size(GTK_WINDOW(window_), 900, 600);

    gtk_container_add(GTK_CONTAINER(window_), mainBox_);
    gtk_box_pack_start(GTK_BOX(mainBox_), tabBar_.widget(), TRUE, TRUE, 0);

    tabBar_.setTabChangedCallback([this](int /*pageIndex*/, Document* doc) {
        if (doc) {
            docManager_.setActiveDocument(doc);
            updateWindowTitle();
            if (auto* ed = activeEditor()) {
                gtk_widget_grab_focus(ed->widget());
            }
        }
    });

    tabBar_.setTabCloseCallback([this](int pageIndex, Document* /*doc*/) {
        closeTab(pageIndex);
    });

    tabBar_.setTabCloseOthersCallback([this](int keepPageIndex) {
        closeOtherTabs(keepPageIndex);
    });

    tabBar_.setTabCloseAllCallback([this]() {
        closeAllTabs();
    });

    setupShortcuts();

    // Start with one initial tab
    newTab();
}

Document* MainWindow::newTab() {
    Document* doc = docManager_.createUntitled();
    auto editor = std::make_unique<Editor>(editorConfig_);

    editor->setModifiedChangedCallback([this, doc](bool modified) {
        doc->setModified(modified);
        const int idx = tabBar_.indexOfDocument(doc);
        if (idx >= 0) {
            tabBar_.updateTabTitle(idx);
        }
        if (docManager_.activeDocument() == doc) {
            updateWindowTitle();
        }
    });

    GtkWidget* edWidget = editor->widget();
    const std::string docId = doc->id();
    editors_[docId] = std::move(editor);

    tabBar_.addTab(doc, edWidget);
    return doc;
}

bool MainWindow::closeTab(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= tabBar_.count()) {
        return false;
    }

    Document* doc = tabBar_.documentAt(pageIndex);
    if (!doc) {
        return false;
    }

    const std::string docId = doc->id();
    tabBar_.removeTab(pageIndex);
    editors_.erase(docId);
    docManager_.removeDocument(docId);

    if (tabBar_.count() == 0) {
        newTab();
    } else {
        updateWindowTitle();
    }
    return true;
}

bool MainWindow::closeActiveTab() {
    return closeTab(tabBar_.activeIndex());
}

void MainWindow::nextTab() {
    const int total = tabBar_.count();
    if (total > 1) {
        const int next = (tabBar_.activeIndex() + 1) % total;
        tabBar_.setActiveIndex(next);
    }
}

void MainWindow::prevTab() {
    const int total = tabBar_.count();
    if (total > 1) {
        const int prev = (tabBar_.activeIndex() - 1 + total) % total;
        tabBar_.setActiveIndex(prev);
    }
}

void MainWindow::switchToTab(int index) {
    if (index >= 0 && index < tabBar_.count()) {
        tabBar_.setActiveIndex(index);
    }
}

void MainWindow::closeOtherTabs(int keepIndex) {
    if (keepIndex < 0 || keepIndex >= tabBar_.count()) {
        return;
    }

    Document* keepDoc = tabBar_.documentAt(keepIndex);
    if (!keepDoc) {
        return;
    }

    for (int i = tabBar_.count() - 1; i >= 0; --i) {
        if (i != keepIndex) {
            closeTab(i);
        }
    }
}

void MainWindow::closeAllTabs() {
    while (tabBar_.count() > 1) {
        closeTab(0);
    }
    closeTab(0); // This will close the last tab and auto-create a fresh newTab()
}

Editor* MainWindow::activeEditor() const {
    Document* doc = docManager_.activeDocument();
    if (!doc) {
        return nullptr;
    }
    auto it = editors_.find(doc->id());
    if (it != editors_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Document* MainWindow::activeDocument() const {
    return docManager_.activeDocument();
}

void MainWindow::updateWindowTitle() {
    Document* doc = activeDocument();
    if (!doc) {
        gtk_window_set_title(GTK_WINDOW(window_), "NotepadX");
        return;
    }
    std::string title = doc->displayName() + " - NotepadX";
    gtk_window_set_title(GTK_WINDOW(window_), title.c_str());
}

void MainWindow::setupShortcuts() {
    g_signal_connect(window_, "key-press-event", G_CALLBACK(onKeyPress), this);
}

gboolean MainWindow::onKeyPress([[maybe_unused]] GtkWidget* widget, GdkEventKey* event, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    if (!self || !event) {
        return FALSE;
    }

    const guint state = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK);

    // Ctrl+N -> New Tab
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N)) {
        self->newTab();
        return TRUE;
    }

    // Ctrl+W -> Close Tab
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W)) {
        self->closeActiveTab();
        return TRUE;
    }

    // Ctrl+Tab -> Next Tab
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab)) {
        self->nextTab();
        return TRUE;
    }

    // Ctrl+Shift+Tab -> Prev Tab
    if ((state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
        (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab)) {
        self->prevTab();
        return TRUE;
    }

    // Alt+1 through Alt+9 -> Switch to Tab 1-9
    if (state == GDK_MOD1_MASK) {
        if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
            const int targetIdx = static_cast<int>(event->keyval - GDK_KEY_1);
            self->switchToTab(targetIdx);
            return TRUE;
        }
    }

    return FALSE;
}

void MainWindow::show() {
    gtk_widget_show_all(window_);
}

} // namespace notepadx
