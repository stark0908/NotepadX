#include "editor/Editor.h"

#include <cstring>

namespace notepadx {

Editor::Editor(const EditorConfig& config)
    : config_(config) {
    adapter_.setNotificationCallback([this](const SCNotification* scn) {
        handleNotification(scn);
    });
    adapter_.setFont(config_.fontName, config_.fontSizePt);
    adapter_.setLineNumbers(config_.showLineNumbers);
    adapter_.setWordWrap(config_.wordWrap);
    adapter_.setTabWidth(config_.tabWidth);
    adapter_.setUseTabs(config_.useTabs);
    adapter_.setEolMode(config_.eolMode);
}

Editor::~Editor() {
    cancelUndoTimer();
}

void Editor::applyConfig(const EditorConfig& config) {
    if (config_.fontName != config.fontName || config_.fontSizePt != config.fontSizePt) {
        adapter_.setFont(config.fontName, config.fontSizePt);
    }
    if (config_.showLineNumbers != config.showLineNumbers) {
        adapter_.setLineNumbers(config.showLineNumbers);
    }
    if (config_.wordWrap != config.wordWrap) {
        adapter_.setWordWrap(config.wordWrap);
    }
    if (config_.tabWidth != config.tabWidth) {
        adapter_.setTabWidth(config.tabWidth);
    }
    if (config_.useTabs != config.useTabs) {
        adapter_.setUseTabs(config.useTabs);
    }
    if (config_.eolMode != config.eolMode) {
        adapter_.setEolMode(config.eolMode);
    }
    config_ = config;
}

bool Editor::isModified() const {
    return isModified_;
}

void Editor::setSavePoint() {
    adapter_.send(SCI_SETSAVEPOINT);
    isModified_ = false;
    if (modifiedChangedCb_) {
        modifiedChangedCb_(false);
    }
}

void Editor::undo() {
    cancelUndoTimer();
    adapter_.undo();
}

void Editor::redo() {
    cancelUndoTimer();
    adapter_.redo();
}

void Editor::sealUndoAction() {
    cancelUndoTimer();
    adapter_.send(SCI_BEGINUNDOACTION);
    adapter_.send(SCI_ENDUNDOACTION);
}

void Editor::cancelUndoTimer() {
    if (undoTimeoutId_ != 0) {
        g_source_remove(undoTimeoutId_);
        undoTimeoutId_ = 0;
    }
}

void Editor::scheduleUndoTimer() {
    cancelUndoTimer();
    undoTimeoutId_ = g_timeout_add(750, +[](gpointer ptr) -> gboolean {
        auto* self = static_cast<Editor*>(ptr);
        self->undoTimeoutId_ = 0;
        self->adapter_.send(SCI_BEGINUNDOACTION);
        self->adapter_.send(SCI_ENDUNDOACTION);
        return G_SOURCE_REMOVE;
    }, this);
}

void Editor::setContentChangedCallback(ContentChangedCallback cb) {
    contentChangedCb_ = std::move(cb);
}

void Editor::setModifiedChangedCallback(ModifiedChangedCallback cb) {
    modifiedChangedCb_ = std::move(cb);
}

void Editor::setUpdateUiCallback(UpdateUiCallback cb) {
    updateUiCb_ = std::move(cb);
}

void Editor::setMarginClickCallback(MarginClickCallback cb) {
    marginClickCb_ = std::move(cb);
}

void Editor::handleNotification(const SCNotification* scn) {
    if (!scn) {
        return;
    }

    switch (scn->nmhdr.code) {
        case SCN_SAVEPOINTREACHED:
            isModified_ = false;
            if (modifiedChangedCb_) {
                modifiedChangedCb_(false);
            }
            break;
        case SCN_SAVEPOINTLEFT:
            isModified_ = true;
            if (modifiedChangedCb_) {
                modifiedChangedCb_(true);
            }
            break;
        case SCN_MODIFIED:
            if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                if (scn->modificationType & SC_PERFORMED_USER) {
                    if ((scn->modificationType & SC_MOD_INSERTTEXT) && scn->text && std::strchr(scn->text, '\n')) {
                        sealUndoAction();
                    } else {
                        scheduleUndoTimer();
                    }
                }
                if (contentChangedCb_) {
                    contentChangedCb_();
                }
            }
            break;
        case SCN_UPDATEUI:
            if (updateUiCb_) {
                updateUiCb_();
            }
            break;
        case SCN_MARGINCLICK:
            if (marginClickCb_) {
                marginClickCb_(static_cast<int>(scn->line), scn->margin);
            }
            break;
        default:
            break;
    }
}

} // namespace notepadx
