#include "FeedParser.h"
#include "I18n.h"

#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtEndian>

#include <cctype>

namespace {

int zoneOffsetMinutes(const QString &zone, bool *ok)
{
    *ok = true;
    if (zone.isEmpty() || zone == QLatin1String("GMT") || zone == QLatin1String("UT")
        || zone == QLatin1String("UTC") || zone == QLatin1String("Z"))
        return 0;
    if (zone.size() == 5 && (zone[0] == QLatin1Char('+') || zone[0] == QLatin1Char('-'))) {
        const int hh = zone.midRef(1, 2).toInt();
        const int mm = zone.midRef(3, 2).toInt();
        return (zone[0] == QLatin1Char('-') ? -1 : 1) * (hh * 60 + mm);
    }
    static const QList<QPair<QString, int>> named{
        {"EST", -5}, {"EDT", -4}, {"CST", -6}, {"CDT", -5}, {"MST", -7},
        {"MDT", -6}, {"PST", -8}, {"PDT", -7}, {"MSK", 3}, {"CET", 1}, {"CEST", 2}};
    for (const auto &z : named)
        if (z.first == zone)
            return z.second * 60;
    *ok = false;
    return 0;
}

QString text(QXmlStreamReader &r)
{
    return r.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

// The feed's own vocabulary: RSS 2.0 (no namespace), RSS 1.0 and Atom. Other namespaces such as
// Media RSS reuse names like title and content for something else.
bool isCore(const QStringRef &ns)
{
    return ns.isEmpty() || ns == QLatin1String("http://purl.org/rss/1.0/") || ns == QLatin1String("http://www.w3.org/2005/Atom");
}

// HTML entities are not XML: one &nbsp; makes a strict parser reject the whole feed. The common
// ones become character references first (inside CDATA that leaves HTML that means the same).
QByteArray declareHtmlEntities(const QByteArray &xml)
{
    static const QHash<QByteArray, int> named{
        {"nbsp", 160}, {"mdash", 8212}, {"ndash", 8211}, {"hellip", 8230}, {"laquo", 171}, {"raquo", 187},
        {"copy", 169}, {"reg", 174}, {"trade", 8482}, {"lsquo", 8216}, {"rsquo", 8217}, {"ldquo", 8220},
        {"rdquo", 8221}, {"bdquo", 8222}, {"bull", 8226}, {"middot", 183}, {"times", 215}, {"euro", 8364},
        {"deg", 176}, {"shy", 173}, {"thinsp", 8201}, {"ensp", 8194}, {"emsp", 8195}, {"zwnj", 8204},
        {"zwj", 8205}, {"minus", 8722}, {"iexcl", 161}, {"para", 182}, {"sect", 167}};
    if (!xml.contains('&'))
        return xml;
    QByteArray out;
    out.reserve(xml.size());
    int i = 0;
    while (i < xml.size()) {
        const char c = xml[i];
        if (c == '&') {
            int j = i + 1;
            while (j < xml.size() && j - i <= 10 && std::isalnum(static_cast<unsigned char>(xml[j])))
                ++j;
            if (j < xml.size() && xml[j] == ';' && j > i + 1) {
                const QByteArray name = xml.mid(i + 1, j - i - 1);
                const auto it = named.constFind(name);
                if (it != named.constEnd()) {
                    out += "&#" + QByteArray::number(it.value()) + ';';
                    i = j + 1;
                    continue;
                }
            }
        }
        out += c;
        ++i;
    }
    return out;
}

} // namespace

QDateTime parseFeedDate(const QString &input)
{
    const QString s = input.trimmed();
    static const QRegularExpression iso(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}"));
    if (iso.match(s).hasMatch()) {
        QString t = s;
        static const QRegularExpression fraction(QStringLiteral("\\.\\d+"));
        t.remove(fraction);
        const QDateTime d = QDateTime::fromString(t, Qt::ISODate);
        return d.isValid() ? QDateTime::fromSecsSinceEpoch(d.toSecsSinceEpoch(), Qt::UTC) : QDateTime();
    }
    static const QRegularExpression rfc(QStringLiteral(
        "^(?:[A-Za-z]+,?\\s*)?(\\d{1,2})\\s+([A-Za-z]{3})[A-Za-z]*\\.?\\s+(\\d{2,4})\\s+(\\d{1,2}):(\\d{2})(?::(\\d{2}))?\\s*([+-]\\d{4}|[A-Za-z]{1,5})?"));
    const auto m = rfc.match(s);
    if (!m.hasMatch())
        return QDateTime();
    static const QStringList months{"jan", "feb", "mar", "apr", "may", "jun",
                                    "jul", "aug", "sep", "oct", "nov", "dec"};
    const int month = months.indexOf(m.captured(2).toLower()) + 1;
    int year = m.captured(3).toInt();
    if (year < 100)
        year += 2000;
    bool ok = false;
    const int offset = zoneOffsetMinutes(m.captured(7).toUpper(), &ok);
    const QDate date(year, month, m.captured(1).toInt());
    const QTime time(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt());
    if (month == 0 || !date.isValid() || !time.isValid() || !ok)
        return QDateTime();
    return QDateTime(date, time, Qt::UTC).addSecs(-60 * offset);
}

qint64 entryId(const QString &key)
{
    const QByteArray h = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1);
    const qint64 v = qint64(qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(h.constData()))
                            & Q_UINT64_C(0x7fffffffffffffff));
    return v ? v : 1;
}

ParsedFeed parseFeed(const QByteArray &xml)
{
    ParsedFeed out;
    QXmlStreamReader r(declareHtmlEntities(xml));
    if (!r.readNextStartElement()) {
        out.error = tx("Пустой ответ или не XML");
        return out;
    }
    const QStringRef root = r.name();
    const bool atom = root == QLatin1String("feed");
    if (!atom && root != QLatin1String("rss") && root != QLatin1String("RDF")) {
        out.error = tx("По адресу не RSS-лента");
        return out;
    }

    bool inItem = false;
    Entry cur;
    QString key, summary, content, date, updated;
    auto finishItem = [&] {
        cur.html = content.isEmpty() ? summary : content;
        if (cur.title.isEmpty())
            cur.title = plainText(cur.html).left(120);
        cur.readingMinutes = readingMinutes(cur.html);
        cur.published = parseFeedDate(date.isEmpty() ? updated : date);
        if (key.isEmpty())
            key = !cur.url.isEmpty() ? cur.url : cur.title + date;
        cur.sourceKey = key;
        cur.id = entryId(key);
        out.entries << cur;
    };

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement()) {
            const QStringRef n = r.name();
            if (inItem && (n == QLatin1String("item") || n == QLatin1String("entry"))) {
                finishItem();
                inItem = false;
            }
            continue;
        }
        if (!r.isStartElement())
            continue;
        const QStringRef n = r.name();
        if (!inItem) {
            if (n == QLatin1String("item") || n == QLatin1String("entry")) {
                inItem = true;
                cur = Entry();
                key.clear();
                summary.clear();
                content.clear();
                date.clear();
                updated.clear();
            } else if (n == QLatin1String("title") && isCore(r.namespaceUri()) && out.title.isEmpty()) {
                out.title = text(r);
            }
            continue;
        }
        const QStringRef ns = r.namespaceUri();
        const bool core = isCore(ns);
        const bool dc = ns == QLatin1String("http://purl.org/dc/elements/1.1/");
        if (!core && !dc && n != QLatin1String("encoded")) {
            continue; // media:*, itunes:*, … carry their own titles and texts
        } else if (dc) {
            if (n == QLatin1String("creator"))
                cur.author = text(r).simplified();
            else if (n == QLatin1String("date"))
                date = text(r);
        } else if (n == QLatin1String("title")) {
            cur.title = text(r).simplified();
        } else if (n == QLatin1String("link")) {
            if (atom || r.attributes().hasAttribute(QLatin1String("href"))) {
                const QStringRef rel = r.attributes().value(QLatin1String("rel"));
                if (rel.isEmpty() || rel == QLatin1String("alternate"))
                    cur.url = r.attributes().value(QLatin1String("href")).toString();
            } else {
                cur.url = text(r);
            }
        } else if (n == QLatin1String("guid") || n == QLatin1String("id")) {
            key = text(r);
        } else if (n == QLatin1String("encoded") && ns.contains(QLatin1String("purl.org/rss/1.0/modules/content"))) {
            content = text(r);
        } else if (n == QLatin1String("content")) {
            content = text(r);
        } else if (n == QLatin1String("description") || n == QLatin1String("summary")) {
            summary = text(r);
        } else if (n == QLatin1String("pubDate") || n == QLatin1String("published") || n == QLatin1String("date")) {
            date = text(r);
        } else if (n == QLatin1String("updated")) {
            updated = text(r);
        } else if (n == QLatin1String("creator") || n == QLatin1String("author")) {
            cur.author = text(r).simplified();
        }
    }
    if (r.hasError()) {
        out.error = tx("Лента повреждена: %1").arg(r.errorString());
        out.entries.clear();
    }
    return out;
}
