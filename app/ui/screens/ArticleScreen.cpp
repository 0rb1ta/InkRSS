#include "ArticleScreen.h"
#include "I18n.h"

#include "ArticleHtml.h"
#include "Device.h"
#include "Log.h"
#include "Store.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QImageReader>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QTextDocument>

namespace {

QString dateText(const QDateTime &t)
{
    const QLocale locale(I18n::english() ? QLocale::English : QLocale::Russian);
    return t.isValid() ? locale.toString(t.toLocalTime(), QStringLiteral("d MMMM, HH:mm")) : QString();
}

} // namespace

ArticleScreen::ArticleScreen(AppController *app, qint64 id, QWidget *parent)
    : Screen(parent)
    , m_app(app)
    , m_doc(new QTextDocument(this))
{
    setObjectName(QStringLiteral("article"));
    m_entry = app->store().entry(id);
    if (!m_entry.read)
        app->store().setRead(id, true);
    for (const Feed &fd : app->store().feeds())
        if (fd.id == m_entry.feedId)
            m_feed = fd.title;
    m_bar = new TopBar(m_feed, true, this);
    m_bar->setTitleFont(Theme::bodyFont());
    connect(m_bar, &TopBar::back, this, [this] { m_app->back(); });
    m_menu = m_bar->addIconButton(QStringLiteral("bars"));
    connect(m_menu, &QPushButton::clicked, this, [this] { showMenu(); });
}

QString ArticleScreen::describe() const
{
    const Config &c = m_app->config();
    return QStringLiteral("id=%1 chars=%2 images=%3 pages=%4 page=%5 font=%6 pt=%7")
        .arg(m_entry.id).arg(textLength(m_entry.html)).arg(m_images).arg(m_doc->pageCount()).arg(m_page + 1)
        .arg(c.fontFamily.isEmpty() ? QStringLiteral("default") : c.fontFamily)
        .arg(c.fontPt > 0 ? c.fontPt : Theme::kDefaultArticlePt);
}

ArticleScreen::~ArticleScreen() = default;

QRect ArticleScreen::pageRect() const
{
    const int margin = Theme::mm(6);
    return rect().adjusted(margin, m_bar->height() + Theme::mm(4), -margin, -Theme::mm(8));
}

void ArticleScreen::layoutDocument()
{
    const Config &c = m_app->config();
    const int pt = c.fontPt > 0 ? c.fontPt : Theme::kDefaultArticlePt;
    // Printed points look small at arm's length on e-ink.
    const QFont font = Theme::articleFont(c.fontFamily, pt * 0.3528 * 1.25);
    m_doc->setDefaultFont(font);
    m_doc->setDocumentMargin(0);
    m_doc->setDefaultStyleSheet(articleStyleSheet());
    QString body;
    QFile f(articlePath(m_app->articlesDir(), m_entry.id));
    if (f.open(QIODevice::ReadOnly))
        body = QString::fromUtf8(f.readAll());
    else
        body = cleanHtml(m_entry.html, QUrl(m_entry.url)).html; // not prepared yet: text without images
    body = blockLayout(body);
    if (!c.images) {
        body = withoutImages(body);
    } else {
        body = fitImages(body, pageRect().width(), articleImageMaxHeight(pageRect().height()), [](const QString &src) {
            return QImageReader(QUrl(src).toLocalFile()).size();
        });
    }
    m_images = body.count(QLatin1String("<img"), Qt::CaseInsensitive);
    QStringList meta;
    const QString minutes = m_entry.readingMinutes > 0 ? tx("%1 мин").arg(m_entry.readingMinutes) : QString();
    for (const QString &part : {m_feed, m_entry.author, dateText(m_entry.published), minutes})
        if (!part.isEmpty())
            meta << part.toHtmlEscaped();
    m_doc->setHtml(QStringLiteral("<h1>%1</h1><p class=\"meta\">%2</p>%3")
                       .arg(m_entry.title.toHtmlEscaped(), meta.join(QStringLiteral(" · ")), body));
    // A page size makes the layout move lines that would be cut to the next page.
    m_doc->setPageSize(QSizeF(pageRect().width(), pageRect().height()));
    if (m_page >= m_doc->pageCount())
        m_page = qMax(0, m_doc->pageCount() - 1);
}

void ArticleScreen::resizeEvent(QResizeEvent *event)
{
    Screen::resizeEvent(event);
    m_bar->setGeometry(0, 0, width(), m_bar->sizeHint().height());
    layoutDocument();
}

void ArticleScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    const QRect r = pageRect();
    const qreal top = m_page * m_doc->pageSize().height();
    p.save();
    p.translate(r.left(), r.top() - top);
    m_doc->drawContents(&p, QRectF(0, top, r.width(), r.height()));
    p.restore();
    p.setFont(Theme::smallFont());
    const QRect foot(r.left(), r.bottom() + Theme::mm(2), r.width(), Theme::mm(5));
    p.drawText(foot, Qt::AlignCenter, QStringLiteral("%1 / %2").arg(m_page + 1).arg(m_doc->pageCount()));
}

void ArticleScreen::pageKey(int direction)
{
    const int next = m_page + direction;
    if (next < 0 || next >= m_doc->pageCount())
        return;
    m_page = next;
    Log::write(QStringLiteral("page: article %1/%2").arg(m_page + 1).arg(m_doc->pageCount()));
    update();
}

void ArticleScreen::mouseReleaseEvent(QMouseEvent *event)
{
    // Pages turn with the keys and swipes only; a tap does nothing except on a link, whose
    // address cannot open here: it is shown as a QR code for the phone.
    const QRect r = pageRect();
    if (!r.contains(event->pos()))
        return;
    const QPointF docPos(event->pos().x() - r.left(), event->pos().y() - r.top() + m_page * m_doc->pageSize().height());
    const QString href = m_doc->documentLayout()->anchorAt(docPos);
    if (!href.isEmpty()) {
        Log::write(QStringLiteral("tap: link"));
        Ui::showQr(this, tx("Ссылка"), QString(), href, tx("Наведите камеру телефона"));
    }
}

void ArticleScreen::showMenu()
{
    const Entry now = m_app->store().entry(m_entry.id);
    const Config c = m_app->config();
    // Every change redraws the article behind the menu at once.
    auto apply = [this](std::function<void(Config &)> change) {
        Config next = m_app->config();
        change(next);
        m_app->updateConfig(next);
        layoutDocument();
        update();
        Log::write(QStringLiteral("screen: article %1").arg(describe()));
    };
    Ui::FontPanel font;
    font.family = c.fontFamily;
    font.fontChanged = [apply](const QString &family) { apply([family](Config &n) { n.fontFamily = family; }); };
    font.sizes = {{tx("Текст"), c.fontPt > 0 ? c.fontPt : Theme::kDefaultArticlePt, 8, 18,
                   [apply](int pt) { apply([pt](Config &n) { n.fontPt = pt; }); }}};
    QList<Ui::MenuItem> actions{
        {now.starred ? QStringLiteral("star") : QStringLiteral("regular:star"),
         now.starred ? tx("Убрать из избранного") : tx("В избранное")},
        {QStringLiteral("regular:envelope"), tx("Оставить непрочитанной")},
        {QStringLiteral("font"), tx("Шрифт и размер")},
        {QStringLiteral("qrcode"), tx("QR-код ссылки")}};
    if (m_app->fullText())
        actions << Ui::MenuItem{QStringLiteral("download"), tx("Скачать полный текст")};
    const int choice = Ui::cornerMenu(m_menu, actions, &font, 2);
    switch (choice) {
    case 0:
        m_app->store().setStarred(m_entry.id, !now.starred);
        Ui::toast(this, now.starred ? tx("Убрано из избранного") : tx("В избранном"));
        break;
    case 1:
        m_app->store().setRead(m_entry.id, false);
        m_app->back();
        break;
    case 3:
        if (!m_entry.url.isEmpty())
            Ui::showQr(this, m_entry.title, QString(), m_entry.url, tx("Наведите камеру телефона"));
        break;
    case 4: {
        QPointer<ArticleScreen> self(this);
        Ui::toast(this, tx("Загружаем полный текст…"));
        m_app->fetchFullText(m_entry.id, [self](QString error) {
            if (!self)
                return;
            if (!error.isEmpty()) {
                Ui::toast(self, error);
                return;
            }
            self->m_entry = self->m_app->store().entry(self->m_entry.id);
            self->layoutDocument();
            self->update();
            Ui::toast(self, tx("Полный текст загружен"));
        });
        break;
    }
    default:
        break;
    }
}
