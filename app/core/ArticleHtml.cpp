#include "ArticleHtml.h"

#include "FileIO.h"
#include "ImageCache.h"

#include <QCryptographicHash>
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

QString keepLeadImage(const QString &teaserHtml, const QString &fullHtml)
{
    if (fullHtml.contains(QLatin1String("<img"), Qt::CaseInsensitive))
        return fullHtml;
    static const QRegularExpression figure(QStringLiteral("<figure\\b[^>]*>.*?</figure\\s*>"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto leadFigure = figure.match(teaserHtml);
    if (leadFigure.hasMatch())
        return leadFigure.captured(0) + fullHtml;
    static const QRegularExpression img(QStringLiteral("<img\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    const auto leadImage = img.match(teaserHtml);
    return leadImage.hasMatch() ? leadImage.captured(0) + fullHtml : fullHtml;
}

QString blockLayout(const QString &html)
{
    static const QRegularExpression containers(
        QStringLiteral("<(/?)(?:figure|section|article|header|footer|aside|main|nav|picture)\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression captionOpen(QStringLiteral("<figcaption\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression captionClose(QStringLiteral("</figcaption\\s*>"), QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.replace(containers, QStringLiteral("<\\1div>"));
    out.replace(captionOpen, QStringLiteral("<p class=\"caption\">"));
    out.replace(captionClose, QStringLiteral("</p>"));
    return out;
}

QString articlePath(const QString &articlesDir, qint64 id)
{
    return articlesDir + QStringLiteral("/%1.html").arg(id);
}

QString retryPath(const QString &articlesDir, qint64 id)
{
    return articlePath(articlesDir, id) + QStringLiteral(".missing");
}

QByteArray articleFingerprint(const QString &html)
{
    return QCryptographicHash::hash(html.toUtf8(), QCryptographicHash::Sha1).toHex();
}

int imageAttempts(const QString &path, const QByteArray &fingerprint = {})
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const QList<QByteArray> lines = f.readAll().trimmed().split('\n');
    if (!fingerprint.isEmpty() && lines.value(0) != fingerprint)
        return 0;
    return qMax(0, lines.value(1).toInt());
}

bool localImagesExist(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    static const QRegularExpression img(QStringLiteral("<img src=\"([^\"]*)\""));
    const QString html = QString::fromUtf8(f.readAll());
    auto it = img.globalMatch(html);
    while (it.hasNext()) {
        const QUrl url(unescapeAttr(it.next().captured(1)));
        if (url.isLocalFile() && !QFile::exists(url.toLocalFile()))
            return false;
    }
    return true;
}

bool articleReady(const QString &articlesDir, qint64 id)
{
    const QString page = articlePath(articlesDir, id);
    if (!QFile::exists(page))
        return false;
    const QString retries = retryPath(articlesDir, id);
    if (QFile::exists(retries))
        return imageAttempts(retries) >= kMaxImageAttempts;
    return localImagesExist(page);
}

void removeArticleFiles(const QString &articlesDir, const QList<qint64> &ids)
{
    for (qint64 id : ids) {
        QFile::remove(articlePath(articlesDir, id));
        QFile::remove(retryPath(articlesDir, id));
    }
}

int removeUnusedImages(const QString &articlesDir, const QString &imagesDir)
{
    return removeUnusedImages(QList<QString>{articlesDir}, imagesDir);
}

int removeUnusedImages(const QList<QString> &articlesDirs, const QString &imagesDir)
{
    static const QRegularExpression name(QStringLiteral("/([0-9a-f]+\\.png)\""));
    QSet<QString> used;
    for (const QString &articlesDir : articlesDirs) {
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

QString articleStyleSheet()
{
    return QStringLiteral("body, p, h1, h2, h3, li { color: #000; }"
                          " h1 { font-size: 140%; margin-top: 0; margin-bottom: 0.1em; }"
                          " p { margin-top: 0; margin-bottom: 0.3em; } .meta { color: #333; font-size: 75%; }"
                          " .caption { color: #333; font-size: 80%; }"
                          " a { color: #000; text-decoration: underline; }");
}

int articleImageMaxHeight(int pageHeight)
{
    return pageHeight * 30 / 100;
}

PrepareFn makePrepare(ImageCache &cache, const QString &articlesDir, bool images)
{
    ImageCache *c = &cache;
    return [c, articlesDir, images](const Entry &entry, std::function<void(bool)> done) {
        const CleanArticle clean = cleanHtml(entry.html, QUrl(entry.url));
        const QString page = articlePath(articlesDir, entry.id);
        const QString retries = retryPath(articlesDir, entry.id);
        if (!images) {
            QDir().mkpath(articlesDir);
            const bool saved = writeFileIfChanged(page, withoutImages(clean.html).toUtf8());
            if (saved)
                QFile::remove(retries);
            done(saved);
            return;
        }
        const QByteArray fingerprint = articleFingerprint(entry.html);
        c->fetch(clean.images, [c, clean, articlesDir, page, retries, fingerprint, done] {
            QDir().mkpath(articlesDir);
            bool missing = false;
            for (const QUrl &url : clean.images)
                missing |= c->localPath(url).isEmpty();
            const bool saved = writeFileIfChanged(page, withLocalImages(clean.html, *c).toUtf8());
            bool tracked = true;
            if (saved) {
                if (missing)
                    tracked = writeFileIfChanged(retries, fingerprint + '\n' + QByteArray::number(imageAttempts(retries, fingerprint) + 1));
                else
                    QFile::remove(retries);
            }
            done(saved && tracked);
        });
    };
}
