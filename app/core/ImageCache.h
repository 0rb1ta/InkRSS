#pragma once

#include "HttpTransport.h"

#include <QUrl>

#include <functional>

// Article images kept on the reader: scaled down to the screen width and stored in greys, which
// is all an e-ink screen shows and much smaller than the originals.
struct ImageStats {
    int fetched = 0;
    int failed = 0;
    qint64 bytes = 0; // as saved on the reader
};

class ImageCache {
public:
    ImageCache(HttpTransport &transport, const QString &dir, int maxWidthPx);
    // Downloads the images not yet cached (at most kMaxPerArticle); a failed image is skipped.
    void fetch(const QList<QUrl> &urls, std::function<void()> done);
    QString localPath(const QUrl &url) const; // empty when not cached
    // Counts since the last call (for the sync log); resets them.
    ImageStats takeStats();

    static constexpr int kMaxPerArticle = 30;

private:
    QString pathFor(const QUrl &url) const;
    void fetchFrom(QList<QUrl> urls, std::function<void()> done);

    HttpTransport &m_transport;
    QString m_dir;
    int m_maxWidth;
    ImageStats m_stats;
};
