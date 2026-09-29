#pragma once

#include "editor/EditorConfig.h"
#include "editor/ScintillaAdapter.h"

#include <functional>
#include <memory>

namespace notepadx {

class Editor {
public:
    using ContentChangedCallback = std::function<void()>;
    using ModifiedChangedCallback = std::function<void(bool modified)>;
    using UpdateUiCallback = std::function<void()>;

    explicit Editor(const EditorConfig& config = EditorConfig::createDefault());
    ~Editor() = default;

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    Editor(Editor&&) = delete;
    Editor& operator=(Editor&&) = delete;

    [[nodiscard]] GtkWidget* widget() const noexcept { return adapter_.widget(); }
    [[nodiscard]] ScintillaAdapter& adapter() noexcept { return adapter_; }
    [[nodiscard]] const ScintillaAdapter& adapter() const noexcept { return adapter_; }

    void applyConfig(const EditorConfig& config);
    [[nodiscard]] const EditorConfig& config() const noexcept { return config_; }

    [[nodiscard]] bool isModified() const;
    void setSavePoint();

    using MarginClickCallback = std::function<void(int line, int margin)>;

    void setContentChangedCallback(ContentChangedCallback cb);
    void setModifiedChangedCallback(ModifiedChangedCallback cb);
    void setUpdateUiCallback(UpdateUiCallback cb);
    void setMarginClickCallback(MarginClickCallback cb);

private:
    void handleNotification(const SCNotification* scn);

    ScintillaAdapter adapter_;
    EditorConfig config_;
    ContentChangedCallback contentChangedCb_{nullptr};
    ModifiedChangedCallback modifiedChangedCb_{nullptr};
    UpdateUiCallback updateUiCb_{nullptr};
    MarginClickCallback marginClickCb_{nullptr};
    bool isModified_{false};
};

} // namespace notepadx
