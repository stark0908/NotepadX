#include "app/MainWindow.h"

#include <gdk/gdkkeysyms.h>
#include <fstream>
#include <sstream>

namespace notepadx {

MainWindow::MainWindow(GtkApplication* app)
    : window_(gtk_application_window_new(app)),
      mainBox_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
      autosave_([this](const std::string& docId) { saveDocumentToStore(docId); }, 2000),
      editorConfig_(EditorConfig::createDefault()) {
    gtk_window_set_title(GTK_WINDOW(window_), "NotepadX");
    gtk_window_set_default_size(GTK_WINDOW(window_), 900, 600);

    gtk_container_add(GTK_CONTAINER(window_), mainBox_);
    gtk_box_pack_start(GTK_BOX(mainBox_), tabBar_.widget(), TRUE, TRUE, 0);

    autosave_.setTimerScheduler([this](uint32_t delayMs, std::function<void()> cb) {
        if (autosaveTimeoutId_ != 0) {
            g_source_remove(autosaveTimeoutId_);
            autosaveTimeoutId_ = 0;
        }
        auto* cbPtr = new std::function<void()>(std::move(cb));
        autosaveTimeoutId_ = g_timeout_add(delayMs, +[](gpointer data) -> gboolean {
            auto* fn = static_cast<std::function<void()>*>(data);
            (*fn)();
            delete fn;
            return G_SOURCE_REMOVE;
        }, cbPtr);
    });

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

    g_signal_connect(window_, "delete-event", G_CALLBACK(onDeleteEvent), this);

    setupShortcuts();
    restoreSession();
}

MainWindow::~MainWindow() {
    if (autosaveTimeoutId_ != 0) {
        g_source_remove(autosaveTimeoutId_);
        autosaveTimeoutId_ = 0;
    }
}

Document* MainWindow::newTab() {
    Document* doc = docManager_.createUntitled();
    return openDocument(std::unique_ptr<Document>(docManager_.removeDocument(doc->id())), "");
}

Document* MainWindow::openDocument(std::unique_ptr<Document> doc, std::string_view content) {
    if (!doc) {
        return nullptr;
    }

    Document* rawDoc = docManager_.addDocument(std::move(doc));
    auto editor = std::make_unique<Editor>(editorConfig_);

    if (!content.empty()) {
        editor->adapter().setText(content);
    }

    editor->setModifiedChangedCallback([this, rawDoc](bool modified) {
        rawDoc->setModified(modified);
        const int idx = tabBar_.indexOfDocument(rawDoc);
        if (idx >= 0) {
            tabBar_.updateTabTitle(idx);
        }
        if (docManager_.activeDocument() == rawDoc) {
            updateWindowTitle();
        }
    });

    editor->setContentChangedCallback([this, rawDoc]() {
        if (rawDoc->isUnnamed()) {
            autosave_.markDirty(rawDoc->id());
        }
    });

    GtkWidget* edWidget = editor->widget();
    const std::string docId = rawDoc->id();
    editors_[docId] = std::move(editor);

    tabBar_.addTab(rawDoc, edWidget);
    return rawDoc;
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
    auto it = editors_.find(docId);
    if (it != editors_.end() && doc->isUnnamed()) {
        std::string text = it->second->adapter().getText();
        if (!text.empty()) {
            ClosedTabEntry entry;
            entry.id = docId;
            entry.title = doc->title();
            entry.filePath = doc->filePath();
            entry.content = std::move(text);
            entry.cursorPosition = it->second->adapter().getCurrentPos();
            entry.scrollLine = it->second->adapter().getFirstVisibleLine();
            recentlyClosed_.push(std::move(entry));
            recentlyClosed_.saveToFile(SessionManager::defaultTrashFile());
        } else {
            docStore_.deleteDocumentContent(docId);
        }
    }

    autosave_.markClean(docId);
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
    closeTab(0);
}

bool MainWindow::reopenClosedTab() {
    auto opt = recentlyClosed_.popLatest();
    if (!opt.has_value()) {
        return false;
    }
    recentlyClosed_.saveToFile(SessionManager::defaultTrashFile());

    auto doc = std::make_unique<Document>(opt->id, opt->title, opt->filePath);
    Document* created = openDocument(std::move(doc), opt->content);
    if (created) {
        if (auto* ed = editors_[created->id()].get()) {
            ed->adapter().setCurrentPos(opt->cursorPosition);
            ed->adapter().setFirstVisibleLine(opt->scrollLine);
        }
        return true;
    }
    return false;
}

void MainWindow::saveDocumentToStore(const std::string& docId) {
    auto it = editors_.find(docId);
    if (it != editors_.end()) {
        Document* doc = docManager_.findById(docId);
        if (doc && doc->isUnnamed()) {
            docStore_.saveDocumentContent(docId, it->second->adapter().getText());
        }
    }
}

void MainWindow::saveCurrentSession() {
    autosave_.flushNow();
    SessionState state;
    state.activeTabIndex = tabBar_.activeIndex();

    const int count = tabBar_.count();
    for (int i = 0; i < count; ++i) {
        Document* doc = tabBar_.documentAt(i);
        if (!doc) {
            continue;
        }

        SessionTab tab;
        tab.id = doc->id();
        tab.title = doc->title();
        tab.filePath = doc->filePath();
        tab.isModified = doc->isModified();

        auto it = editors_.find(doc->id());
        if (it != editors_.end()) {
            tab.cursorPosition = it->second->adapter().getCurrentPos();
            tab.scrollLine = it->second->adapter().getFirstVisibleLine();
            if (doc->isUnnamed()) {
                docStore_.saveDocumentContent(doc->id(), it->second->adapter().getText());
            }
        }
        state.tabs.push_back(std::move(tab));
    }

    SessionManager::saveSession(state, SessionManager::defaultSessionFile());
    recentlyClosed_.saveToFile(SessionManager::defaultTrashFile());
}

void MainWindow::restoreSession() {
    recentlyClosed_.loadFromFile(SessionManager::defaultTrashFile());
    SessionState state = SessionManager::loadSession(SessionManager::defaultSessionFile());

    if (state.empty()) {
        newTab();
        return;
    }

    for (const auto& tab : state.tabs) {
        auto doc = std::make_unique<Document>(tab.id, tab.title, tab.filePath);
        std::string content;
        if (doc->isUnnamed()) {
            content = docStore_.loadDocumentContent(doc->id());
        } else if (!doc->filePath().empty() && std::filesystem::exists(doc->filePath())) {
            std::ifstream in(doc->filePath(), std::ios::binary);
            if (in.is_open()) {
                std::ostringstream ss;
                ss << in.rdbuf();
                content = ss.str();
            }
        }

        Document* created = openDocument(std::move(doc), content);
        if (created) {
            if (auto* ed = editors_[created->id()].get()) {
                ed->adapter().setCurrentPos(tab.cursorPosition);
                ed->adapter().setFirstVisibleLine(tab.scrollLine);
                if (!tab.isModified) {
                    ed->setSavePoint();
                }
            }
        }
    }

    if (tabBar_.count() == 0) {
        newTab();
    } else {
        switchToTab(state.activeTabIndex);
    }
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

    // Ctrl+Shift+T -> Reopen last closed tab
    if ((state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
        (event->keyval == GDK_KEY_t || event->keyval == GDK_KEY_T)) {
        self->reopenClosedTab();
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

gboolean MainWindow::onDeleteEvent([[maybe_unused]] GtkWidget* widget,
                                  [[maybe_unused]] GdkEvent* event,
                                  gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    if (self) {
        self->saveCurrentSession();
    }
    return FALSE; // Allow default window destroy
}

void MainWindow::show() {
    gtk_widget_show_all(window_);
}

} // namespace notepadx
