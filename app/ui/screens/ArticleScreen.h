#pragma once

#include "AppController.h"
#include "Models.h"
#include "Widgets.h"

class QTextDocument;

// One article in pages under a header (back, feed, ☰). Pages turn with the keys and swipes; a tap
// on a link shows its QR code. Opening it marks the article read.
class ArticleScreen : public Screen {
    Q_OBJECT
public:
    ArticleScreen(AppController *app, qint64 id, QWidget *parent = nullptr);
    ~ArticleScreen() override;
    void pageKey(int direction) override;
    QString describe() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void layoutDocument();
    void showMenu();
    QRect pageRect() const;

    AppController *m_app;
    Entry m_entry;
    QTextDocument *m_doc;
    TopBar *m_bar;
    QPushButton *m_menu;
    QString m_feed;
    int m_images = 0;
    int m_page = 0;
};
