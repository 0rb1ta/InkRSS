#include "Widgets.h"
#include "I18n.h"

#include "Log.h"

#include "Device.h"
#include "Qr.h"
#include "Shots.h"
#include "Theme.h"

#include <QDialog>
#include <QStackedWidget>
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QInputMethod>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace {

void setVariant(QWidget *w, const char *variant)
{
    w->setProperty("variant", QString::fromLatin1(variant));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ---------- ElidedButton ----------

ElidedButton::ElidedButton(const QString &text, QWidget *parent)
    : QPushButton(parent)
    , m_full(text)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    relayout();
}

void ElidedButton::setFullText(const QString &text)
{
    m_full = text;
    relayout();
}

void ElidedButton::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    relayout();
}

void ElidedButton::relayout()
{
    // Room for the text: the width minus the style's side padding (about the font's height).
    const QFontMetrics fm(font());
    const int room = width() - fm.height() - (icon().isNull() ? 0 : iconSize().width() + 4);
    const QString shown = room > fm.height() ? fm.elidedText(m_full, Qt::ElideRight, room) : m_full;
    if (text() != shown)
        setText(shown);
    setToolTip(m_full);
}


// ---------- ElidedLabel ----------

ElidedLabel::ElidedLabel(int maxLines, QWidget *parent)
    : QLabel(parent)
    , m_maxLines(maxLines)
{
    setWordWrap(true);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setMinimumWidth(1);
}

void ElidedLabel::setFullText(const QString &text)
{
    m_full = text;
    relayout();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    relayout();
}

void ElidedLabel::relayout()
{
    const QFontMetrics fm(font());
    const int width = qMax(10, contentsRect().width());
    QStringList lines;
    QString rest = m_full.simplified();
    while (!rest.isEmpty() && lines.size() < m_maxLines) {
        if (lines.size() == m_maxLines - 1 || fm.horizontalAdvance(rest) <= width) {
            lines << fm.elidedText(rest, Qt::ElideRight, width);
            break;
        }
        // Longest prefix that fits, broken at a space when possible.
        int cut = rest.size();
        while (cut > 1 && fm.horizontalAdvance(rest.left(cut)) > width)
            --cut;
        const int space = rest.left(cut).lastIndexOf(QLatin1Char(' '));
        if (space > 0)
            cut = space;
        lines << rest.left(cut).trimmed();
        rest = rest.mid(cut).trimmed();
    }
    setText(lines.join(QLatin1Char('\n')));
    setFixedHeight(fm.lineSpacing() * (m_reserveAll ? m_maxLines : qMax(1, lines.size())) + 2);
    setToolTip(m_full);
}


// ---------- TopBar ----------

TopBar::TopBar(const QString &title, bool withBack, QWidget *parent)
    : QFrame(parent)
{
    setStyleSheet(QStringLiteral("TopBar { border-bottom: 2px solid #000; }"));
    setFixedHeight(Theme::mm(12)); // same header height on every screen
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(Theme::mm(2), 0, Theme::mm(2), 0);
    h->setAlignment(Qt::AlignVCenter);
    if (withBack) {
        auto *back = new QPushButton(Theme::icon(QStringLiteral("arrow-left")), QString(), this);
        back->setObjectName(QStringLiteral("back"));
        back->setFixedSize(Theme::mm(10), Theme::mm(8));
        back->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
        connect(back, &QPushButton::clicked, this, &TopBar::back);
        h->addWidget(back, 0, Qt::AlignVCenter);
    }
    m_title = new QLabel(title, this);
    m_title->setFont(Theme::titleFont());
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(m_title, 1);
    m_right = new QHBoxLayout;
    m_right->setSpacing(Theme::mm(2));
    h->addLayout(m_right);
}

void TopBar::setTitleFont(const QFont &font)
{
    QFont f = font;
    f.setWeight(QFont::Normal);
    m_title->setFont(f);
}

QPushButton *TopBar::addIconButton(const QString &icon)
{
    // A framed look, only centred vertically in the fixed-height bar.
    auto *b = new QPushButton(Theme::icon(icon), QString(), this);
    b->setObjectName(icon);
    b->setFixedSize(Theme::mm(10), Theme::mm(8));
    b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
    m_right->addWidget(b, 0, Qt::AlignVCenter);
    return b;
}

void TopBar::setTitle(const QString &title)
{
    m_title->setText(title);
}


// ---------- Navigator ----------

namespace {

// After the event loop has laid the screen out, so pages are counted at their real size.
void logScreen(Screen *screen)
{
    QPointer<Screen> s(screen);
    QTimer::singleShot(0, [s] {
        if (!s)
            return;
        Log::write(QStringLiteral("screen: %1 %2").arg(s->objectName(), s->describe()));
        // With the "debug" file next to the log every screen is saved as a picture.
        if (Log::debugEnabled())
            Shots::save(s->window(), s->objectName());
    });
}

} // namespace

void Navigator::setRoot(Screen *screen)
{
    while (count() > 0) {
        QWidget *w = widget(0);
        removeWidget(w);
        w->deleteLater();
    }
    push(screen);
}

void Navigator::push(Screen *screen)
{
    Log::debug(QStringLiteral("push %1").arg(QString::fromLatin1(screen->metaObject()->className())));
    addWidget(screen);
    setCurrentWidget(screen);
    screen->shown();
    logScreen(screen);
}

void Navigator::pop()
{
    if (count() <= 1)
        return;
    QWidget *top = currentWidget();
    removeWidget(top);
    top->deleteLater();
    setCurrentIndex(count() - 1);
    if (Screen *s = current()) {
        s->shown();
        logScreen(s);
    }
}

Screen *Navigator::current() const
{
    return qobject_cast<Screen *>(currentWidget());
}

// ---------- dialogs ----------

namespace {

QDialog *frameDialog(QWidget *parent)
{
    auto *d = new QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog);
    d->setModal(true);
    d->setStyleSheet(QStringLiteral("QDialog { border: 3px solid #000; border-radius: %1px; }").arg(Theme::mm(1.5)));
    d->setMinimumWidth(parent ? parent->window()->width() * 85 / 100 : Theme::mm(110));
    return d;
}

void finishDialog(QDialog *d)
{
    d->adjustSize();
    if (QWidget *w = d->parentWidget()) {
        const QRect r = w->window()->geometry();
        d->move(r.center() - QPoint(d->width() / 2, d->height() / 2));
    }
}

QLabel *label(const QString &text, const QFont &font, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

} // namespace

bool Ui::showQr(QWidget *parent, const QString &title, const QString &subtitle, const QString &url,
                const QString &hint, const QString &actionText)
{
    const QImage qrImage = makeQrImage(url, qMax(3, Theme::mm(1.0)));
    if (qrImage.isNull()) {
        toast(parent, tx("Ссылка слишком длинная для QR-кода"));
        return false;
    }
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    if (!subtitle.isEmpty())
        v->addWidget(label(subtitle, Theme::bodyFont(), d));
    auto *qr = new QLabel(d);
    qr->setAlignment(Qt::AlignCenter);
    qr->setPixmap(QPixmap::fromImage(qrImage));
    v->addWidget(qr);
    QString shortUrl = url;
    shortUrl.remove(QStringLiteral("https://"));
    v->addWidget(label(shortUrl, Theme::smallFont(), d));
    if (!hint.isEmpty()) {
        QLabel *h = label(hint, Theme::smallFont(), d);
        h->setAlignment(Qt::AlignLeft);
        v->addWidget(h);
    }
    auto *row = new QHBoxLayout;
    auto *close = new QPushButton(Theme::icon(QStringLiteral("xmark")), tx("Закрыть"), d);
    row->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::reject);
    if (!actionText.isEmpty()) {
        auto *act = new QPushButton(Theme::icon(QStringLiteral("rotate")), actionText, d);
        setVariant(act, "dark");
        row->addWidget(act);
        QObject::connect(act, &QPushButton::clicked, d, &QDialog::accept);
    }
    v->addLayout(row);
    finishDialog(d);
    const bool accepted = d->exec() == QDialog::Accepted;
    d->deleteLater();
    return accepted;
}

bool Ui::confirm(QWidget *parent, const QString &text, const QString &okText)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(text, Theme::bodyFont(), d));
    auto *row = new QHBoxLayout;
    auto *no = new QPushButton(tx("Отмена"), d);
    auto *yes = new QPushButton(okText, d);
    setVariant(yes, "dark");
    row->addWidget(no);
    row->addWidget(yes);
    v->addLayout(row);
    QObject::connect(no, &QPushButton::clicked, d, &QDialog::reject);
    QObject::connect(yes, &QPushButton::clicked, d, &QDialog::accept);
    finishDialog(d);
    const bool ok = d->exec() == QDialog::Accepted;
    d->deleteLater();
    return ok;
}

int Ui::ask(QWidget *parent, const QString &title, const QString &text, const QString &first, const QString &second)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    v->addWidget(label(text, Theme::bodyFont(), d));
    int chosen = -1;
    auto *a = new QPushButton(first, d);
    auto *b = new QPushButton(second, d);
    setVariant(a, "dark");
    v->addWidget(a);
    v->addWidget(b);
    QObject::connect(a, &QPushButton::clicked, d, [d, &chosen] {
        chosen = 0;
        d->accept();
    });
    QObject::connect(b, &QPushButton::clicked, d, [d, &chosen] {
        chosen = 1;
        d->accept();
    });
    finishDialog(d);
    d->exec();
    d->deleteLater();
    return chosen;
}

int Ui::choose(QWidget *parent, const QString &title, const QStringList &options)
{
    QDialog *d = frameDialog(parent);
    auto *v = new QVBoxLayout(d);
    v->setContentsMargins(Theme::mm(4), Theme::mm(4), Theme::mm(4), Theme::mm(4));
    v->addWidget(label(title, Theme::titleFont(), d));
    int chosen = -1;
    for (int i = 0; i < options.size(); ++i) {
        auto *b = new QPushButton(options[i], d);
        v->addWidget(b);
        QObject::connect(b, &QPushButton::clicked, d, [d, &chosen, i] {
            chosen = i;
            d->accept();
        });
    }
    auto *cancel = new QPushButton(tx("Отмена"), d);
    v->addWidget(cancel);
    QObject::connect(cancel, &QPushButton::clicked, d, &QDialog::reject);
    // A menu, not a page: about 60% of the screen wide.
    if (parent) {
        const int w = parent->window()->width();
        d->setMinimumWidth(w * 55 / 100);
        d->setMaximumWidth(w * 65 / 100);
    }
    finishDialog(d);
    d->exec();
    d->deleteLater();
    return chosen;
}

void Ui::toast(QWidget *parent, const QString &text)
{
    QWidget *host = parent ? parent->window() : nullptr;
    if (!host)
        return;
    auto *l = new QLabel(text, host);
    l->setStyleSheet(QStringLiteral("background: #000; color: #fff; padding: %1px; border-radius: %2px;")
                         .arg(Theme::mm(2))
                         .arg(Theme::mm(1)));
    l->setFont(Theme::bodyFont());
    l->setWordWrap(true);
    l->setMaximumWidth(host->width() * 9 / 10);
    l->adjustSize();
    l->move((host->width() - l->width()) / 2, host->height() - l->height() - Theme::mm(16));
    l->show();
    l->raise();
    QTimer::singleShot(2500, l, &QObject::deleteLater);
}

void Ui::hideKeyboard(QWidget *input)
{
    if (input)
        input->clearFocus();
    QGuiApplication::inputMethod()->hide();
}

// ---------- building blocks ----------

QPushButton *Ui::bigButton(const QString &icon, const QString &text, bool dark, QWidget *parent)
{
    auto *b = new QPushButton(dark ? Theme::icon(icon, Qt::white) : Theme::icon(icon), QStringLiteral(" ") + text, parent);
    b->setObjectName(icon);
    b->setMinimumHeight(Theme::mm(10));
    b->setFont(Theme::bodyFont());
    b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
    if (dark)
        b->setProperty("variant", "dark");
    return b;
}

QLabel *Ui::centered(const QString &text, const QFont &font, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setAlignment(Qt::AlignCenter);
    l->setWordWrap(true);
    return l;
}

QLabel *Ui::logo(int width, QWidget *parent)
{
    QSvgRenderer renderer(QStringLiteral(":/logo.svg"));
    const QSize size = renderer.defaultSize().scaled(width, width * 4, Qt::KeepAspectRatio);
    QPixmap pm(size);
    pm.fill(Qt::white);
    QPainter p(&pm);
    renderer.render(&p);
    p.end();
    auto *l = new QLabel(parent);
    l->setPixmap(pm);
    l->setFixedSize(size);
    return l;
}

QLabel *Ui::pill(int count, QWidget *parent)
{
    auto *l = new QLabel(QString::number(count), parent);
    QFont f = Theme::smallFont();
    f.setWeight(count ? QFont::Bold : QFont::Normal);
    l->setFont(f);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral("border: 2px %1 #000; border-radius: %2px; padding: 0 %3px; color: %4;")
                         .arg(count ? QStringLiteral("solid") : QStringLiteral("dashed"))
                         .arg(Theme::mm(2.2))
                         .arg(Theme::mm(1.5))
                         .arg(count ? QStringLiteral("#000") : QStringLiteral("#444")));
    l->setMinimumWidth(Theme::mm(8));
    return l;
}

QPushButton *Ui::chip(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setFont(Theme::smallFont());
    // Own min-height: the shared 8 mm would cut the text of a small button.
    b->setStyleSheet(QStringLiteral("QPushButton { border: 2px solid #000; border-radius: %1px; padding: %2px %3px; min-height: 0; }")
                         .arg(Theme::mm(2.5))
                         .arg(Theme::mm(1))
                         .arg(Theme::mm(2.5)));
    return b;
}

int Ui::cornerMenu(QWidget *anchor, const QList<MenuItem> &items, const FontPanel *font, int fontItem)
{
    // A popup closes by itself on a tap outside it.
    auto *d = new QDialog(anchor, Qt::Popup | Qt::FramelessWindowHint);
    d->setObjectName(QStringLiteral("menu"));
    d->setStyleSheet(QStringLiteral("QDialog { border: 2px solid #000; border-radius: %1px; background: #fff; }"
                                    "QPushButton#item { border: none; border-bottom: 1px solid #999; border-radius: 0;"
                                    " text-align: left; padding: 0 %2px; min-height: %3px; }"
                                    "QWidget#row { border-bottom: 1px solid #999; }")
                         .arg(Theme::mm(1.2))
                         .arg(Theme::mm(3))
                         .arg(Theme::mm(10)));
    // The pages sit in one column and only the shown one is visible, so the popup is always exactly
    // as tall as its current page (a stacked widget keeps the height of the tallest page).
    auto *outer = new QVBoxLayout(d);
    outer->setContentsMargins(2, 2, 2, 2);
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetFixedSize);
    QList<QWidget *> pages;
    const QPoint corner = anchor->mapToGlobal(QPoint(anchor->width(), anchor->height() + Theme::mm(1)));
    const int width = Theme::mm(72);
    auto show = [d, outer, corner, &pages](QWidget *page) {
        for (QWidget *p : pages)
            p->setVisible(p == page);
        outer->activate();
        d->move(corner.x() - d->width(), corner.y());
    };
    auto page = [d, outer, &pages, width](QVBoxLayout **v) {
        auto *w = new QWidget(d);
        w->setFixedWidth(width);
        *v = new QVBoxLayout(w);
        (*v)->setContentsMargins(0, 0, 0, 0);
        (*v)->setSpacing(0);
        outer->addWidget(w);
        pages << w;
        return w;
    };
    auto item = [](const QString &icon, const QString &text, QWidget *parent, bool bold = false) {
        auto *b = new QPushButton(icon.isEmpty() ? QIcon() : Theme::icon(icon), QStringLiteral("  ") + text, parent);
        b->setObjectName(QStringLiteral("item"));
        QFont f = Theme::bodyFont();
        if (bold)
            f.setWeight(QFont::Bold);
        b->setFont(f);
        b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
        return b;
    };
    int chosen = -1;

    QVBoxLayout *v;
    QWidget *menuPage = page(&v);
    QWidget *fontPage = nullptr;
    for (int i = 0; i < items.size(); ++i) {
        const bool opensFont = font && i == fontItem;
        QPushButton *b = item(items[i].icon, items[i].text + (opensFont ? QStringLiteral("  ›") : QString()), menuPage);
        v->addWidget(b);
        QObject::connect(b, &QPushButton::clicked, d, [d, &chosen, i, opensFont, &fontPage, show, &items] {
            Log::write(QStringLiteral("menu: %1").arg(items[i].icon));
            if (opensFont)
                return show(fontPage);
            chosen = i;
            d->accept();
        });
    }

    if (font) {
        // "Шрифт и размер": the font row and one stepper per size.
        fontPage = page(&v);
        QPushButton *back = item(QStringLiteral("arrow-left"), tx("Шрифт и размер"), fontPage, true);
        v->addWidget(back);
        QObject::connect(back, &QPushButton::clicked, d, [show, menuPage] { show(menuPage); });
        QPushButton *fontRow = item(QString(), tx("Шрифт"), fontPage);
        auto *fh = new QHBoxLayout(fontRow);
        fh->setContentsMargins(0, 0, Theme::mm(3), 0);
        auto *current = new QLabel(fontRow);
        current->setFont(Theme::bodyFont());
        current->setAttribute(Qt::WA_TransparentForMouseEvents);
        current->setStyleSheet(QStringLiteral("background: transparent;"));
        fh->addStretch(1);
        fh->addWidget(current);
        auto setCurrent = [current](const QString &family) {
            current->setText((family.isEmpty() ? tx("По умолчанию") : family) + QStringLiteral("  ›"));
        };
        setCurrent(font->family);
        v->addWidget(fontRow);
        for (const SizeRow &size : font->sizes) {
            auto *row = new QWidget(fontPage);
            row->setObjectName(QStringLiteral("row"));
            row->setAttribute(Qt::WA_StyledBackground);
            auto *h = new QHBoxLayout(row);
            h->setContentsMargins(Theme::mm(3), Theme::mm(1.5), Theme::mm(2), Theme::mm(1.5));
            auto *l = new QLabel(size.title, row);
            l->setFont(Theme::bodyFont());
            h->addWidget(l, 1);
            auto *s = new Stepper(size.value, size.min, size.max, row);
            const auto changed = size.changed;
            QObject::connect(s, &Stepper::changed, d, [changed](int value) { changed(value); });
            h->addWidget(s);
            v->addWidget(row);
        }

        // The font list, each name in its own typeface.
        QWidget *listPage = page(&v);
        QPushButton *listBack = item(QStringLiteral("arrow-left"), tx("Шрифт"), listPage, true);
        v->addWidget(listBack);
        QObject::connect(listBack, &QPushButton::clicked, d, [show, fontPage] { show(fontPage); });
        auto family = std::make_shared<QString>(font->family);
        const QStringList families = Theme::articleFamilies();
        QList<QPushButton *> names;
        for (int i = 0; i < families.size(); ++i) {
            QPushButton *b = item(QString(), families[i], listPage);
            if (i > 0) {
                QFont f = b->font();
                f.setFamily(families[i]);
                f.setWeight(QFont::Normal);
                b->setFont(f);
            }
            names << b;
            v->addWidget(b);
        }
        auto mark = [names, families, family] {
            for (int i = 0; i < names.size(); ++i) {
                const bool on = i == 0 ? family->isEmpty() : families[i] == *family;
                names[i]->setText((on ? QStringLiteral("✓ ") : QStringLiteral("  ")) + families[i]);
            }
        };
        mark();
        const auto fontChanged = font->fontChanged;
        for (int i = 0; i < names.size(); ++i)
            QObject::connect(names[i], &QPushButton::clicked, d,
                             [i, families, family, mark, setCurrent, fontChanged, show, fontPage] {
                                 *family = i == 0 ? QString() : families[i];
                                 mark();
                                 setCurrent(*family);
                                 fontChanged(*family);
                                 show(fontPage);
                             });
        QObject::connect(fontRow, &QPushButton::clicked, d, [show, listPage] { show(listPage); });
    }

    show(menuPage);
    d->exec();
    d->deleteLater();
    if (chosen < 0)
        Log::write(QStringLiteral("menu: closed"));
    return chosen;
}

// ---------- font and size ----------

Stepper::Stepper(int value, int min, int max, QWidget *parent)
    : QWidget(parent)
    , m_value(qBound(min, value, max))
    , m_min(min)
    , m_max(max)
{
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    const QString frame = QStringLiteral("border: 2px solid #000; min-height: 0; padding: 0; border-radius: %1px;").arg(Theme::mm(0.8));
    auto button = [&](const QString &text, int delta, const QString &name) {
        auto *b = new QPushButton(text, this);
        b->setObjectName(name);
        QFont f = Theme::bodyFont();
        f.setWeight(QFont::Bold);
        b->setFont(f);
        b->setFixedSize(Theme::mm(12), Theme::mm(9));
        b->setStyleSheet(QStringLiteral("QPushButton { %1 }").arg(frame));
        connect(b, &QPushButton::clicked, this, [this, delta] { step(delta); });
        return b;
    };
    m_label = new QLabel(QString::number(m_value), this);
    m_label->setFont(Theme::bodyFont());
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setFixedWidth(Theme::mm(11));
    h->addWidget(button(QStringLiteral("A−"), -1, QStringLiteral("size-")));
    h->addWidget(m_label);
    h->addWidget(button(QStringLiteral("A+"), 1, QStringLiteral("size+")));
}

void Stepper::step(int delta)
{
    const int next = qBound(m_min, m_value + delta, m_max);
    if (next == m_value)
        return;
    m_value = next;
    m_label->setText(QString::number(m_value));
    emit changed(m_value);
}

