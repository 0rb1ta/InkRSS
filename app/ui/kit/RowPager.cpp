#include "RowPager.h"

#include "I18n.h"
#include "Issue.h"
#include "Theme.h"

#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

RowPager::RowPager(QWidget *parent)
    : QWidget(parent)
    , m_empty(tx("Пусто"))
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    m_body = new QWidget(this);
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(Theme::mm(3), Theme::mm(1), Theme::mm(3), 0);
    m_bodyLayout->setSpacing(0);
    v->addWidget(m_body, 1);
    // Page number in the middle whatever the side texts are: three equal columns.
    auto *foot = new QWidget(this);
    foot->setFixedHeight(Theme::mm(7));
    auto *g = new QGridLayout(foot);
    g->setContentsMargins(Theme::mm(3), 0, Theme::mm(3), 0);
    m_footLeft = new QLabel(foot);
    m_footer = new QLabel(foot);
    m_footRight = new QLabel(foot);
    const Qt::Alignment align[] = {Qt::AlignLeft | Qt::AlignVCenter, Qt::AlignCenter, Qt::AlignRight | Qt::AlignVCenter};
    int col = 0;
    for (QLabel *l : {m_footLeft, m_footer, m_footRight}) {
        l->setFont(Theme::smallFont());
        l->setAlignment(align[col]);
        l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        g->addWidget(l, 0, col);
        g->setColumnStretch(col++, 1);
    }
    v->addWidget(foot);
}

void RowPager::setRows(const QList<Row> &rows, bool keepPage)
{
    m_rows = rows;
    if (!keepPage)
        m_page = 0;
    m_laidWidth = -1;
    layoutPages();
}

int RowPager::pageCount() const
{
    return m_starts.size() + (m_end && !m_rows.isEmpty() ? 1 : 0);
}

void RowPager::setFooter(const QString &left, const QString &right)
{
    m_footLeft->setText(left);
    m_footRight->setText(right);
}

void RowPager::flip(int direction)
{
    const int next = m_page + direction;
    if (next < 0 || next >= pageCount())
        return;
    m_page = next;
    showPage();
}

void RowPager::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutPages();
}

void RowPager::layoutPages()
{
    const QMargins m = m_bodyLayout->contentsMargins();
    const int w = m_body->width() - m.left() - m.right();
    const int h = m_body->height() - m.top() - m.bottom();
    if (w <= 0 || h <= 0)
        return;
    if (w == m_laidWidth && h == m_laidHeight && m_laidWidth >= 0) {
        showPage();
        return;
    }
    m_laidWidth = w;
    m_laidHeight = h;
    QList<int> heights;
    heights.reserve(m_rows.size());
    for (const Row &r : m_rows)
        heights << r.height(w);
    m_starts = pageStarts(heights, h);
    if (m_page >= pageCount())
        m_page = qMax(0, pageCount() - 1);
    showPage();
}

void RowPager::showPage()
{
    while (QLayoutItem *item = m_bodyLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater(); // never delete inside the event that triggered the change
        }
        delete item;
    }
    if (m_rows.isEmpty()) {
        auto *l = new QLabel(m_empty, m_body);
        l->setFont(Theme::bodyFont());
        l->setAlignment(Qt::AlignCenter);
        l->setWordWrap(true);
        m_bodyLayout->addWidget(l, 1);
        m_footer->clear();
        return;
    }
    if (m_page >= m_starts.size() && m_end) {
        m_bodyLayout->addWidget(m_end(m_body), 1);
    } else if (m_page < m_starts.size()) {
        const int from = m_starts[m_page];
        const int to = m_page + 1 < m_starts.size() ? m_starts[m_page + 1] : m_rows.size();
        for (int i = from; i < to; ++i) {
            QWidget *w = m_rows[i].make(m_body);
            w->setFixedHeight(m_rows[i].height(m_laidWidth));
            m_bodyLayout->addWidget(w);
        }
        m_bodyLayout->addStretch(1);
    }
    m_footer->setText(pageCount() > 1 ? QStringLiteral("— %1 / %2 —").arg(m_page + 1).arg(pageCount()) : QString());
}

TapRow::TapRow(std::function<void()> onClick, QWidget *parent)
    : QWidget(parent)
    , m_onClick(std::move(onClick))
{
}

void TapRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (rect().contains(event->pos()) && m_onClick)
        m_onClick();
}

void TapRow::paintEvent(QPaintEvent *)
{
    // A thin dotted rule under every row, as in a newspaper's list of headlines.
    QPainter p(this);
    p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1, Qt::DotLine));
    p.drawLine(0, height() - 1, width(), height() - 1);
}
