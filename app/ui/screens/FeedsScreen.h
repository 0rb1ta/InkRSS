#pragma once

#include "AppController.h"
#include "Widgets.h"

class QLineEdit;
class RowPager;

// Folders and feeds with unread counts and update errors. Without a server, feeds are added
// here (by address or from an OPML file).
class FeedsScreen : public Screen {
    Q_OBJECT
public:
    explicit FeedsScreen(AppController *app, QWidget *parent = nullptr);
    void pageKey(int direction) override;
    void shown() override;

private:
    void rebuild();
    void addFeed();
    void importOpml();

    AppController *m_app;
    RowPager *m_pager;
    QWidget *m_addRow = nullptr;
    QLineEdit *m_url = nullptr;
};
