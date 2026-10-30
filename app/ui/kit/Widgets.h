#pragma once

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>

#include <functional>

class QHBoxLayout;

// A label that keeps to `maxLines` lines and ends with "…" when the text is longer.
class ElidedLabel : public QLabel {
    Q_OBJECT
public:
    ElidedLabel(int maxLines, QWidget *parent = nullptr);
    void setFullText(const QString &text);
    QString fullText() const { return m_full; }
    void setReserveAllLines(bool on) { m_reserveAll = on; relayout(); }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    QString m_full;
    int m_maxLines;
    bool m_reserveAll = true;
};

// A button whose label is cut with "…" instead of widening the button.
class ElidedButton : public QPushButton {
    Q_OBJECT
public:
    explicit ElidedButton(const QString &text, QWidget *parent = nullptr);
    void setFullText(const QString &text);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    QString m_full;
};

// Every screen: receives the hardware page keys.
class Screen : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual void pageKey(int direction) { Q_UNUSED(direction); }
    virtual void shown() {} // when it becomes the visible screen again (refresh data)
    // One line for the log: what the screen shows now ("rows=12 pages=3 page=1").
    virtual QString describe() const { return QString(); }
};

// Header: optional back arrow, title, icon buttons on the right.
class TopBar : public QFrame {
    Q_OBJECT
public:
    TopBar(const QString &title, bool withBack, QWidget *parent = nullptr);
    QPushButton *addIconButton(const QString &icon);
    void setTitle(const QString &title);
    void setTitleFont(const QFont &font);

signals:
    void back();

private:
    QLabel *m_title;
    QHBoxLayout *m_right;
};

// Screen stack.
class Navigator : public QStackedWidget {
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;
    void setRoot(Screen *screen);
    void push(Screen *screen);
    void pop();
    Screen *current() const;
    int depth() const { return count(); }
};

// "A− 10 A+": a number changed one step at a time, within [min, max].
class Stepper : public QWidget {
    Q_OBJECT
public:
    Stepper(int value, int min, int max, QWidget *parent = nullptr);
    int value() const { return m_value; }
signals:
    void changed(int value);

private:
    void step(int delta);
    int m_value;
    int m_min;
    int m_max;
    QLabel *m_label;
};

namespace Ui {
// The large buttons of the start and login pages: icon, text, at least 10 mm tall.
QPushButton *bigButton(const QString &icon, const QString &text, bool dark, QWidget *parent);
QLabel *centered(const QString &text, const QFont &font, QWidget *parent);
QLabel *logo(int width, QWidget *parent);
// A count in a rounded frame ("31"); a zero is drawn with a dashed frame.
QLabel *pill(int count, QWidget *parent);
// A small framed button at the right end of a settings row.
QPushButton *chip(const QString &text, QWidget *parent);

struct MenuItem {
    QString icon;
    QString text;
};
struct SizeRow {
    QString title;
    int value;
    int min;
    int max;
    std::function<void(int value)> changed; // applied at once, the screen behind redraws
};
// The "Шрифт и размер" page of a menu: the article font (a list on its own page) and steppers.
struct FontPanel {
    QString family;
    std::function<void(const QString &family)> fontChanged;
    QList<SizeRow> sizes;
};
// A short menu under `anchor` (the ☰ button), right-aligned. Returns the chosen index or -1.
// Item `fontItem` turns the menu into the font page in place, with "←" back.
int cornerMenu(QWidget *anchor, const QList<MenuItem> &items, const FontPanel *font = nullptr, int fontItem = -1);
// Modal QR dialog. Returns true when `actionText` was pressed.
bool showQr(QWidget *parent, const QString &title, const QString &subtitle, const QString &url,
            const QString &hint, const QString &actionText = {});
bool confirm(QWidget *parent, const QString &text, const QString &okText);
// Returns the chosen index or -1.
int choose(QWidget *parent, const QString &title, const QStringList &options);
// A question with two answers, the first one dark: 0 or 1, -1 when closed without an answer.
int ask(QWidget *parent, const QString &title, const QString &text, const QString &first, const QString &second);
void toast(QWidget *parent, const QString &text);
// Closes the firmware on-screen keyboard (it is modal: the app does not react while it is open).
void hideKeyboard(QWidget *input);
} // namespace Ui
