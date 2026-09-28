#include "app/MainWindow.h"

#include <gdk/gdkkeysyms.h>
#include <filesystem>
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
    setupDragAndDrop();
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

    const std::string filePath = doc->filePath();
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

    if (!filePath.empty()) {
        fileWatcher_.watch(filePath, [this](const std::string& path) {
            onExternalFileChanged(path);
        });
    }

    return rawDoc;
}

Document* MainWindow::openFile(const std::string& filePath) {
    if (filePath.empty()) {
        return nullptr;
    }

    std::error_code ec;
    const std::string absPath = std::filesystem::canonical(filePath, ec).string();
    const std::string targetPath = ec ? filePath : absPath;

    if (Document* existing = docManager_.findByPath(targetPath)) {
        const int idx = tabBar_.indexOfDocument(existing);
        if (idx >= 0) {
            tabBar_.setActiveIndex(idx);
        }
        return existing;
    }

    std::ifstream in(targetPath, std::ios::binary);
    if (!in.is_open()) {
        return nullptr;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    // If current tab is unnamed, empty, and unmodified -> reuse it
    Document* current = activeDocument();
    if (current && current->isUnnamed() && !current->isModified()) {
        auto it = editors_.find(current->id());
        if (it != editors_.end() && it->second->adapter().getLength() == 0) {
            current->setFilePath(targetPath);
            it->second->adapter().setText(content);
            it->second->setSavePoint();
            const int idx = tabBar_.indexOfDocument(current);
            if (idx >= 0) {
                tabBar_.updateTabTitle(idx);
            }
            updateWindowTitle();
            fileWatcher_.watch(targetPath, [this](const std::string& p) {
                onExternalFileChanged(p);
            });
            return current;
        }
    }

    auto doc = std::make_unique<Document>("", "", targetPath);
    return openDocument(std::move(doc), content);
}

bool MainWindow::openFileDialog() {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Open File",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    bool success = false;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            success = (openFile(filename) != nullptr);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return success;
}

bool MainWindow::saveActiveDocument() {
    Document* doc = activeDocument();
    if (!doc) {
        return false;
    }

    if (doc->isUnnamed()) {
        return saveActiveDocumentAs();
    }
    return saveDocument(doc, doc->filePath());
}

bool MainWindow::saveActiveDocumentAs() {
    Document* doc = activeDocument();
    if (!doc) {
        return false;
    }

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Save File As",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (!doc->isUnnamed()) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), doc->filePath().c_str());
    } else {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), doc->title().c_str());
    }

    bool success = false;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            success = saveDocument(doc, filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return success;
}

bool MainWindow::saveDocument(Document* doc, const std::string& targetPath) {
    if (!doc || targetPath.empty()) {
        return false;
    }

    auto it = editors_.find(doc->id());
    if (it == editors_.end()) {
        return false;
    }

    const std::string content = it->second->adapter().getText();
    const std::string oldPath = doc->filePath();

    fileWatcher_.ignoreNextChange(targetPath);

    const auto tmpPath = targetPath + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, targetPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    if (doc->isUnnamed()) {
        docStore_.deleteDocumentContent(doc->id());
        autosave_.markClean(doc->id());
    }

    if (!oldPath.empty() && oldPath != targetPath) {
        fileWatcher_.unwatch(oldPath);
    }

    doc->setFilePath(targetPath);
    it->second->setSavePoint();

    const int idx = tabBar_.indexOfDocument(doc);
    if (idx >= 0) {
        tabBar_.updateTabTitle(idx);
    }
    updateWindowTitle();

    fileWatcher_.watch(targetPath, [this](const std::string& p) {
        onExternalFileChanged(p);
    });

    return true;
}

bool MainWindow::promptToSaveIfModified(Document* doc) {
    if (!doc || !doc->isModified()) {
        return true;
    }

    // Unnamed documents are frictionlessly persisted to trash without prompts
    if (doc->isUnnamed()) {
        return true;
    }

    std::string msg = "Save changes to \"" + doc->title() + "\" before closing?";
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(window_),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "%s", msg.c_str()
    );

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        "If you close without saving, your changes will be discarded."
    );

    gtk_dialog_add_button(GTK_DIALOG(dialog), "Don't Save", GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Save", GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        return saveDocument(doc, doc->filePath());
    }
    if (response == GTK_RESPONSE_NO) {
        return true; // Discard changes
    }
    return false; // Cancel
}

void MainWindow::onExternalFileChanged(const std::string& path) {
    Document* doc = docManager_.findByPath(path);
    if (!doc) {
        return;
    }

    std::string msg = "\"" + doc->title() + "\" has been modified by another program.\nDo you want to reload it?";
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(window_),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s", msg.c_str()
    );

    if (doc->isModified()) {
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "Reloading will discard your current unsaved modifications."
        );
    }

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        std::ifstream in(path, std::ios::binary);
        if (in.is_open()) {
            std::ostringstream ss;
            ss << in.rdbuf();
            auto it = editors_.find(doc->id());
            if (it != editors_.end()) {
                it->second->adapter().setText(ss.str());
                it->second->setSavePoint();
                const int idx = tabBar_.indexOfDocument(doc);
                if (idx >= 0) {
                    tabBar_.updateTabTitle(idx);
                }
                updateWindowTitle();
            }
        }
    }
}

bool MainWindow::closeTab(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= tabBar_.count()) {
        return false;
    }

    Document* doc = tabBar_.documentAt(pageIndex);
    if (!doc) {
        return false;
    }

    if (!promptToSaveIfModified(doc)) {
        return false;
    }

    const std::string docId = doc->id();
    auto it = editors_.find(docId);
    if (it != editors_.end()) {
        if (doc->isUnnamed()) {
            std::string text = it->second->adapter().getText();
            if (!text.empty()) {
                ClosedTabEntry entry;
                entry.id = docId;
                entry.title = doc->title();
                entry.filePath = doc->filePath();
                entry.content = std::move(text);
                entry.cursorPosition = it->second->adapter().getCurrentPos();
                entry.scrollLine = it->second->adapter().getFirstVisibleLine();
                entry.closedTimestamp = 0;
                recentlyClosed_.push(std::move(entry));
                recentlyClosed_.saveToFile(SessionManager::defaultTrashFile());
            } else {
                docStore_.deleteDocumentContent(docId);
            }
        } else if (!doc->filePath().empty()) {
            fileWatcher_.unwatch(doc->filePath());
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
            if (!closeTab(i)) {
                break; // If user cancelled prompt
            }
        }
    }
}

void MainWindow::closeAllTabs() {
    while (tabBar_.count() > 1) {
        if (!closeTab(0)) {
            return;
        }
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

void MainWindow::setupDragAndDrop() {
    gtk_drag_dest_set(window_, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(window_);
    g_signal_connect(window_, "drag-data-received", G_CALLBACK(onDragDataReceived), this);
}

void MainWindow::onDragDataReceived([[maybe_unused]] GtkWidget* widget,
                                   GdkDragContext* context,
                                   [[maybe_unused]] gint x,
                                   [[maybe_unused]] gint y,
                                   GtkSelectionData* data,
                                   [[maybe_unused]] guint info,
                                   guint time,
                                   gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    if (!self || !data) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar** uris = gtk_selection_data_get_uris(data);
    bool anyOpened = false;
    if (uris) {
        for (gchar** u = uris; *u; ++u) {
            gchar* filename = g_filename_from_uri(*u, nullptr, nullptr);
            if (filename) {
                if (self->openFile(filename)) {
                    anyOpened = true;
                }
                g_free(filename);
            }
        }
        g_strfreev(uris);
    }
    gtk_drag_finish(context, anyOpened ? TRUE : FALSE, FALSE, time);
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

    // Ctrl+O -> Open File
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_o || event->keyval == GDK_KEY_O)) {
        self->openFileDialog();
        return TRUE;
    }

    // Ctrl+Shift+S -> Save As
    if ((state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
        (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S)) {
        self->saveActiveDocumentAs();
        return TRUE;
    }

    // Ctrl+S -> Save
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S)) {
        self->saveActiveDocument();
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
