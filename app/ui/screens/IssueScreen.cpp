#include "IssueScreen.h"
#include "I18n.h"

#include "Issue.h"
#include "Log.h"
#include "RowPager.h"
#include "Store.h"
#include "Theme.h"
#include "UpdateCheck.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QTextLayout>
#include <QVBoxLayout>

namespace {

// Sizes in millimetres of the reader's screen.
const double kLeadMm = 5.8;
const double kHeadlineMm = 4.7;
const double kExcerptMm = 4.2;
const double kMetaMm = 3.4;
const double kSectionMm = 3.6;

// Word-wrapped lines of `text`, at most `maxLines`; the last one ends with "…" when cut.
QStringList wrap(const QFont &font, const QString &text, int width, int maxLines)
{
    QStringList out;
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(qMax(1, width));
        if (out.size() == maxLines - 1) {
            const QString rest = text.mid(line.textStart()).simplified();
            out << QFontMetrics(font).elidedText(rest, Qt::ElideRight, width);
            break;
        }
        out << text.mid(line.textStart(), line.textLength()).trimmed();
    }
    layout.endLayout();
    return out;
}

// A list row drawn by hand: the same numbers decide its height and what it paints, so a page
// never has gaps or cut lines.
class PaintRow : public QWidget {
public:
    PaintRow(std::function<void(QPainter &, int width)> paint, std::function<void()> onClick, QWidget *parent)
        : QWidget(parent)
        , m_paint(std::move(paint))
        , m_onClick(std::move(onClick))
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        m_paint(p, width());
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_onClick && rect().contains(event->pos()))
            m_onClick();
    }

private:
    std::function<void(QPainter &, int)> m_paint;
    std::function<void()> m_onClick;
};

void drawLines(QPainter &p, const QFont &font, const QStringList &lines, int x, int y, const QColor &color)
{
    const QFontMetrics fm(font);
    p.setFont(font);
    p.setPen(color);
    for (int i = 0; i < lines.size(); ++i)
        p.drawText(x, y + i * fm.lineSpacing() + fm.ascent(), lines[i]);
}

void rule(QPainter &p, int width, int y, int thickness, Qt::PenStyle style)
{
    p.setPen(QPen(style == Qt::SolidLine ? Qt::black : QColor(0x88, 0x88, 0x88), thickness, style));
    p.drawLine(0, y, width, y);
}

// "N + 1 · 3 мин" fitted into `maxWidth`: the feed name is cut, the minutes never are.
QString fitMeta(const QFont &font, const QString &feed, int minutes, int maxWidth)
{
    const QFontMetrics fm(font);
    const QString full = headlineMeta(feed, minutes);
    if (fm.horizontalAdvance(full) <= maxWidth || feed.isEmpty())
        return full;
    const QString tail = minutes > 0 ? QStringLiteral(" · ") + headlineMeta(QString(), minutes) : QString();
    return fm.elidedText(feed, Qt::ElideRight, qMax(0, maxWidth - fm.horizontalAdvance(tail))) + tail;
}

} // namespace

IssueScreen::IssueScreen(AppController *app, const ListScope &scope, QWidget *parent)
    : Screen(parent)
    , m_app(app)
    , m_scope(scope)
{
    const bool issue = scope.kind == ListScope::Issue;
    setObjectName(issue ? QStringLiteral("issue") : QStringLiteral("list"));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    m_bar = new TopBar(issue ? QStringLiteral("InkRSS") : scope.title, !issue, this);
    connect(m_bar, &TopBar::back, this, [this] { m_app->back(); });
    if (issue) {
        // Shown only when GitHub has a newer release; opens a QR with the release page.
        QPushButton *update = m_bar->addIconButton(QStringLiteral("circle-up"));
        UpdateCheck *updates = m_app->updates();
        update->setVisible(updates->available());
        connect(updates, &UpdateCheck::found, update, [update] { update->setVisible(true); });
        connect(update, &QPushButton::clicked, this, [this, updates] {
            Ui::showQr(this, tx("Вышла InkRSS %1").arg(updates->latestVersion()),
                       tx("У вас версия %1").arg(QStringLiteral(APP_VERSION)), updates->releaseUrl(),
                       tx("Откройте страницу релиза на телефоне или компьютере, скачайте архив и скопируйте его на читалку по USB поверх старых файлов. Подключение и статьи сохранятся."));
        });
    }
    if (scope.kind != ListScope::Starred) {
        QPushButton *all = m_bar->addIconButton(QStringLiteral("check-double"));
        connect(all, &QPushButton::clicked, this, [this] { markAllRead(); });
    }
    if (issue) {
        QPushButton *refresh = m_bar->addIconButton(QStringLiteral("rotate"));
        connect(refresh, &QPushButton::clicked, this, [this] { startSync(); });
        m_menu = m_bar->addIconButton(QStringLiteral("bars"));
        connect(m_menu, &QPushButton::clicked, this, [this] { m_app->showMenu(m_menu); });
    } else if (scope.kind == ListScope::Feed && !m_app->fullText()) {
        QPushButton *remove = m_bar->addIconButton(QStringLiteral("xmark"));
        connect(remove, &QPushButton::clicked, this, [this] {
            if (!Ui::confirm(this, tx("Отписаться от ленты «%1»?").arg(m_scope.title), tx("Отписаться")))
                return;
            m_app->removeFeed(m_scope.id);
            m_app->back();
        });
    }
    v->addWidget(m_bar);
    m_pager = new RowPager(this);
    if (issue) {
        m_pager->setEmptyText(m_app->store().feeds().isEmpty() && !m_app->fullText()
                                  ? tx("Лент пока нет.\nМеню → «Ленты и папки» → «+»")
                                  : tx("Новых статей нет.\nНажмите «Обновить» вверху."));
        m_pager->setEndPage([this](QWidget *parent) { return endPage(parent); });
    } else {
        m_pager->setEmptyText(tx("Здесь пока пусто"));
    }
    v->addWidget(m_pager, 1);
}

QString IssueScreen::describe() const
{
    return QStringLiteral("articles=%1 rows=%2 pages=%3 page=%4")
        .arg(m_ids.size()).arg(m_pager->rowCount()).arg(m_pager->pageCount()).arg(m_pager->page() + 1);
}

void IssueScreen::pageKey(int direction)
{
    m_pager->flip(direction);
    Log::write(QStringLiteral("page: %1 %2/%3").arg(objectName()).arg(m_pager->page() + 1).arg(m_pager->pageCount()));
}

void IssueScreen::shown()
{
    if (!m_loaded) {
        reload();
        return;
    }
    // Back from an article: same articles, fresh read marks, same page.
    for (qint64 id : m_ids)
        m_entries.insert(id, m_app->store().entry(id));
    rebuild(true);
}

void IssueScreen::reload()
{
    m_loaded = true;
    Store &store = m_app->store();
    QList<Entry> list;
    switch (m_scope.kind) {
    case ListScope::Issue: list = store.entries(-1, -1, true, 2000); break;
    case ListScope::Feed: list = store.entries(m_scope.id, -1, false, 500); break;
    case ListScope::Category: list = store.entries(-1, m_scope.id, false, 500); break;
    case ListScope::Starred: list = store.starred(); break;
    }
    m_ids.clear();
    m_entries.clear();
    for (const Entry &e : list) {
        m_ids << e.id;
        m_entries.insert(e.id, e);
    }
    m_feedTitles.clear();
    for (const Feed &f : store.feeds())
        m_feedTitles.insert(f.id, f.title);
    rebuild(false);
}

void IssueScreen::rebuild(bool keepPage)
{
    QList<Entry> list;
    for (qint64 id : m_ids)
        if (m_entries.value(id).id)
            list << m_entries.value(id);

    QList<IssueItem> items;
    if (m_scope.kind == ListScope::Issue) {
        items = buildIssue(list, m_app->store().feeds(), m_app->store().categories(),
                           m_app->config().flatIssue ? IssueLayout::Flat : IssueLayout::Sections);
    } else {
        for (const Entry &e : list) {
            IssueItem it;
            it.entryId = e.id;
            items << it;
        }
    }

    const QString family = m_app->config().fontFamily;
    // Each step of the headline size is 10%.
    const double k = 1.0 + 0.1 * (m_app->config().headlineSize - Config::kHeadlineSizeDefault);
    const QFont leadFont = Theme::articleFont(family, kLeadMm * k, true);
    const QFont titleFont = Theme::articleFont(family, kHeadlineMm * k);
    const QFont excerptFont = Theme::articleFont(family, kExcerptMm * k);
    QFont metaFont = Theme::smallFont();
    metaFont.setPixelSize(Theme::mm(kMetaMm * k));
    metaFont.setWeight(QFont::Normal);
    QFont sectionFont = Theme::smallFont();
    sectionFont.setPixelSize(Theme::mm(kSectionMm));
    sectionFont.setWeight(QFont::Bold);
    const int pad = Theme::mm(1.6);
    const int gap = Theme::mm(2);
    const int metaMax = Theme::mm(38);
    const QColor ink(Qt::black);
    const QColor faded(0x55, 0x55, 0x55);

    QList<RowPager::Row> rows;
    for (const IssueItem &it : items) {
        RowPager::Row row;
        switch (it.kind) {
        case IssueItem::Lead: {
            const Entry e = m_entries.value(it.entryId);
            const QString meta = headlineMeta(m_feedTitles.value(e.feedId), e.readingMinutes);
            const QString excerpt = meta + QStringLiteral(" · ") + leadExcerpt(e.html, 400);
            const QColor color = e.read ? faded : ink;
            auto layoutOf = [=](int w) {
                return qMakePair(wrap(leadFont, e.title, w, 3), wrap(excerptFont, excerpt, w, 2));
            };
            row.height = [=](int w) {
                const auto l = layoutOf(w);
                return pad + l.first.size() * QFontMetrics(leadFont).lineSpacing() + Theme::mm(1)
                    + l.second.size() * QFontMetrics(excerptFont).lineSpacing() + pad * 2;
            };
            row.make = [=](QWidget *parent) -> QWidget * {
                return new PaintRow(
                    [=](QPainter &p, int w) {
                        const auto l = layoutOf(w);
                        drawLines(p, leadFont, l.first, 0, pad, color);
                        drawLines(p, excerptFont, l.second, 0,
                                  pad + l.first.size() * QFontMetrics(leadFont).lineSpacing() + Theme::mm(1), QColor(0x22, 0x22, 0x22));
                    },
                    [this, id = e.id] { m_app->openArticle(id); }, parent);
            };
            break;
        }
        case IssueItem::Section: {
            const QString title = it.title.toUpper();
            const QString count = QStringLiteral("%1 ›").arg(it.count);
            ListScope scope;
            scope.kind = it.feedId > 0 ? ListScope::Feed : ListScope::Category;
            scope.id = it.feedId > 0 ? it.feedId : it.categoryId;
            scope.title = it.title;
            const int h = Theme::mm(3) + QFontMetrics(sectionFont).height() + Theme::mm(1);
            row.height = [=](int) { return h; };
            row.make = [=](QWidget *parent) -> QWidget * {
                std::function<void()> open;
                if (scope.id > 0)
                    open = [this, scope] { m_app->openList(scope); };
                return new PaintRow(
                    [=](QPainter &p, int w) {
                        const QFontMetrics fm(sectionFont);
                        p.setFont(sectionFont);
                        p.setPen(ink);
                        const int base = h - Theme::mm(1) - fm.descent();
                        const int countW = fm.horizontalAdvance(count);
                        p.drawText(0, base, fm.elidedText(title, Qt::ElideRight, w - countW - gap));
                        p.drawText(w - countW, base, count);
                        rule(p, w, h - 1, 2, Qt::SolidLine);
                    },
                    open, parent);
            };
            break;
        }
        case IssueItem::Headline: {
            const Entry e = m_entries.value(it.entryId);
            const QString meta = fitMeta(metaFont, m_feedTitles.value(e.feedId), e.readingMinutes, metaMax);
            const int metaW = QFontMetrics(metaFont).horizontalAdvance(meta);
            const QColor color = e.read ? faded : ink;
            auto linesOf = [=](int w) { return wrap(titleFont, e.title, w - metaW - gap, 2); };
            row.height = [=](int w) {
                return qMax(linesOf(w).size() * QFontMetrics(titleFont).lineSpacing(), QFontMetrics(metaFont).height())
                    + pad * 2;
            };
            row.make = [=](QWidget *parent) -> QWidget * {
                return new PaintRow(
                    [=](QPainter &p, int w) {
                        const QStringList lines = linesOf(w);
                        drawLines(p, titleFont, lines, 0, pad, color);
                        // The meta sits on the first line's baseline.
                        const int base = pad + QFontMetrics(titleFont).ascent();
                        p.setFont(metaFont);
                        p.setPen(QColor(0x44, 0x44, 0x44));
                        p.drawText(w - metaW, base, meta);
                        rule(p, w, p.device()->height() - 1, 1, Qt::DotLine);
                    },
                    [this, id = e.id] { m_app->openArticle(id); }, parent);
            };
            break;
        }
        }
        rows << row;
    }
    m_pager->setRows(rows, keepPage);
    updateFooter();
}

void IssueScreen::updateFooter()
{
    if (m_scope.kind != ListScope::Issue)
        return;
    if (!m_progress.isEmpty()) {
        m_pager->setFooter(m_progress, QString());
        return;
    }
    int unread = 0;
    int minutes = 0;
    for (qint64 id : m_ids) {
        const Entry &e = m_entries[id];
        if (!e.read) {
            ++unread;
            minutes += e.readingMinutes;
        }
    }
    const IssueFooter f = issueFooter(m_app->store().meta(QStringLiteral("lastSync")).toLongLong(), unread, minutes,
                                      QDateTime::currentDateTime());
    m_pager->setFooter(f.left, f.right);
}

void IssueScreen::startSync()
{
    if (m_app->syncing())
        return;
    m_progress = tx("Обновление…");
    updateFooter();
    QPointer<IssueScreen> self(this);
    m_app->sync(
        [self](int done, int total) {
            if (!self)
                return;
            self->m_progress = tx("Обновление… %1 / %2").arg(done).arg(total);
            self->updateFooter();
        },
        [self](QString error) {
            if (!self)
                return;
            self->m_progress.clear();
            self->reload();
            if (!error.isEmpty()) {
                self->m_pager->setFooter(error, QString());
                Ui::toast(self, error);
            }
        });
}

void IssueScreen::markAllRead()
{
    Store &store = m_app->store();
    switch (m_scope.kind) {
    case ListScope::Issue:
        if (!Ui::confirm(this, tx("Отметить все статьи выпуска прочитанными?"), tx("Отметить")))
            return;
        for (qint64 id : m_ids)
            if (!m_entries[id].read)
                store.setRead(id, true);
        break;
    case ListScope::Feed:
        if (!Ui::confirm(this, tx("Отметить всю ленту прочитанной?"), tx("Отметить")))
            return;
        store.markAllRead(m_scope.id, -1);
        break;
    case ListScope::Category:
        if (!Ui::confirm(this, tx("Отметить всю папку прочитанной?"), tx("Отметить")))
            return;
        if (m_scope.id > 0)
            store.markAllRead(-1, m_scope.id);
        else
            for (qint64 id : m_ids)
                store.setRead(id, true);
        break;
    case ListScope::Starred:
        return;
    }
    Log::write(QStringLiteral("mark all read: %1 articles=%2").arg(objectName()).arg(m_ids.size()));
    for (qint64 id : m_ids)
        m_entries.insert(id, store.entry(id));
    rebuild(true);
}

QWidget *IssueScreen::endPage(QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *v = new QVBoxLayout(w);
    v->setAlignment(Qt::AlignCenter);
    v->setSpacing(Theme::mm(4));
    int read = 0;
    for (qint64 id : m_ids)
        read += m_entries[id].read ? 1 : 0;
    auto *cup = new QLabel(w);
    cup->setPixmap(Theme::icon(QStringLiteral("mug-hot")).pixmap(Theme::mm(10), Theme::mm(10)));
    cup->setAlignment(Qt::AlignCenter);
    v->addWidget(cup);
    v->addWidget(Ui::centered(m_app->config().flatIssue ? tx("Всё прочитано") : tx("Выпуск прочитан"),
                              Theme::articleFont(m_app->config().fontFamily, 6, true), w));
    v->addWidget(Ui::centered(tx("Просмотрено %1, прочитано %2.").arg(m_ids.size()).arg(read), Theme::smallFont(), w));
    auto *all = Ui::bigButton(QStringLiteral("check-double"), tx("Отметить остальное прочитанным"), false, w);
    connect(all, &QPushButton::clicked, this, [this] { markAllRead(); });
    auto *refresh = Ui::bigButton(QStringLiteral("rotate"), tx("Обновить"), false, w);
    connect(refresh, &QPushButton::clicked, this, [this] { startSync(); });
    for (QPushButton *b : {all, refresh}) {
        b->setMinimumWidth(Theme::mm(80));
        v->addWidget(b, 0, Qt::AlignHCenter);
    }
    return w;
}
