#include "Issue.h"
#include "I18n.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

QList<IssueItem> buildIssue(const QList<Entry> &entries, const QList<Feed> &feeds, const QList<Category> &categories,
                           IssueLayout layout)
{
    QList<IssueItem> out;
    if (entries.isEmpty())
        return out;
    if (layout == IssueLayout::Flat) {
        for (const Entry &e : entries) {
            IssueItem h;
            h.entryId = e.id;
            out << h;
        }
        return out;
    }
    int lead = 0;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].readingMinutes >= kLeadMinutes) {
            lead = i;
            break;
        }
    IssueItem leadItem;
    leadItem.kind = IssueItem::Lead;
    leadItem.entryId = entries[lead].id;
    out << leadItem;

    QHash<qint64, qint64> folderOf;
    for (const Feed &f : feeds)
        folderOf.insert(f.id, f.categoryId);
    QList<qint64> order;
    QHash<qint64, QString> names;
    for (const Category &c : categories) {
        order << c.id;
        names.insert(c.id, c.title);
    }
    order << 0;
    names.insert(0, tx("Без папки"));

    QHash<qint64, QList<qint64>> bySection;
    for (int i = 0; i < entries.size(); ++i) {
        qint64 folder = folderOf.value(entries[i].feedId, 0);
        if (!names.contains(folder))
            folder = 0;
        bySection[folder] << entries[i].id;
    }
    // One folder (no folders at all, or Miniflux's default "All"): its name says nothing, so the
    // feeds become the sections, in feed title order.
    const bool onlyUnfiled = bySection.size() == 1;
    if (onlyUnfiled) {
        QList<Feed> sorted = feeds;
        std::sort(sorted.begin(), sorted.end(), [](const Feed &a, const Feed &b) {
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
        QHash<qint64, QList<qint64>> byFeed;
        for (const Entry &e : entries)
            byFeed[e.feedId] << e.id;
        const bool oneFeed = byFeed.size() == 1;
        for (const Feed &f : sorted) {
            const QList<qint64> ids = byFeed.value(f.id);
            if (ids.isEmpty())
                continue;
            if (!oneFeed) {
                IssueItem s;
                s.kind = IssueItem::Section;
                s.feedId = f.id;
                s.categoryId = 0;
                s.title = f.title;
                s.count = ids.size();
                out << s;
            }
            for (qint64 id : ids) {
                if (id == leadItem.entryId)
                    continue;
                IssueItem h;
                h.kind = IssueItem::Headline;
                h.entryId = id;
                out << h;
            }
        }
        return out;
    }
    for (qint64 folder : order) {
        const QList<qint64> ids = bySection.value(folder);
        if (ids.isEmpty())
            continue;
        if (!onlyUnfiled) {
            IssueItem s;
            s.kind = IssueItem::Section;
            s.categoryId = folder;
            s.title = names.value(folder);
            s.count = ids.size();
            out << s;
        }
        for (qint64 id : ids) {
            if (id == leadItem.entryId)
                continue;
            IssueItem h;
            h.kind = IssueItem::Headline;
            h.entryId = id;
            out << h;
        }
    }
    return out;
}

QString headlineMeta(const QString &feedTitle, int minutes)
{
    const QString time = minutes > 0 ? tx("%1 мин").arg(minutes) : QString();
    if (feedTitle.isEmpty())
        return time;
    return time.isEmpty() ? feedTitle : feedTitle + QStringLiteral(" · ") + time;
}

QString leadExcerpt(const QString &html, int maxChars)
{
    static const QRegularExpression para(QStringLiteral("<p\\b[^>]*>(.*?)</p\\s*>"),
                                         QRegularExpression::CaseInsensitiveOption
                                             | QRegularExpression::DotMatchesEverythingOption);
    QString text;
    auto it = para.globalMatch(html);
    while (it.hasNext() && text.isEmpty())
        text = plainText(it.next().captured(1));
    if (text.isEmpty())
        text = plainText(html);
    if (text.size() <= maxChars)
        return text;
    int cut = text.lastIndexOf(QLatin1Char(' '), maxChars - 1);
    if (cut <= 0)
        cut = maxChars - 1;
    return text.left(cut).trimmed() + QChar(0x2026);
}

IssueFooter issueFooter(qint64 lastSyncSecs, int unread, int minutes, const QDateTime &now)
{
    IssueFooter f;
    f.right = formatMinutes(minutes);
    if (!lastSyncSecs) {
        f.left = tx("Ещё не обновлялось");
        return f;
    }
    const QDateTime t = QDateTime::fromSecsSinceEpoch(lastSyncSecs, now.timeSpec());
    const QString when = t.date() == now.date() ? t.toString(QStringLiteral("HH:mm"))
                                                : t.toString(QStringLiteral("dd.MM HH:mm"));
    f.left = when + QStringLiteral(" · ") + tx("%1 новых").arg(unread);
    return f;
}

QString formatMinutes(int minutes)
{
    if (minutes <= 0)
        return QString();
    if (minutes < 60)
        return tx("≈ %1 мин").arg(minutes);
    const int h = minutes / 60;
    const int m = minutes % 60;
    return m ? tx("≈ %1 ч %2 мин").arg(h).arg(m) : tx("≈ %1 ч").arg(h);
}

QList<int> pageStarts(const QList<int> &heights, int pageHeight)
{
    QList<int> starts;
    int used = 0;
    for (int i = 0; i < heights.size(); ++i) {
        if (starts.isEmpty() || used + heights[i] > pageHeight) {
            starts << i;
            used = 0;
        }
        used += heights[i];
    }
    return starts;
}

int articlePageCount(int documentHeight, int pageHeight)
{
    if (pageHeight <= 0 || documentHeight <= pageHeight)
        return 1;
    return (documentHeight + pageHeight - 1) / pageHeight;
}

int articlePageTop(int page, int pageHeight)
{
    return page * pageHeight;
}
