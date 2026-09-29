#include "ui/TabBar.h"

#include <algorithm>

namespace notepadx {

TabBar::TabBar()
    : notebook_(gtk_notebook_new()) {
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), FALSE);

    g_signal_connect(notebook_, "switch-page", G_CALLBACK(onSwitchPage), this);
    g_signal_connect(notebook_, "page-reordered", G_CALLBACK(+[](GtkNotebook* /*nb*/, GtkWidget* /*child*/, guint /*pageNum*/, gpointer userData) {
        auto* self = static_cast<TabBar*>(userData);
        // Sync tabs_ order with notebook page order
        const int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(self->notebook_));
        std::vector<TabData> reordered;
        reordered.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(self->notebook_), i);
            auto it = std::find_if(self->tabs_.begin(), self->tabs_.end(),
                                   [self, page](const TabData& td) {
                                       return td.tabBox == gtk_notebook_get_tab_label(GTK_NOTEBOOK(self->notebook_), page);
                                   });
            if (it != self->tabs_.end()) {
                reordered.push_back(*it);
            }
        }
        if (reordered.size() == self->tabs_.size()) {
            self->tabs_ = std::move(reordered);
        }
    }), this);
}

GtkWidget* TabBar::createTabHeader(Document* doc, TabData& data) {
    GtkWidget* eventBox = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(eventBox), FALSE);

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add(GTK_CONTAINER(eventBox), hbox);

    GtkWidget* label = gtk_label_new(doc->displayName().c_str());
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    GtkWidget* closeImg = gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    GtkWidget* closeBtn = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(closeBtn), closeImg);
    gtk_button_set_relief(GTK_BUTTON(closeBtn), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(closeBtn, FALSE);
    gtk_widget_set_tooltip_text(closeBtn, "Close Tab (Ctrl+W)");
    gtk_box_pack_start(GTK_BOX(hbox), closeBtn, FALSE, FALSE, 0);

    data.doc = doc;
    data.labelWidget = label;
    data.closeBtn = closeBtn;
    data.tabBox = eventBox;

    if (!doc->filePath().empty()) {
        gtk_widget_set_tooltip_text(eventBox, doc->filePath().c_str());
    }

    g_object_set_data(G_OBJECT(closeBtn), "tab-bar", this);
    g_object_set_data(G_OBJECT(closeBtn), "tab-doc", doc);

    g_signal_connect(closeBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer /*userData*/) {
        auto* self = static_cast<TabBar*>(g_object_get_data(G_OBJECT(btn), "tab-bar"));
        auto* targetDoc = static_cast<Document*>(g_object_get_data(G_OBJECT(btn), "tab-doc"));
        if (self && targetDoc && self->tabCloseCb_) {
            const int idx = self->indexOfDocument(targetDoc);
            if (idx >= 0) {
                self->tabCloseCb_(idx, targetDoc);
            }
        }
    }), nullptr);

    g_signal_connect(eventBox, "button-press-event", G_CALLBACK(onTabButtonPress), this);

    gtk_widget_show_all(eventBox);
    return eventBox;
}

int TabBar::pageIndexOf(GtkWidget* pageWidget) const {
    return gtk_notebook_page_num(GTK_NOTEBOOK(notebook_), pageWidget);
}

void TabBar::addTab(Document* doc, GtkWidget* pageWidget) {
    TabData data;
    GtkWidget* header = createTabHeader(doc, data);
    tabs_.push_back(data);

    suppressSwitchSignal_ = true;
    const int newIdx = gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), pageWidget, header);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook_), pageWidget, TRUE);
    gtk_widget_show_all(pageWidget);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), newIdx);
    suppressSwitchSignal_ = false;

    if (tabChangedCb_) {
        tabChangedCb_(newIdx, doc);
    }
}

void TabBar::removeTab(int pageIndex) {
    if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= tabs_.size()) {
        return;
    }

    tabs_.erase(tabs_.begin() + pageIndex);
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), pageIndex);
}

void TabBar::updateTabTitle(int pageIndex) {
    if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= tabs_.size()) {
        return;
    }
    Document* doc = tabs_[static_cast<size_t>(pageIndex)].doc;
    if (doc && tabs_[static_cast<size_t>(pageIndex)].labelWidget) {
        gtk_label_set_text(GTK_LABEL(tabs_[static_cast<size_t>(pageIndex)].labelWidget),
                           doc->displayName().c_str());
        if (!doc->filePath().empty()) {
            gtk_widget_set_tooltip_text(tabs_[static_cast<size_t>(pageIndex)].tabBox,
                                       doc->filePath().c_str());
        }
    }
}

int TabBar::activeIndex() const {
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_));
}

void TabBar::setActiveIndex(int pageIndex) {
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), pageIndex);
}

int TabBar::count() const {
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_));
}

Document* TabBar::documentAt(int pageIndex) const {
    if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= tabs_.size()) {
        return nullptr;
    }
    return tabs_[static_cast<size_t>(pageIndex)].doc;
}

GtkWidget* TabBar::pageAt(int pageIndex) const {
    return gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook_), pageIndex);
}

int TabBar::indexOfDocument(const Document* doc) const {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].doc == doc) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TabBar::setTabChangedCallback(TabChangedCallback cb) {
    tabChangedCb_ = std::move(cb);
}

void TabBar::setTabCloseCallback(TabCloseCallback cb) {
    tabCloseCb_ = std::move(cb);
}

void TabBar::setTabCloseOthersCallback(TabCloseOthersCallback cb) {
    tabCloseOthersCb_ = std::move(cb);
}

void TabBar::setTabCloseAllCallback(TabCloseAllCallback cb) {
    tabCloseAllCb_ = std::move(cb);
}

void TabBar::setNewTabCallback(NewTabCallback cb) {
    newTabCb_ = std::move(cb);
}

void TabBar::onSwitchPage([[maybe_unused]] GtkNotebook* notebook,
                         [[maybe_unused]] GtkWidget* page,
                         guint pageNum,
                         gpointer userData) {
    auto* self = static_cast<TabBar*>(userData);
    if (!self || self->suppressSwitchSignal_) {
        return;
    }
    if (self->tabChangedCb_) {
        Document* doc = self->documentAt(static_cast<int>(pageNum));
        self->tabChangedCb_(static_cast<int>(pageNum), doc);
    }
}

gboolean TabBar::onTabButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer userData) {
    auto* self = static_cast<TabBar*>(userData);
    if (!self || !event) {
        return FALSE;
    }

    int clickedIndex = -1;
    for (size_t i = 0; i < self->tabs_.size(); ++i) {
        if (self->tabs_[i].tabBox == widget) {
            clickedIndex = static_cast<int>(i);
            break;
        }
    }
    if (clickedIndex < 0) {
        return FALSE;
    }

    if (event->button == 2) { // Middle click -> Close Tab
        if (self->tabCloseCb_) {
            self->tabCloseCb_(clickedIndex, self->tabs_[static_cast<size_t>(clickedIndex)].doc);
        }
        return TRUE;
    }

    if (event->button == 3) { // Right click -> Context Menu
        self->showContextMenu(event, clickedIndex);
        return TRUE;
    }

    return FALSE;
}

void TabBar::showContextMenu(GdkEventButton* event, int pageIndex) {
    GtkWidget* menu = gtk_menu_new();

    GtkWidget* itemClose = gtk_menu_item_new_with_label("Close Tab");
    g_signal_connect(itemClose, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer userData) {
        auto* self = static_cast<TabBar*>(userData);
        const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab-index"));
        if (self->tabCloseCb_ && idx >= 0 && idx < self->count()) {
            self->tabCloseCb_(idx, self->documentAt(idx));
        }
    }), this);
    // Pass index as object data
    g_object_set_data(G_OBJECT(itemClose), "tab-index", GINT_TO_POINTER(pageIndex));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemClose);

    GtkWidget* itemCloseOthers = gtk_menu_item_new_with_label("Close Other Tabs");
    g_signal_connect(itemCloseOthers, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer userData) {
        auto* self = static_cast<TabBar*>(userData);
        const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab-index"));
        if (self->tabCloseOthersCb_) {
            self->tabCloseOthersCb_(idx);
        }
    }), this);
    g_object_set_data(G_OBJECT(itemCloseOthers), "tab-index", GINT_TO_POINTER(pageIndex));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemCloseOthers);

    GtkWidget* itemCloseAll = gtk_menu_item_new_with_label("Close All Tabs");
    g_signal_connect_swapped(itemCloseAll, "activate", G_CALLBACK(+[](TabBar* self) {
        if (self->tabCloseAllCb_) {
            self->tabCloseAllCb_();
        }
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemCloseAll);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), reinterpret_cast<const GdkEvent*>(event));
}

} // namespace notepadx
