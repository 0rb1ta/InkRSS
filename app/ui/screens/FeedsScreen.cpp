#include "FeedsScreen.h"
#include "I18n.h"

#include "DirectSource.h"
#include "RowPager.h"
#include "Store.h"
#include "Theme.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FeedsScreen::FeedsScreen(AppController *app, QWidget *parent)
    : Screen(parent)
    , m_app(app)
{
    setObjectName(QStringLiteral("feeds"));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *bar = new TopBar(tx("Ленты и папки"), true, this);
    connect(bar, &TopBar::back, this, [this] { m_app->back(); });
    if (!m_app->fullText()) {
        QPushButton *opml = bar->addIconButton(QStringLiteral("file-lines"));
        connect(opml, &QPushButton::clicked, this, [this] { importOpml(); });
        QPushButton *add = bar->addIconButton(QStringLiteral("plus"));
        connect(add, &QPushButton::clicked, this, [this] {
            m_addRow->setVisible(!m_addRow->isVisible());
            if (m_addRow->isVisible())
                m_url->setFocus(); // the firmware keyboard opens below; the field stays above it
            else
                Ui::hideKeyboard(m_url);
        });
    }
    v->addWidget(bar);
    // The address is typed in a row on the screen itself, not in a dialog: a modal dialog sat
    // under the firmware keyboard, and closing it from the keyboard's Enter took the app down.
    m_addRow = new QWidget(this);
    auto *row = new QHBoxLayout(m_addRow);
    row->setContentsMargins(Theme::mm(3), Theme::mm(2), Theme::mm(3), Theme::mm(2));
    m_url = new QLineEdit(m_addRow);
    m_url->setFont(Theme::bodyFont());
    m_url->setPlaceholderText(tx("Адрес RSS-ленты"));
    auto *ok = new QPushButton(tx("Добавить"), m_addRow);
    ok->setFont(Theme::smallFont());
    row->addWidget(m_url, 1);
    row->addWidget(ok);
    connect(ok, &QPushButton::clicked, this, [this] { addFeed(); });
    // The firmware keyboard may not send Return: the button does the same.
    connect(m_url, &QLineEdit::returnPressed, this, [this] { addFeed(); });
    m_addRow->hide();
    v->addWidget(m_addRow);
    m_pager = new RowPager(this);
    m_pager->setEmptyText(m_app->fullText() ? tx("Лент нет. Добавьте их на сайте Miniflux и обновите.")
                                            : tx("Лент нет. Нажмите «+» или загрузите OPML-файл."));
    v->addWidget(m_pager, 1);
}

void FeedsScreen::pageKey(int direction)
{
    m_pager->flip(direction);
}

void FeedsScreen::shown()
{
    rebuild();
}

void FeedsScreen::rebuild()
{
    Store &store = m_app->store();
    const QFont body = Theme::bodyFont();
    QFont bold = body;
    bold.setWeight(QFont::Bold);
    const QFont small = Theme::smallFont();
    const int rowH = QFontMetrics(body).height() + Theme::mm(4);

    QList<RowPager::Row> rows;
    // `count` < 0: no counter; `error`: the feed's last update failure instead of the counter.
    auto addRow = [&](const QString &title, int count, const QString &error, bool folder, const ListScope &scope) {
        RowPager::Row r;
        r.height = [rowH](int) { return rowH; };
        r.make = [=](QWidget *parent) {
            auto *w = new TapRow([this, scope] { m_app->openList(scope); }, parent);
            auto *h = new QHBoxLayout(w);
            h->setContentsMargins(folder ? 0 : Theme::mm(5), 0, 0, 0);
            auto *t = new ElidedLabel(1, w);
            t->setFont(folder ? bold : body);
            t->setFullText(title);
            h->addWidget(t, 1);
            if (!error.isEmpty()) {
                auto *c = new ElidedLabel(1, w);
                c->setFont(small);
                c->setFullText(QStringLiteral("⚠ ") + error);
                c->setMaximumWidth(Theme::mm(45));
                h->addWidget(c);
            } else if (count >= 0) {
                h->addWidget(Ui::pill(count, w));
            }
            return w;
        };
        rows << r;
    };
    auto addFeeds = [&](qint64 categoryId) {
        for (const Feed &f : store.feeds(categoryId)) {
            ListScope s;
            s.kind = ListScope::Feed;
            s.id = f.id;
            s.title = f.title;
            const int n = store.unreadCount(f.id);
            addRow(f.title, n, f.error, false, s);
        }
    };
    for (const Category &c : store.categories()) {
        if (store.feeds(c.id).isEmpty())
            continue;
        ListScope s;
        s.kind = ListScope::Category;
        s.id = c.id;
        s.title = c.title;
        const int n = store.unreadCount(-1, c.id);
        addRow(c.title, n, QString(), true, s);
        addFeeds(c.id);
    }
    addFeeds(0);
    m_pager->setRows(rows, true);
}

void FeedsScreen::addFeed()
{
    Ui::hideKeyboard(m_url);
    QString url = m_url->text().trimmed();
    if (url.isEmpty())
        return;
    if (!url.contains(QLatin1String("://")))
        url.prepend(QStringLiteral("https://"));
    m_url->clear();
    m_addRow->hide();
    DirectSource::addFeed(m_app->store(), QUrl(url));
    Ui::toast(this, tx("Лента добавлена; статьи появятся после обновления"));
    rebuild();
}

void FeedsScreen::importOpml()
{
    const QString path = DirectSource::findOpml(m_app->storageRoot());
    if (path.isEmpty()) {
        Ui::toast(this, tx("Положите файл .opml в корень памяти читалки"));
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const int added = DirectSource::importOpml(m_app->store(), f.readAll());
    Ui::toast(this, tx("Добавлено лент: %1").arg(added));
    rebuild();
}
