#include "app/MainWindow.h"

#include <gdk/gdkkeysyms.h>
#include "platform/Encoding.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace notepadx {

MainWindow::MainWindow(GtkApplication* app)
    : window_(gtk_application_window_new(app)),
      mainBox_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
      autosave_([this](const std::string& docId) { saveDocumentToStore(docId); }, 500) {
    settings_.loadFromFile();
    setupMenuBar();

    gtk_window_set_title(GTK_WINDOW(window_), "NotepadX");
    gtk_window_set_default_size(GTK_WINDOW(window_), 900, 600);
    setupAppIcon();

    gtk_container_add(GTK_CONTAINER(window_), mainBox_);
    gtk_box_pack_start(GTK_BOX(mainBox_), menuBar_->widget(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox_), tabBar_.widget(), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox_), searchBar_.widget(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox_), statusBar_.widget(), FALSE, FALSE, 0);

    statusBar_.setLanguageClickedCallback([this]() {
        menuBar_->showLanguageMenu();
    });
    statusBar_.setEolClickedCallback([this]() {
        if (auto* ed = activeEditor()) {
            const int cur = ed->config().eolMode;
            const int next = (cur == 0) ? 2 : 0;
            auto cfg = ed->config();
            cfg.eolMode = next;
            ed->applyConfig(cfg);
            ed->adapter().send(SCI_CONVERTEOLS, next);
            statusBar_.updateEol(next);
        }
    });

    autosave_.setTimerScheduler([this](uint32_t delayMs, std::function<void()> cb) {
        if (autosaveTimeoutId_ != 0) {
            g_source_remove(autosaveTimeoutId_);
            autosaveTimeoutId_ = 0;
        }
        struct TimeoutData {
            MainWindow* self;
            std::function<void()> cb;
        };
        auto* data = new TimeoutData{this, std::move(cb)};
        autosaveTimeoutId_ = g_timeout_add(delayMs, +[](gpointer ptr) -> gboolean {
            auto* d = static_cast<TimeoutData*>(ptr);
            d->self->autosaveTimeoutId_ = 0;
            d->cb();
            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    });

    tabBar_.setTabChangedCallback([this](int /*pageIndex*/, Document* doc) {
        if (doc) {
            docManager_.setActiveDocument(doc);
            updateWindowTitle();
            updateStatusBar();
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
    setupSearch();
    restoreSession();
}

MainWindow::~MainWindow() {
    if (autosaveTimeoutId_ != 0) {
        g_source_remove(autosaveTimeoutId_);
        autosaveTimeoutId_ = 0;
    }
}

Document* MainWindow::newTab() {
    return openDocument(docManager_.createUntitledDocument(), "");
}

Document* MainWindow::openDocument(std::unique_ptr<Document> doc, std::string_view content) {
    if (!doc) {
        return nullptr;
    }

    const std::string filePath = doc->filePath();
    Document* rawDoc = docManager_.addDocument(std::move(doc));
    auto editor = std::make_unique<Editor>(settings_.editorConfig());

    if (!content.empty()) {
        editor->adapter().setText(content);
    }

    const std::string lang = (rawDoc->language().empty() || rawDoc->language() == "Plain Text") && !rawDoc->filePath().empty()
        ? lexerManager_.detectLanguage(rawDoc->filePath())
        : rawDoc->language();
    rawDoc->setLanguage(lang);
    lexerManager_.applyLanguage(editor->adapter(), lang, settings_.isDarkTheme());

    // Setup search highlight indicator
    constexpr int kSearchIndicator = 8;
    editor->adapter().send(SCI_INDICSETSTYLE, kSearchIndicator, INDIC_ROUNDBOX);
    editor->adapter().send(SCI_INDICSETFORE, kSearchIndicator, 0x00D7FF); // Gold / orange
    editor->adapter().send(SCI_INDICSETALPHA, kSearchIndicator, 100);
    editor->adapter().send(SCI_INDICSETOUTLINEALPHA, kSearchIndicator, 180);

    editor->setModifiedChangedCallback([this, rawDoc](bool modified) {
        rawDoc->setModified(modified);
        updateAllTabTitles();
        if (docManager_.activeDocument() == rawDoc) {
            updateWindowTitle();
            updateStatusBar();
        }
    });

    editor->setContentChangedCallback([this, rawDoc]() {
        if (rawDoc->isUnnamed()) {
            autosave_.markDirty(rawDoc->id());
        }
    });

    editor->setUpdateUiCallback([this]() {
        updateStatusBar();
    });

    editor->setMarginClickCallback([this](int line, int margin) {
        if (margin == 0 || margin == 1) {
            toggleBookmark(line);
        }
    });

    GtkWidget* edWidget = editor->widget();
    const std::string docId = rawDoc->id();
    editors_[docId] = std::move(editor);

    tabBar_.addTab(rawDoc, edWidget);
    updateAllTabTitles();

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
    const std::string rawData = ss.str();

    const auto detected = Encoding::detect(rawData);
    const std::string content = Encoding::toUtf8(rawData, detected.type);

    settings_.addRecentFile(targetPath);
    settings_.saveToFile();
    if (menuBar_) {
        menuBar_->updateRecentFiles(settings_.recentFiles());
    }

    // If current tab is unnamed, empty, and unmodified -> reuse it
    Document* current = activeDocument();
    if (current && current->isUnnamed() && !current->isModified()) {
        auto it = editors_.find(current->id());
        if (it != editors_.end() && it->second->adapter().getLength() == 0) {
            current->setFilePath(targetPath);
            current->setEncoding(detected.name);
            it->second->adapter().setText(content);
            it->second->setSavePoint();
            const std::string lang = lexerManager_.detectLanguage(targetPath);
            current->setLanguage(lang);
            lexerManager_.applyLanguage(it->second->adapter(), lang, settings_.isDarkTheme());
            updateAllTabTitles();
            updateWindowTitle();
            updateStatusBar();
            fileWatcher_.watch(targetPath, [this](const std::string& p) {
                onExternalFileChanged(p);
            });
            return current;
        }
    }

    auto doc = std::make_unique<Document>("", "", targetPath);
    doc->setEncoding(detected.name);
    doc->setLanguage(lexerManager_.detectLanguage(targetPath));
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

    EncodingType encType = EncodingType::Utf8;
    if (doc->encoding() == "UTF-8 BOM") encType = EncodingType::Utf8Bom;
    else if (doc->encoding() == "ISO-8859-1") encType = EncodingType::Latin1;
    else if (doc->encoding() == "UTF-16 LE") encType = EncodingType::Utf16Le;
    else if (doc->encoding() == "UTF-16 BE") encType = EncodingType::Utf16Be;

    const std::string encoded = Encoding::fromUtf8(content, encType);

    std::error_code canEc;
    const std::filesystem::path resolvedPath = std::filesystem::is_symlink(targetPath, canEc)
        ? std::filesystem::canonical(targetPath, canEc)
        : std::filesystem::path(targetPath);
    const std::string savePath = (canEc ? targetPath : resolvedPath.string());

    fileWatcher_.ignoreNextChange(targetPath);
    if (savePath != targetPath) {
        fileWatcher_.ignoreNextChange(savePath);
    }

    const auto tmpPath = savePath + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        out.flush();
        if (!out.good()) {
            std::error_code rmEc;
            std::filesystem::remove(tmpPath, rmEc);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, savePath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    settings_.addRecentFile(targetPath);
    settings_.saveToFile();
    if (menuBar_) {
        menuBar_->updateRecentFiles(settings_.recentFiles());
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

    const std::string lang = lexerManager_.detectLanguage(targetPath);
    doc->setLanguage(lang);
    lexerManager_.applyLanguage(it->second->adapter(), lang, settings_.isDarkTheme());

    updateAllTabTitles();
    updateWindowTitle();
    updateStatusBar();

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

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
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
            const std::string rawData = ss.str();
            const auto detected = Encoding::detect(rawData);
            const std::string content = Encoding::toUtf8(rawData, detected.type);
            doc->setEncoding(detected.name);

            auto it = editors_.find(doc->id());
            if (it != editors_.end()) {
                it->second->adapter().setText(content);
                it->second->setSavePoint();
                const int idx = tabBar_.indexOfDocument(doc);
                if (idx >= 0) {
                    tabBar_.updateTabTitle(idx);
                }
                updateWindowTitle();
                updateStatusBar();
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
    updateAllTabTitles();

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

void MainWindow::updateAllTabTitles() {
    const int count = tabBar_.count();
    for (int i = 0; i < count; ++i) {
        Document* doc = tabBar_.documentAt(i);
        if (doc) {
            tabBar_.updateTabTitle(i, docManager_.displayName(doc), doc->filePath());
        }
    }
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
        tab.language = doc->language();

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
        doc->setLanguage(tab.language);
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

    docManager_.syncUntitledCounterWithExisting();
    updateAllTabTitles();

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
    std::string title = docManager_.displayName(doc) + " - NotepadX";
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

void MainWindow::setupAppIcon() {
    gtk_window_set_icon_name(GTK_WINDOW(window_), "notepadx");
    gtk_window_set_default_icon_name("notepadx");

    // Add local search paths to GTK's icon theme
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if (theme) {
        gtk_icon_theme_append_search_path(theme, "data/icons");
        gtk_icon_theme_append_search_path(theme, "../data/icons");
        if (const char* appdir = getenv("APPDIR")) {
            std::string appIcons = std::string(appdir) + "/usr/share/icons";
            gtk_icon_theme_append_search_path(theme, appIcons.c_str());
        }
    }

    // On Wayland, compositors match icons via desktop entry app_id and ignore _NET_WM_ICON.
    // Skip synchronous rasterization to avoid blocking startup presentation.
    GdkDisplay* display = gdk_display_get_default();
    const bool isWayland = display && (g_strcmp0(G_OBJECT_TYPE_NAME(display), "GdkWaylandDisplay") == 0);
    if (isWayland) {
        return;
    }

    // Locate the SVG icon file across local dev, AppImage, or system install paths
    std::string iconPath;
    const std::vector<std::string> candidates = {
        "data/icons/hicolor/scalable/apps/notepadx.svg",
        "../data/icons/hicolor/scalable/apps/notepadx.svg",
        "/usr/share/icons/hicolor/scalable/apps/notepadx.svg"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) {
            iconPath = c;
            break;
        }
    }
    if (iconPath.empty()) {
        if (const char* appdir = getenv("APPDIR")) {
            std::string p = std::string(appdir) + "/usr/share/icons/hicolor/scalable/apps/notepadx.svg";
            if (std::filesystem::exists(p)) {
                iconPath = p;
            }
        }
    }

    if (!iconPath.empty()) {
        // Multi-resolution pixbuf list deferred to idle for X11 / XWayland
        struct IconIdleData {
            GtkWidget* window;
            std::string path;
        };
        auto* data = new IconIdleData{window_, std::move(iconPath)};
        g_idle_add(+[](gpointer p) -> gboolean {
            auto* d = static_cast<IconIdleData*>(p);
            GList* iconList = nullptr;
            for (int sz : {16, 24, 32, 48, 64, 128, 256}) {
                GError* err = nullptr;
                GdkPixbuf* pix = gdk_pixbuf_new_from_file_at_scale(d->path.c_str(), sz, sz, TRUE, &err);
                if (pix) {
                    iconList = g_list_append(iconList, pix);
                } else {
                    g_clear_error(&err);
                }
            }
            if (iconList) {
                gtk_window_set_default_icon_list(iconList);
                if (GTK_IS_WINDOW(d->window)) {
                    gtk_window_set_icon_list(GTK_WINDOW(d->window), iconList);
                }
                g_list_free_full(iconList, g_object_unref);
            }
            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    }
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

    // Ctrl+F -> Find
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F)) {
        self->showFindBar(false);
        return TRUE;
    }

    // Ctrl+H -> Replace
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_h || event->keyval == GDK_KEY_H)) {
        self->showFindBar(true);
        return TRUE;
    }

    // Ctrl+G -> Go to Line
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G)) {
        self->goToLineDialog();
        return TRUE;
    }

    // Ctrl+D -> Duplicate Line
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D)) {
        self->duplicateLine();
        return TRUE;
    }

    // Ctrl+L -> Delete Line
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L)) {
        self->deleteLine();
        return TRUE;
    }

    // Alt+Up -> Move Line Up
    if ((state == GDK_MOD1_MASK) && (event->keyval == GDK_KEY_Up)) {
        self->moveLineUp();
        return TRUE;
    }

    // Alt+Down -> Move Line Down
    if ((state == GDK_MOD1_MASK) && (event->keyval == GDK_KEY_Down)) {
        self->moveLineDown();
        return TRUE;
    }

    // Alt+Z -> Toggle Word Wrap
    if ((state == GDK_MOD1_MASK) && (event->keyval == GDK_KEY_z || event->keyval == GDK_KEY_Z)) {
        self->toggleWordWrap();
        return TRUE;
    }

    // Ctrl++ / Ctrl+= -> Zoom In
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_equal || event->keyval == GDK_KEY_KP_Add)) {
        self->zoomIn();
        return TRUE;
    }

    // Ctrl+- -> Zoom Out
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract)) {
        self->zoomOut();
        return TRUE;
    }

    // Ctrl+0 -> Reset Zoom
    if ((state == GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_0 || event->keyval == GDK_KEY_KP_0)) {
        self->resetZoom();
        return TRUE;
    }

    // F3 -> Find Next
    if (event->keyval == GDK_KEY_F3) {
        const bool backward = (state & GDK_SHIFT_MASK) != 0;
        self->findNextOrPrev(self->searchBar_.currentOptions(), backward);
        return TRUE;
    }

    // Ctrl+Shift+F2 -> Clear All Bookmarks
    if ((state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) && event->keyval == GDK_KEY_F2) {
        self->clearAllBookmarks();
        return TRUE;
    }

    // Ctrl+F2 -> Toggle Bookmark
    if ((state == GDK_CONTROL_MASK) && event->keyval == GDK_KEY_F2) {
        self->toggleBookmark();
        return TRUE;
    }

    // Shift+F2 -> Previous Bookmark
    if ((state == GDK_SHIFT_MASK) && event->keyval == GDK_KEY_F2) {
        self->prevBookmark();
        return TRUE;
    }

    // F2 -> Next Bookmark
    if (state == 0 && event->keyval == GDK_KEY_F2) {
        self->nextBookmark();
        return TRUE;
    }

    // Esc -> Hide Find Bar
    if (event->keyval == GDK_KEY_Escape && self->searchBar_.isVisible()) {
        self->hideFindBar();
        return TRUE;
    }

    return FALSE;
}

void MainWindow::setupSearch() {
    searchBar_.setFindCallback([this](const SearchOptions& opt, bool backward) {
        findNextOrPrev(opt, backward);
    });

    searchBar_.setQueryChangedCallback([this](const SearchOptions& opt) {
        highlightMatches(opt);
    });

    searchBar_.setReplaceCallback([this](const SearchOptions& opt) {
        replaceCurrent(opt);
    });

    searchBar_.setReplaceAllCallback([this](const SearchOptions& opt) {
        replaceAllMatches(opt);
    });

    searchBar_.setCloseCallback([this]() {
        clearSearchHighlights();
        if (auto* ed = activeEditor()) {
            gtk_widget_grab_focus(ed->widget());
        }
    });
}

void MainWindow::showFindBar(bool replaceMode) {
    searchBar_.show(replaceMode);
}

void MainWindow::hideFindBar() {
    searchBar_.hide();
}

void MainWindow::clearSearchHighlights() {
    auto* ed = activeEditor();
    if (!ed) return;
    const size_t len = ed->adapter().getLength();
    constexpr int kSearchIndicator = 8;
    ed->adapter().send(SCI_SETINDICATORCURRENT, kSearchIndicator);
    ed->adapter().send(SCI_INDICATORCLEARRANGE, 0, len);
}

void MainWindow::highlightMatches(const SearchOptions& opt) {
    auto* ed = activeEditor();
    if (!ed) return;

    clearSearchHighlights();

    if (opt.query.empty()) {
        searchBar_.setStatusText("");
        return;
    }

    const std::string text = ed->adapter().getText();
    auto matches = searchEngine_.findAll(text, opt);
    if (matches.empty()) {
        searchBar_.setStatusText("No matches found");
        return;
    }

    constexpr int kSearchIndicator = 8;
    ed->adapter().send(SCI_SETINDICATORCURRENT, kSearchIndicator);
    for (const auto& m : matches) {
        ed->adapter().send(SCI_INDICATORFILLRANGE, m.start, m.length());
    }

    searchBar_.setStatusText(std::to_string(matches.size()) + " matches");
}

void MainWindow::findNextOrPrev(const SearchOptions& opt, bool backward) {
    auto* ed = activeEditor();
    if (!ed || opt.query.empty()) return;

    const std::string text = ed->adapter().getText();
    const size_t curPos = static_cast<size_t>(ed->adapter().getCurrentPos());
    const size_t anchor = static_cast<size_t>(ed->adapter().send(SCI_GETANCHOR));

    SearchResult res;
    if (backward) {
        const size_t searchPos = std::min(curPos, anchor);
        res = searchEngine_.findPrevious(text, searchPos, opt);
    } else {
        const size_t searchPos = std::max(curPos, anchor);
        res = searchEngine_.findNext(text, searchPos, opt);
    }

    if (res.found) {
        ed->adapter().send(SCI_SETSEL, res.startPos, res.endPos);
        ed->adapter().send(SCI_SCROLLCARET);
        searchBar_.setStatusText(res.statusText);
    } else {
        searchBar_.setStatusText(res.statusText);
    }

    highlightMatches(opt);
}

void MainWindow::replaceCurrent(const SearchOptions& opt) {
    auto* ed = activeEditor();
    if (!ed || opt.query.empty()) return;

    const sptr_t selStart = ed->adapter().send(SCI_GETSELECTIONSTART);
    const sptr_t selEnd = ed->adapter().send(SCI_GETSELECTIONEND);

    if (selEnd > selStart) {
        std::string text = ed->adapter().getText();
        if (static_cast<size_t>(selEnd) <= text.size()) {
            std::string selText = text.substr(selStart, selEnd - selStart);
            RegexEngine re;
            if (re.compile(opt.query, opt.caseSensitive, opt.wholeWord, opt.isRegex)) {
                MatchResult m;
                if (re.match(selText, 0, &m) && m.start == 0 && m.end == selText.size()) {
                    std::string rep = re.expandReplacement(selText, m, opt.replacement);
                    ed->adapter().send(SCI_SETTARGETSTART, selStart);
                    ed->adapter().send(SCI_SETTARGETEND, selEnd);
                    ed->adapter().send(SCI_REPLACETARGET, rep.size(), reinterpret_cast<sptr_t>(rep.c_str()));
                    ed->adapter().send(SCI_SETSEL, selStart, selStart + rep.size());
                }
            }
        }
    }

    findNextOrPrev(opt, false);
}

void MainWindow::replaceAllMatches(const SearchOptions& opt) {
    auto* ed = activeEditor();
    if (!ed || opt.query.empty()) return;

    const std::string text = ed->adapter().getText();
    auto [newText, count] = searchEngine_.replaceAll(text, opt);
    if (count > 0) {
        ed->adapter().setText(newText);
        searchBar_.setStatusText("Replaced " + std::to_string(count) + " occurrences");
        clearSearchHighlights();
    } else {
        searchBar_.setStatusText("No matches to replace");
    }
}

void MainWindow::setupMenuBar() {
    MenuBar::Callbacks cbs;
    cbs.onNewTab = [this]() { newTab(); };
    cbs.onOpenFile = [this]() { openFileDialog(); };
    cbs.onSaveFile = [this]() { saveActiveDocument(); };
    cbs.onSaveFileAs = [this]() { saveActiveDocumentAs(); };
    cbs.onOpenRecentFile = [this](const std::string& path) { openFile(path); };
    cbs.onClearRecentFiles = [this]() {
        settings_.clearRecentFiles();
        settings_.saveToFile();
        menuBar_->updateRecentFiles({});
    };
    cbs.onCloseTab = [this]() { closeActiveTab(); };
    cbs.onReopenTab = [this]() { reopenClosedTab(); };
    cbs.onQuit = [this]() { gtk_window_close(GTK_WINDOW(window_)); };

    cbs.onUndo = [this]() { undo(); };
    cbs.onRedo = [this]() { redo(); };
    cbs.onCut = [this]() { cut(); };
    cbs.onCopy = [this]() { copy(); };
    cbs.onPaste = [this]() { paste(); };
    cbs.onSelectAll = [this]() { selectAll(); };
    cbs.onDuplicateLine = [this]() { duplicateLine(); };
    cbs.onDeleteLine = [this]() { deleteLine(); };
    cbs.onMoveLineUp = [this]() { moveLineUp(); };
    cbs.onMoveLineDown = [this]() { moveLineDown(); };

    cbs.onFind = [this]() { showFindBar(false); };
    cbs.onReplace = [this]() { showFindBar(true); };
    cbs.onFindNext = [this]() { findNextOrPrev(searchBar_.currentOptions(), false); };
    cbs.onFindPrev = [this]() { findNextOrPrev(searchBar_.currentOptions(), true); };
    cbs.onGoToLine = [this]() { goToLineDialog(); };
    cbs.onToggleBookmark = [this]() { toggleBookmark(); };
    cbs.onNextBookmark = [this]() { nextBookmark(); };
    cbs.onPrevBookmark = [this]() { prevBookmark(); };
    cbs.onClearAllBookmarks = [this]() { clearAllBookmarks(); };

    cbs.onToggleWordWrap = [this]() { toggleWordWrap(); };
    cbs.onToggleLineNumbers = [this]() { toggleLineNumbers(); };
    cbs.onZoomIn = [this]() { zoomIn(); };
    cbs.onZoomOut = [this]() { zoomOut(); };
    cbs.onResetZoom = [this]() { resetZoom(); };
    cbs.onToggleTheme = [this]() { toggleTheme(); };

    cbs.onSelectLanguage = [this](const std::string& lang) { setLanguage(lang); };
    cbs.onAbout = [this]() { showAboutDialog(); };

    menuBar_ = std::make_unique<MenuBar>(lexerManager_.availableLanguages(), std::move(cbs));
    menuBar_->updateRecentFiles(settings_.recentFiles());
}

void MainWindow::undo() {
    if (auto* ed = activeEditor()) ed->undo();
}

void MainWindow::redo() {
    if (auto* ed = activeEditor()) ed->redo();
}

void MainWindow::cut() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_CUT);
}

void MainWindow::copy() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_COPY);
}

void MainWindow::paste() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_PASTE);
}

void MainWindow::selectAll() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_SELECTALL);
}

void MainWindow::duplicateLine() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_LINEDUPLICATE);
}

void MainWindow::deleteLine() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_LINEDELETE);
}

void MainWindow::moveLineUp() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_MOVESELECTEDLINESUP);
}

void MainWindow::moveLineDown() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_MOVESELECTEDLINESDOWN);
}

void MainWindow::toggleWordWrap() {
    settings_.setWordWrap(!settings_.wordWrap());
    for (auto& [id, ed] : editors_) {
        ed->adapter().setWordWrap(settings_.wordWrap());
    }
}

void MainWindow::toggleLineNumbers() {
    settings_.setShowLineNumbers(!settings_.showLineNumbers());
    for (auto& [id, ed] : editors_) {
        ed->adapter().setLineNumbers(settings_.showLineNumbers());
    }
}

void MainWindow::zoomIn() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_ZOOMIN);
}

void MainWindow::zoomOut() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_ZOOMOUT);
}

void MainWindow::resetZoom() {
    if (auto* ed = activeEditor()) ed->adapter().send(SCI_SETZOOM, 0);
}

void MainWindow::toggleTheme() {
    settings_.setTheme(settings_.isDarkTheme() ? "light" : "dark");
    for (auto& [id, ed] : editors_) {
        Document* doc = docManager_.findById(id);
        const std::string lang = doc ? lexerManager_.detectLanguage(doc->filePath()) : "Plain Text";
        lexerManager_.applyLanguage(ed->adapter(), lang, settings_.isDarkTheme());
    }
}

void MainWindow::setLanguage(const std::string& lang) {
    if (Document* doc = activeDocument()) {
        doc->setLanguage(lang);
    }
    if (auto* ed = activeEditor()) {
        lexerManager_.applyLanguage(ed->adapter(), lang, settings_.isDarkTheme());
        statusBar_.updateLanguage(lang);
    }
}

void MainWindow::goToLineDialog() {
    auto* ed = activeEditor();
    if (!ed) return;

    const sptr_t lineCount = ed->adapter().send(SCI_GETLINECOUNT);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Go to Line",
        GTK_WINDOW(window_),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Go", GTK_RESPONSE_OK,
        nullptr
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Line number:"), FALSE, FALSE, 0);

    const sptr_t curLine = ed->adapter().send(SCI_LINEFROMPOSITION, ed->adapter().getCurrentPos()) + 1;
    GtkWidget* spin = gtk_spin_button_new_with_range(1, std::max<double>(1, lineCount), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), curLine);
    gtk_box_pack_start(GTK_BOX(hbox), spin, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content), hbox);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        const int target = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
        ed->adapter().send(SCI_GOTOLINE, target - 1);
        ed->adapter().send(SCI_SCROLLCARET);
    }
    gtk_widget_destroy(dialog);
}

void MainWindow::showAboutDialog() {
    GtkWidget* about = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), "NotepadX");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), "0.1.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), "Fast, distraction-free Notepad++ alternative for Linux");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(about), GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), "© 2026 NotepadX Authors");
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(about);
}

void MainWindow::updateStatusBar() {
    auto* ed = activeEditor();
    if (!ed) return;

    const sptr_t pos = ed->adapter().getCurrentPos();
    const sptr_t line = ed->adapter().send(SCI_LINEFROMPOSITION, pos);
    const sptr_t lineStart = ed->adapter().send(SCI_POSITIONFROMLINE, line);
    const sptr_t col = pos - lineStart + 1;
    const sptr_t selStart = ed->adapter().send(SCI_GETSELECTIONSTART);
    const sptr_t selEnd = ed->adapter().send(SCI_GETSELECTIONEND);
    const size_t len = ed->adapter().getLength();
    const sptr_t lineCount = ed->adapter().send(SCI_GETLINECOUNT);

    statusBar_.updateCursor(static_cast<int>(line + 1), static_cast<int>(col));
    statusBar_.updateSelection(static_cast<int>(std::abs(selEnd - selStart)));
    statusBar_.updateDocStats(static_cast<int>(lineCount), len);
    statusBar_.updateEol(ed->config().eolMode);

    Document* doc = activeDocument();
    if (doc) {
        statusBar_.updateLanguage(doc->language());
        statusBar_.updateEncoding(doc->encoding());
    } else {
        statusBar_.updateEncoding("UTF-8");
    }
}

void MainWindow::toggleBookmark(int targetLine) {
    auto* ed = activeEditor();
    if (!ed) return;

    const sptr_t line = (targetLine >= 0) ? targetLine
        : ed->adapter().send(SCI_LINEFROMPOSITION, ed->adapter().getCurrentPos());
    const sptr_t state = ed->adapter().send(SCI_MARKERGET, line);
    constexpr int kMarkerMask = (1 << 1);

    if (state & kMarkerMask) {
        ed->adapter().send(SCI_MARKERDELETE, line, 1);
    } else {
        ed->adapter().send(SCI_MARKERADD, line, 1);
    }
}

void MainWindow::nextBookmark() {
    auto* ed = activeEditor();
    if (!ed) return;

    constexpr int kMarkerMask = (1 << 1);
    const sptr_t curLine = ed->adapter().send(SCI_LINEFROMPOSITION, ed->adapter().getCurrentPos());
    sptr_t nextLine = ed->adapter().send(SCI_MARKERNEXT, curLine + 1, kMarkerMask);
    if (nextLine == -1) {
        nextLine = ed->adapter().send(SCI_MARKERNEXT, 0, kMarkerMask);
    }

    if (nextLine != -1) {
        ed->adapter().send(SCI_GOTOLINE, nextLine);
        ed->adapter().send(SCI_SCROLLCARET);
    }
}

void MainWindow::prevBookmark() {
    auto* ed = activeEditor();
    if (!ed) return;

    constexpr int kMarkerMask = (1 << 1);
    const sptr_t curLine = ed->adapter().send(SCI_LINEFROMPOSITION, ed->adapter().getCurrentPos());
    sptr_t prevLine = (curLine > 0) ? ed->adapter().send(SCI_MARKERPREVIOUS, curLine - 1, kMarkerMask) : -1;
    if (prevLine == -1) {
        const sptr_t totalLines = ed->adapter().send(SCI_GETLINECOUNT);
        prevLine = ed->adapter().send(SCI_MARKERPREVIOUS, totalLines, kMarkerMask);
    }

    if (prevLine != -1) {
        ed->adapter().send(SCI_GOTOLINE, prevLine);
        ed->adapter().send(SCI_SCROLLCARET);
    }
}

void MainWindow::clearAllBookmarks() {
    if (auto* ed = activeEditor()) {
        ed->adapter().send(SCI_MARKERDELETEALL, 1);
    }
}

gboolean MainWindow::onDeleteEvent([[maybe_unused]] GtkWidget* widget,
                                  [[maybe_unused]] GdkEvent* event,
                                  gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    if (self) {
        self->saveCurrentSession();
        self->settings_.saveToFile();
    }
    return FALSE; // Allow default window destroy
}

void MainWindow::show() {
    gtk_widget_show_all(window_);
}

void MainWindow::present() {
    if (window_) {
        gtk_window_present(GTK_WINDOW(window_));
    }
}

} // namespace notepadx
