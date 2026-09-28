#include "editor/Editor.h"

namespace notepadx {

Editor::Editor(const EditorConfig& config)
    : config_(config) {
    adapter_.setNotificationCallback([this](const SCNotification* scn) {
        handleNotification(scn);
    });
    applyConfig(config_);
}

void Editor::applyConfig(const EditorConfig& config) {
    config_ = config;
    adapter_.setFont(config_.fontName, config_.fontSizePt);
    adapter_.setLineNumbers(config_.showLineNumbers);
    adapter_.setWordWrap(config_.wordWrap);
    adapter_.setTabWidth(config_.tabWidth);
    adapter_.setUseTabs(config_.useTabs);
    adapter_.setEolMode(config_.eolMode);
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
