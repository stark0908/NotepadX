#include "ui/StatusBar.h"

namespace notepadx {

StatusBar::StatusBar()
    : container_(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12)),
      lblPos_(gtk_label_new("Ln 1, Col 1")),
      lblSel_(gtk_label_new("Sel 0")),
      lblStats_(gtk_label_new("Lines 1, Len 0")),
      lblEol_(gtk_label_new("LF")),
      lblEncoding_(gtk_label_new("UTF-8")),
      lblLang_(gtk_label_new("Plain Text")) {

    gtk_container_set_border_width(GTK_CONTAINER(container_), 2);

    // Left-aligned editor status
    gtk_box_pack_start(GTK_BOX(container_), lblPos_, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(container_), lblSel_, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(container_), lblStats_, FALSE, FALSE, 4);

    // Expanding spacer to push right side
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(container_), spacer, TRUE, TRUE, 0);

    // Right-aligned metadata
    GtkWidget* btnEol = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btnEol), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(btnEol), lblEol_);
    gtk_widget_set_tooltip_text(btnEol, "Click to toggle EOL mode (LF / CRLF)");
    g_signal_connect(btnEol, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<StatusBar*>(data);
        if (self->eolCb_) {
            self->eolCb_();
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(container_), btnEol, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(container_), lblEncoding_, FALSE, FALSE, 4);

    GtkWidget* btnLang = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btnLang), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(btnLang), lblLang_);
    gtk_widget_set_tooltip_text(btnLang, "Click to choose language");
    g_signal_connect(btnLang, "clicked", G_CALLBACK(+[](GtkButton* /*b*/, gpointer data) {
        auto* self = static_cast<StatusBar*>(data);
        if (self->langCb_) {
            self->langCb_();
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(container_), btnLang, FALSE, FALSE, 4);

    gtk_widget_show_all(container_);
}

void StatusBar::updateCursor(int line, int col) {
    std::string text = "Ln " + std::to_string(line) + ", Col " + std::to_string(col);
    gtk_label_set_text(GTK_LABEL(lblPos_), text.c_str());
}

void StatusBar::updateSelection(int selLength) {
    std::string text = "Sel " + std::to_string(selLength);
    gtk_label_set_text(GTK_LABEL(lblSel_), text.c_str());
}

void StatusBar::updateDocStats(int lineCount, size_t length) {
    std::string text = "Lines " + std::to_string(lineCount) + ", Len " + std::to_string(length);
    gtk_label_set_text(GTK_LABEL(lblStats_), text.c_str());
}

void StatusBar::updateEol(int eolMode) {
    const char* str = (eolMode == 0) ? "CRLF" : (eolMode == 1 ? "CR" : "LF");
    gtk_label_set_text(GTK_LABEL(lblEol_), str);
}

void StatusBar::updateLanguage(const std::string& lang) {
    gtk_label_set_text(GTK_LABEL(lblLang_), lang.c_str());
}

} // namespace notepadx
