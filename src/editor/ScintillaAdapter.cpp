#include "editor/ScintillaAdapter.h"

#include <algorithm>
#include <cmath>

namespace notepadx {

ScintillaAdapter::ScintillaAdapter()
    : widget_(scintilla_new()) {
    g_signal_connect(widget_, SCINTILLA_NOTIFY, G_CALLBACK(onNotification), this);
    g_signal_connect(widget_, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer userData) {
        auto* self = static_cast<ScintillaAdapter*>(userData);
        self->widget_ = nullptr;
    }), this);

    send(SCI_SETCODEPAGE, SC_CP_UTF8);
    send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    send(SCI_SETMARGINWIDTHN, 0, 0);
    send(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
    send(SCI_SETMARGINMASKN, 1, 1 << 1);
    send(SCI_SETMARGINWIDTHN, 1, 16);
    send(SCI_SETMARGINSENSITIVEN, 1, 1);
    send(SCI_MARKERDEFINE, 1, SC_MARK_CIRCLE);
    send(SCI_MARKERSETBACK, 1, 0x00A0FF);
    send(SCI_MARKERSETFORE, 1, 0x000000);
    send(SCI_SETMARGINWIDTHN, 2, 0);
    send(SCI_SETTABWIDTH, 4);
    send(SCI_SETUSETABS, 0);
    send(SCI_SETTABINDENTS, 1);
    send(SCI_SETBACKSPACEUNINDENTS, 1);
    send(SCI_SETCARETPERIOD, 500);
    send(SCI_SETCARETWIDTH, 2);
    send(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_USER | SC_PERFORMED_UNDO | SC_PERFORMED_REDO);
}

ScintillaAdapter::~ScintillaAdapter() {
    if (widget_) {
        g_signal_handlers_disconnect_by_data(widget_, this);
        if (g_object_is_floating(widget_)) {
            g_object_ref_sink(widget_);
            g_object_unref(widget_);
        }
        widget_ = nullptr;
    }
}

sptr_t ScintillaAdapter::send(unsigned int msg, uptr_t wParam, sptr_t lParam) const {
    return scintilla_send_message(SCINTILLA(widget_), msg, wParam, lParam);
}

void ScintillaAdapter::setText(std::string_view text) {
    send(SCI_CLEARALL);
    if (!text.empty()) {
        send(SCI_ADDTEXT, static_cast<uptr_t>(text.size()), reinterpret_cast<sptr_t>(text.data()));
    }
    send(SCI_EMPTYUNDOBUFFER);
    send(SCI_SETSAVEPOINT);
    send(SCI_GOTOPOS, 0);
    updateLineNumberWidth();
}

std::string ScintillaAdapter::getText() const {
    const auto len = static_cast<size_t>(send(SCI_GETLENGTH));
    if (len == 0) {
        return {};
    }
    std::string buffer(len + 1, '\0');
    send(SCI_GETTEXT, len + 1, reinterpret_cast<sptr_t>(buffer.data()));
    buffer.resize(len);
    return buffer;
}

size_t ScintillaAdapter::getLength() const {
    return static_cast<size_t>(send(SCI_GETLENGTH));
}

void ScintillaAdapter::setLineNumbers(bool show) {
    if (!show) {
        send(SCI_SETMARGINWIDTHN, 0, 0);
        return;
    }
    updateLineNumberWidth();
}

void ScintillaAdapter::updateLineNumberWidth() {
    const sptr_t lines = send(SCI_GETLINECOUNT);
    int digits = 1;
    sptr_t count = lines;
    while (count >= 10) {
        count /= 10;
        ++digits;
    }
    digits = std::max(digits, 1);
    const std::string sample(static_cast<size_t>(digits), '9');

    const sptr_t pixelWidth = send(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(sample.c_str()));
    send(SCI_SETMARGINWIDTHN, 0, pixelWidth + 4);
}

void ScintillaAdapter::setWordWrap(bool enable) {
    send(SCI_SETWRAPMODE, enable ? SC_WRAP_WORD : SC_WRAP_NONE);
}

void ScintillaAdapter::setTabWidth(int spaces) {
    send(SCI_SETTABWIDTH, spaces);
}

void ScintillaAdapter::setUseTabs(bool useTabs) {
    send(SCI_SETUSETABS, useTabs ? 1 : 0);
}

void ScintillaAdapter::setFont(const std::string& fontName, int sizePt) {
    send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(fontName.c_str()));
    send(SCI_STYLESETSIZE, STYLE_DEFAULT, sizePt);
    send(SCI_STYLESETFONT, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(fontName.c_str()));
    send(SCI_STYLESETSIZE, STYLE_LINENUMBER, sizePt);
    updateLineNumberWidth();
}

void ScintillaAdapter::setEolMode(int eolMode) {
    send(SCI_SETEOLMODE, eolMode);
}

void ScintillaAdapter::undo() {
    send(SCI_UNDO);
}

void ScintillaAdapter::redo() {
    send(SCI_REDO);
}

bool ScintillaAdapter::canUndo() const {
    return send(SCI_CANUNDO) != 0;
}

bool ScintillaAdapter::canRedo() const {
    return send(SCI_CANREDO) != 0;
}

sptr_t ScintillaAdapter::getCurrentPos() const {
    return send(SCI_GETCURRENTPOS);
}

void ScintillaAdapter::setCurrentPos(sptr_t pos) {
    send(SCI_SETCURRENTPOS, pos);
    send(SCI_SETANCHOR, pos);
    send(SCI_SCROLLCARET);
}

sptr_t ScintillaAdapter::getFirstVisibleLine() const {
    return send(SCI_GETFIRSTVISIBLELINE);
}

void ScintillaAdapter::setFirstVisibleLine(sptr_t line) {
    const sptr_t current = send(SCI_GETFIRSTVISIBLELINE);
    send(SCI_LINESCROLL, 0, line - current);
}

void ScintillaAdapter::setNotificationCallback(NotificationCallback cb) {
    notificationCb_ = std::move(cb);
}

void ScintillaAdapter::onNotification([[maybe_unused]] GtkWidget* widget,
                                     [[maybe_unused]] gint id,
                                     SCNotification* scn,
                                     gpointer userData) {
    auto* self = static_cast<ScintillaAdapter*>(userData);
    if (!self || !scn) {
        return;
    }

    if (scn->nmhdr.code == SCN_MODIFIED) {
        if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
            self->updateLineNumberWidth();
        }
    }

    if (self->notificationCb_) {
        self->notificationCb_(scn);
    }
}

} // namespace notepadx
