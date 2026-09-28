#pragma once

#include <gtk/gtk.h>
#include "document/Document.h"

#include <functional>
#include <string>
#include <vector>

namespace notepadx {

class TabBar {
public:
    using TabChangedCallback = std::function<void(int pageIndex, Document* doc)>;
    using TabCloseCallback = std::function<void(int pageIndex, Document* doc)>;
    using TabCloseOthersCallback = std::function<void(int keepPageIndex)>;
    using TabCloseAllCallback = std::function<void()>;
    using NewTabCallback = std::function<void()>;

    TabBar();
    ~TabBar() = default;

    TabBar(const TabBar&) = delete;
    TabBar& operator=(const TabBar&) = delete;

    [[nodiscard]] GtkWidget* widget() const noexcept { return notebook_; }

    void addTab(Document* doc, GtkWidget* pageWidget);
    void removeTab(int pageIndex);
    void updateTabTitle(int pageIndex);

    [[nodiscard]] int activeIndex() const;
    void setActiveIndex(int pageIndex);
    [[nodiscard]] int count() const;
    [[nodiscard]] Document* documentAt(int pageIndex) const;
    [[nodiscard]] GtkWidget* pageAt(int pageIndex) const;
    [[nodiscard]] int pageIndexOf(GtkWidget* pageWidget) const;
    [[nodiscard]] int indexOfDocument(const Document* doc) const;

    void setTabChangedCallback(TabChangedCallback cb);
    void setTabCloseCallback(TabCloseCallback cb);
    void setTabCloseOthersCallback(TabCloseOthersCallback cb);
    void setTabCloseAllCallback(TabCloseAllCallback cb);
    void setNewTabCallback(NewTabCallback cb);

private:
    struct TabData {
        Document* doc{nullptr};
        GtkWidget* labelWidget{nullptr};
        GtkWidget* closeBtn{nullptr};
        GtkWidget* tabBox{nullptr};
    };

    GtkWidget* createTabHeader(Document* doc, TabData& data);
    void showContextMenu(GdkEventButton* event, int pageIndex);

    static void onSwitchPage(GtkNotebook* notebook, GtkWidget* page, guint pageNum, gpointer userData);
    static gboolean onTabButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer userData);

    GtkWidget* notebook_{nullptr};
    std::vector<TabData> tabs_;

    TabChangedCallback tabChangedCb_{nullptr};
    TabCloseCallback tabCloseCb_{nullptr};
    TabCloseOthersCallback tabCloseOthersCb_{nullptr};
    TabCloseAllCallback tabCloseAllCb_{nullptr};
    NewTabCallback newTabCb_{nullptr};
    bool suppressSwitchSignal_{false};
};

} // namespace notepadx
