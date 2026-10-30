#include "ImageCache.h"

#include "Log.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QImageReader>

ImageCache::ImageCache(HttpTransport &transport, const QString &dir, int maxWidthPx)
    : m_transport(transport)
    , m_dir(dir)
    , m_maxWidth(maxWidthPx)
{
}

QString ImageCache::pathFor(const QUrl &url) const
{
    const QByteArray h = QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha1).toHex().left(20);
    return m_dir + QLatin1Char('/') + QString::fromLatin1(h) + QStringLiteral(".png");
}

QString ImageCache::localPath(const QUrl &url) const
{
    const QString p = pathFor(url);
    return QFileInfo::exists(p) ? p : QString();
}

ImageStats ImageCache::takeStats()
{
    const ImageStats s = m_stats;
    m_stats = ImageStats();
    return s;
}

void ImageCache::fetch(const QList<QUrl> &urls, std::function<void()> done)
{
    QDir().mkpath(m_dir);
    QList<QUrl> todo;
    for (const QUrl &u : urls.mid(0, kMaxPerArticle))
        if (localPath(u).isEmpty() && !todo.contains(u))
            todo << u;
    fetchFrom(todo, std::move(done));
}

void ImageCache::fetchFrom(QList<QUrl> urls, std::function<void()> done)
{
    if (urls.isEmpty())
        return done();
    const QUrl url = urls.takeFirst();
    HttpRequest req;
    req.method = "GET";
    req.url = url;
    req.headers << qMakePair(QByteArray("User-Agent"), QByteArray("InkRSS/" APP_VERSION));
    m_transport.send(req, [this, url, urls, done](const HttpResponse &resp) {
        QImage img;
        if (resp.status == 200) {
            // Decoded straight at screen width: a large photo decoded in full first needs tens of
            // megabytes, more than the reader can spare during a sync.
            QBuffer buffer;
            buffer.setData(resp.body);
            QImageReader reader(&buffer);
            const QSize size = reader.size();
            if (size.width() > m_maxWidth)
                reader.setScaledSize(QSize(m_maxWidth, qMax(1, size.height() * m_maxWidth / size.width())));
            img = reader.read();
        }
        if (!img.isNull()) {
            if (img.width() > m_maxWidth) // formats that cannot report their size up front
                img = img.scaledToWidth(m_maxWidth, Qt::SmoothTransformation);
            img = img.convertToFormat(QImage::Format_Grayscale8);
            if (img.save(pathFor(url), "PNG")) {
                ++m_stats.fetched;
                m_stats.bytes += QFileInfo(pathFor(url)).size();
            } else {
                ++m_stats.failed;
                Log::write(QStringLiteral("image: cannot save %1").arg(url.toString()));
            }
        } else {
            ++m_stats.failed;
            Log::write(QStringLiteral("image: skipped %1 (%2)").arg(url.toString()).arg(resp.status));
        }
        fetchFrom(urls, done);
    });
}
