#include "ui/SearchBar.h"

namespace notepadx {

SearchBar::SearchBar()
    : container_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4)),
      findRow_(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6)),
      replaceRow_(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6)),
      findEntry_(gtk_search_entry_new()),
      replaceEntry_(gtk_entry_new()),
      btnPrev_(gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON)),
      btnNext_(gtk_button_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_BUTTON)),
      btnClose_(gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON)),
      chkCase_(gtk_toggle_button_new_with_label("Aa")),
      chkWord_(gtk_toggle_button_new_with_label(R"(\b)")),
      chkRegex_(gtk_toggle_button_new_with_label(".*")),
      btnReplace_(gtk_button_new_with_label("Replace")),
      btnReplaceAll_(gtk_button_new_with_label("Replace All")),
      statusLabel_(gtk_label_new("")) {

    gtk_container_set_border_width(GTK_CONTAINER(container_), 4);

    gtk_widget_set_tooltip_text(btnClose_, "Close Search Bar (Esc)");
    gtk_button_set_relief(GTK_BUTTON(btnClose_), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(findRow_), btnClose_, FALSE, FALSE, 0);

    gtk_widget_set_size_request(findEntry_, 250, -1);
    gtk_box_pack_start(GTK_BOX(findRow_), findEntry_, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(btnPrev_, "Previous Match (Shift+F3)");
    gtk_box_pack_start(GTK_BOX(findRow_), btnPrev_, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(btnNext_, "Next Match (F3 / Enter)");
    gtk_box_pack_start(GTK_BOX(findRow_), btnNext_, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(chkCase_, "Match Case");
    gtk_box_pack_start(GTK_BOX(findRow_), chkCase_, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(chkWord_, "Match Whole Word");
    gtk_box_pack_start(GTK_BOX(findRow_), chkWord_, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(chkRegex_, "Use Regular Expression");
    gtk_box_pack_start(GTK_BOX(findRow_), chkRegex_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(findRow_), statusLabel_, FALSE, FALSE, 8);

    gtk_container_add(GTK_CONTAINER(container_), findRow_);

    // Replace row setup
    GtkWidget* replaceIcon = gtk_image_new_from_icon_name("edit-find-replace-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(replaceRow_), replaceIcon, FALSE, FALSE, 4);

    gtk_widget_set_size_request(replaceEntry_, 250, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(replaceEntry_), "Replace with...");
    gtk_box_pack_start(GTK_BOX(replaceRow_), replaceEntry_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(replaceRow_), btnReplace_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(replaceRow_), btnReplaceAll_, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(container_), replaceRow_);

    // Connect signals
    g_signal_connect(findEntry_, "search-changed", G_CALLBACK(onEntryChanged), this);
    g_signal_connect(findEntry_, "activate", G_CALLBACK(onEntryActivate), this);

    g_signal_connect(btnNext_, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<SearchBar*>(data);
        if (self->findCb_) {
            self->findCb_(self->currentOptions(), false);
        }
    }), this);

    g_signal_connect(btnPrev_, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<SearchBar*>(data);
        if (self->findCb_) {
            self->findCb_(self->currentOptions(), true);
        }
    }), this);

    g_signal_connect(btnClose_, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<SearchBar*>(data);
        self->hide();
    }), this);

    g_signal_connect(chkCase_, "toggled", G_CALLBACK(onToggleChanged), this);
    g_signal_connect(chkWord_, "toggled", G_CALLBACK(onToggleChanged), this);
    g_signal_connect(chkRegex_, "toggled", G_CALLBACK(onToggleChanged), this);

    g_signal_connect(btnReplace_, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<SearchBar*>(data);
        if (self->replaceCb_) {
            self->replaceCb_(self->currentOptions());
        }
    }), this);

    g_signal_connect(btnReplaceAll_, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<SearchBar*>(data);
        if (self->replaceAllCb_) {
            self->replaceAllCb_(self->currentOptions());
        }
    }), this);

    gtk_widget_set_no_show_all(container_, TRUE);
    gtk_widget_hide(container_);
}

void SearchBar::show(bool replaceMode) {
    replaceMode_ = replaceMode;
    gtk_widget_show(container_);
    gtk_widget_show(findRow_);

    if (replaceMode_) {
        gtk_widget_show(replaceRow_);
    } else {
        gtk_widget_hide(replaceRow_);
    }

    focusFindEntry();
    notifyQueryChanged();
}

void SearchBar::hide() {
    gtk_widget_hide(container_);
    if (closeCb_) {
        closeCb_();
    }
}

bool SearchBar::isVisible() const {
    return gtk_widget_get_visible(container_) != FALSE;
}

void SearchBar::setStatusText(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(statusLabel_), text.c_str());
}

SearchOptions SearchBar::currentOptions() const {
    SearchOptions opt;
    opt.query = gtk_entry_get_text(GTK_ENTRY(findEntry_));
    opt.replacement = gtk_entry_get_text(GTK_ENTRY(replaceEntry_));
    opt.caseSensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkCase_)) != FALSE;
    opt.wholeWord = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkWord_)) != FALSE;
    opt.isRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkRegex_)) != FALSE;
    opt.wrapAround = true;
    return opt;
}

void SearchBar::focusFindEntry() {
    gtk_widget_grab_focus(findEntry_);
}

void SearchBar::notifyQueryChanged() {
    if (queryChangedCb_) {
        queryChangedCb_(currentOptions());
    }
}

void SearchBar::onEntryChanged([[maybe_unused]] GtkEditable* editable, gpointer userData) {
    auto* self = static_cast<SearchBar*>(userData);
    if (self) {
        self->notifyQueryChanged();
    }
}

void SearchBar::onEntryActivate([[maybe_unused]] GtkEntry* entry, gpointer userData) {
    auto* self = static_cast<SearchBar*>(userData);
    if (self && self->findCb_) {
        self->findCb_(self->currentOptions(), false);
    }
}

void SearchBar::onToggleChanged([[maybe_unused]] GtkToggleButton* btn, gpointer userData) {
    auto* self = static_cast<SearchBar*>(userData);
    if (self) {
        self->notifyQueryChanged();
    }
}

} // namespace notepadx
