#include "ArticleHtml.h"

#include "FileIO.h"
#include "ImageCache.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

namespace {

const int kMaxInlineImage = 200 * 1024;

QString unescapeAttr(QString v)
{
    v.replace(QLatin1String("&quot;"), QLatin1String("\""));
    v.replace(QLatin1String("&#39;"), QLatin1String("'"));
    v.replace(QLatin1String("&lt;"), QLatin1String("<"));
    v.replace(QLatin1String("&gt;"), QLatin1String(">"));
    v.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return v;
}

QString escapeAttr(const QString &v)
{
    return v.toHtmlEscaped();
}

QMap<QString, QString> attributes(const QString &text)
{
    static const QRegularExpression attr(
        QStringLiteral("([a-zA-Z_:][-a-zA-Z0-9_:.]*)\\s*(?:=\\s*(\"[^\"]*\"|'[^']*'|[^\\s\"'>]+))?"));
    QMap<QString, QString> out;
    auto it = attr.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        QString v = m.captured(2);
        if (v.size() >= 2 && (v.startsWith(QLatin1Char('"')) || v.startsWith(QLatin1Char('\''))))
            v = v.mid(1, v.size() - 2);
        out.insert(m.captured(1).toLower(), unescapeAttr(v));
    }
    return out;
}

bool usableRemote(const QUrl &u)
{
    return u.isValid() && (u.scheme() == QLatin1String("http") || u.scheme() == QLatin1String("https"));
}

} // namespace

CleanArticle cleanHtml(const QString &input, const QUrl &base)
{
    CleanArticle out;
    QString s = input;
    static const QRegularExpression blocks(
        QStringLiteral("<(script|style|iframe|form|noscript|object|embed|svg|button|select|textarea|video|audio|template)\\b"
                       "[^>]*>.*?</\\1\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression strays(
        QStringLiteral("</?(script|style|iframe|form|noscript|object|embed|svg|button|select|textarea|video|audio|template|input|link|meta|source)\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression comments(QStringLiteral("<!--.*?-->"),
                                             QRegularExpression::DotMatchesEverythingOption);
    s.remove(comments);
    s.remove(blocks);
    s.remove(strays);

    static const QRegularExpression tag(QStringLiteral("<([a-zA-Z][a-zA-Z0-9]*)(\\s[^>]*)?>"));
    QString result;
    result.reserve(s.size());
    int pos = 0;
    auto it = tag.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        result += s.midRef(pos, m.capturedStart() - pos);
        pos = m.capturedEnd();
        const QString name = m.captured(1).toLower();
        const QMap<QString, QString> a = attributes(m.captured(2));
        if (name == QLatin1String("img")) {
            QString src;
            for (const char *key : {"data-src", "data-original", "data-lazy-src", "src"}) {
                const QString v = a.value(QLatin1String(key)).trimmed();
                if (!v.isEmpty() && !(v.startsWith(QLatin1String("data:")) && QLatin1String(key) != QLatin1String("src"))) {
                    src = v;
                    break;
                }
            }
            if (src.isEmpty())
                continue;
            if (src.startsWith(QLatin1String("data:"))) {
                if (src.size() > kMaxInlineImage)
                    continue;
                result += QStringLiteral("<img src=\"%1\">").arg(escapeAttr(src));
                continue;
            }
            const QUrl u = base.resolved(QUrl(src));
            if (!usableRemote(u))
                continue;
            out.images << u;
            QString alt = a.value(QStringLiteral("alt"));
            result += QStringLiteral("<img src=\"%1\"%2>")
                          .arg(escapeAttr(u.toString()),
                               alt.isEmpty() ? QString() : QStringLiteral(" alt=\"%1\"").arg(escapeAttr(alt)));
            continue;
        }
        QString kept;
        if (name == QLatin1String("a") && a.contains(QStringLiteral("href"))) {
            const QUrl u = base.resolved(QUrl(a.value(QStringLiteral("href")).trimmed()));
            if (usableRemote(u))
                kept += QStringLiteral(" href=\"%1\"").arg(escapeAttr(u.toString()));
        }
        for (const char *key : {"colspan", "rowspan"})
            if (a.contains(QLatin1String(key)))
                kept += QStringLiteral(" %1=\"%2\"").arg(QLatin1String(key), escapeAttr(a.value(QLatin1String(key))));
        result += QLatin1Char('<') + name + kept + QLatin1Char('>');
    }
    result += s.midRef(pos);
    out.html = result;
    return out;
}

QString withLocalImages(const QString &html, const ImageCache &cache)
{
    static const QRegularExpression img(QStringLiteral("<img src=\"([^\"]*)\"([^>]*)>"));
    QString out;
    int pos = 0;
    auto it = img.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        out += html.midRef(pos, m.capturedStart() - pos);
        pos = m.capturedEnd();
        const QString src = unescapeAttr(m.captured(1));
        if (src.startsWith(QLatin1String("data:"))) {
            out += m.captured(0);
            continue;
        }
        const QString local = cache.localPath(QUrl(src));
        if (!local.isEmpty())
            out += QStringLiteral("<img src=\"%1\"%2>").arg(escapeAttr(QUrl::fromLocalFile(local).toString()), m.captured(2));
    }
    out += html.midRef(pos);
    return out;
}

QString fitImages(const QString &html, int maxWidth, int maxHeight,
                  const std::function<QSize(const QString &)> &sizeOf)
{
    static const QRegularExpression img(QStringLiteral("<img src=\"([^\"]*)\"([^>]*)>"));
    QString out;
    int pos = 0;
    auto it = img.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        out += html.midRef(pos, m.capturedStart() - pos);
        pos = m.capturedEnd();
        const QSize size = sizeOf(unescapeAttr(m.captured(1)));
        if (!size.isValid() || size.isEmpty()) {
            out += m.captured(0);
            continue;
        }
        const QSize fitted = size.scaled(qMin(size.width(), maxWidth), qMin(size.height(), maxHeight), Qt::KeepAspectRatio);
        out += QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%3\"%4>")
                   .arg(m.captured(1)).arg(fitted.width()).arg(fitted.height()).arg(m.captured(2));
    }
    out += html.midRef(pos);
    return out;
}

QString articlePath(const QString &articlesDir, qint64 id)
{
    return articlesDir + QStringLiteral("/%1.html").arg(id);
}

void removeArticleFiles(const QString &articlesDir, const QList<qint64> &ids)
{
    for (qint64 id : ids)
        QFile::remove(articlePath(articlesDir, id));
}

int removeUnusedImages(const QString &articlesDir, const QString &imagesDir)
{
    static const QRegularExpression name(QStringLiteral("/([0-9a-f]+\\.png)\""));
    QSet<QString> used;
    const QDir articles(articlesDir);
    for (const QString &file : articles.entryList({QStringLiteral("*.html")}, QDir::Files)) {
        QFile f(articles.filePath(file));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString html = QString::fromUtf8(f.readAll());
        auto it = name.globalMatch(html);
        while (it.hasNext())
            used.insert(it.next().captured(1));
    }
    int removed = 0;
    const QDir images(imagesDir);
    for (const QString &file : images.entryList(QDir::Files))
        if (!used.contains(file) && QFile::remove(images.filePath(file)))
            ++removed;
    return removed;
}

QString withoutImages(const QString &html)
{
    static const QRegularExpression img(QStringLiteral("<img\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.remove(img);
    return out;
}

PrepareFn makePrepare(ImageCache &cache, const QString &articlesDir, bool images)
{
    ImageCache *c = &cache;
    return [c, articlesDir, images](const Entry &entry, std::function<void()> done) {
        const CleanArticle clean = cleanHtml(entry.html, QUrl(entry.url));
        if (!images) {
            QDir().mkpath(articlesDir);
            writeFileIfChanged(articlePath(articlesDir, entry.id), withoutImages(clean.html).toUtf8());
            done();
            return;
        }
        c->fetch(clean.images, [c, clean, articlesDir, id = entry.id, done] {
            QDir().mkpath(articlesDir);
            writeFileIfChanged(articlePath(articlesDir, id), withLocalImages(clean.html, *c).toUtf8());
            done();
        });
    };
}
