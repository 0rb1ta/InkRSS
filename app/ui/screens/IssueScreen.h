#pragma once

#include "AppController.h"
#include "Models.h"
#include "Widgets.h"

#include <QHash>

class QPushButton;
class RowPager;

// The main screen (the issue: lead article, folders as sections, headlines, a closing page) and
// the plain lists of one feed, one folder or the starred articles. Paged, no scrolling.
class IssueScreen : public Screen {
    Q_OBJECT
public:
    IssueScreen(AppController *app, const ListScope &scope, QWidget *parent = nullptr);
    void pageKey(int direction) override;
    void shown() override;
    QString describe() const override;
    void startSync();

private:
    void reload(); // new snapshot of what the screen lists
    void rebuild(bool keepPage);
    void updateFooter();
    void markAllRead();
    QWidget *endPage(QWidget *parent);

    AppController *m_app;
    ListScope m_scope;
    TopBar *m_bar = nullptr;
    QPushButton *m_menu = nullptr;
    RowPager *m_pager = nullptr;
    QList<qint64> m_ids; // the snapshot: articles stay in place when read
    QHash<qint64, Entry> m_entries;
    QHash<qint64, QString> m_feedTitles;
    QString m_progress; // "Обновление… 3 / 10" while a sync runs
    bool m_loaded = false;
};
