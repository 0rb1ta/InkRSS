#pragma once

#include "HttpTransport.h"
#include "MinifluxSource.h"
#include "Source.h"

#include <QUrl>

// Feeds fetched straight from the sites, without a server: only what the feed itself carries,
// read status kept on the reader.
class DirectSource : public Source {
public:
    explicit DirectSource(HttpTransport &transport, PrepareFn prepare = {}, RemovedFn removed = {}, ReadyFn ready = {});
    void sync(Store &store, Progress progress, std::function<void(QString error)> done) override;
    bool fullText() const override { return false; }

    // Returns the feed's id (an existing one when the URL is already subscribed).
    static qint64 addFeed(Store &store, const QUrl &url, const QString &folder = QString());
    // Returns how many feeds were added.
    static int importOpml(Store &store, const QByteArray &opml);
    // system/inkrss/subscriptions.opml, else the first *.opml in the storage root; empty if none.
    static QString findOpml(const QString &storageRoot);

    static constexpr int kMaxAgeDays = 30;
    static constexpr int kKeepRead = 500;

private:
    HttpTransport &m_transport;
    PrepareFn m_prepare;
    RemovedFn m_removed;
    ReadyFn m_ready;
};
