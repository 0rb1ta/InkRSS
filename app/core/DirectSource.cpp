#include "DirectSource.h"
#include "I18n.h"

#include "FeedParser.h"
#include "Log.h"
#include "Opml.h"
#include "Store.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>

#include <memory>

namespace {

struct DirectRun {
    HttpTransport *transport = nullptr;
    Store *store = nullptr;
    PrepareFn prepare;
    RemovedFn removed;
    ReadyFn ready;
    Source::Progress progress;
    std::function<void(QString)> done;
    QList<Feed> feeds;
    QSet<qint64> prepared;
    bool prepareFailed = false;
    int offline = 0; // feeds that got no answer at all
    QElapsedTimer started;
    QElapsedTimer feedStarted;
};

using Run = std::shared_ptr<DirectRun>;

void feedFrom(const Run &r, int i);

void prepareEntries(const Run &r, QList<Entry> entries, std::function<void()> next)
{
    if (entries.isEmpty() || !r->prepare)
        return next();
    const Entry e = entries.takeFirst();
    r->prepared.insert(e.id);
    r->prepare(e, [r, entries, next](bool saved) {
        r->prepareFailed |= !saved;
        prepareEntries(r, entries, next);
    });
}

void finishRun(const Run &r)
{
    const QList<qint64> removed = r->store->removeReadExcept(DirectSource::kKeepRead);
    if (r->removed && !removed.isEmpty())
        r->removed(removed);
    const bool allOffline = !r->feeds.isEmpty() && r->offline == r->feeds.size();
    if (!allOffline)
        r->store->setMeta(QStringLiteral("lastSync"), QString::number(QDateTime::currentSecsSinceEpoch()));
    if (r->prepareFailed) {
        Log::write(QStringLiteral("sync: failed at prepare: cannot save one or more articles"));
        r->done(tx("Не удалось сохранить статьи на читалку"));
        return;
    }
    Log::write(QStringLiteral("sync: done in %1 ms, feeds=%2 offline=%3, cleanup articles=%4")
                   .arg(r->started.elapsed()).arg(r->feeds.size()).arg(r->offline).arg(removed.size()));
    r->done(allOffline ? tx("Нет связи с интернетом") : QString());
}

void feedFrom(const Run &r, int i)
{
    if (i >= r->feeds.size()) {
        // Articles left without a ready page by a sync that was cut short.
        QList<Entry> missing;
        if (r->ready)
            for (qint64 id : r->store->allIds())
                if (!r->prepared.contains(id) && !r->ready(id))
                    missing << r->store->entry(id);
        return prepareEntries(r, missing, [r] { finishRun(r); });
    }
    const Feed feed = r->feeds[i];
    HttpRequest req;
    req.method = "GET";
    req.url = QUrl(feed.url);
    req.headers << qMakePair(QByteArray("User-Agent"), QByteArray("InkRSS/" APP_VERSION));
    r->feedStarted.restart();
    r->transport->send(req, [r, i, feed](const HttpResponse &resp) {
        QList<Entry> fresh;
        int got = 0;
        if (!resp.networkError.isEmpty() || resp.status == 0) {
            ++r->offline;
            r->store->setFeedError(feed.id, tx("Нет связи"));
        } else if (resp.status >= 400) {
            r->store->setFeedError(feed.id, tx("Ошибка сайта (HTTP %1)").arg(resp.status));
        } else {
            const ParsedFeed parsed = parseFeed(resp.body);
            if (!parsed.error.isEmpty()) {
                r->store->setFeedError(feed.id, parsed.error);
            } else {
                r->store->setFeedError(feed.id, QString());
                if ((feed.title.isEmpty() || feed.title == feed.url) && !parsed.title.isEmpty())
                    r->store->setFeedTitle(feed.id, parsed.title);
                const QDateTime now = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), Qt::UTC);
                const QDateTime oldest = now.addDays(-DirectSource::kMaxAgeDays);
                got = parsed.entries.size();
                for (Entry e : parsed.entries) {
                    if (!e.published.isValid())
                        e.published = now;
                    const qint64 legacyId = e.id;
                    e.id = entryId(feed.url + QLatin1Char('\n') + e.sourceKey);
                    const Entry legacy = r->store->entry(legacyId);
                    const bool legacyMine = legacy.id ? legacy.feedId == feed.id : r->store->known(legacyId);
                    if (e.published < oldest || legacyMine || r->store->known(e.id))
                        continue; // known articles keep their local read status and legacy ids
                    e.feedId = feed.id;
                    e.read = false;
                    fresh << e;
                }
                r->store->upsertEntries(fresh);
            }
        }
        Log::write(QStringLiteral("feed %1: %2 entries=%3 new=%4 (%5 ms)%6")
                       .arg(feed.url).arg(resp.status).arg(got).arg(fresh.size()).arg(r->feedStarted.elapsed())
                       .arg(resp.networkError.isEmpty() ? QString() : QStringLiteral(" ") + resp.networkError));
        prepareEntries(r, fresh, [r, i] {
            if (r->progress)
                r->progress(i + 1, r->feeds.size());
            feedFrom(r, i + 1);
        });
    });
}

} // namespace

DirectSource::DirectSource(HttpTransport &transport, PrepareFn prepare, RemovedFn removed, ReadyFn ready)
    : m_transport(transport)
    , m_prepare(std::move(prepare))
    , m_removed(std::move(removed))
    , m_ready(std::move(ready))
{
}

void DirectSource::sync(Store &store, Progress progress, std::function<void(QString)> done)
{
    // There is no server to tell about read marks.
    store.clearChanges(store.pendingChanges());
    auto r = std::make_shared<DirectRun>();
    r->transport = &m_transport;
    r->store = &store;
    r->prepare = m_prepare;
    r->removed = m_removed;
    r->ready = m_ready;
    r->progress = std::move(progress);
    r->done = std::move(done);
    r->feeds = store.feeds();
    r->started.start();
    r->feedStarted.start();
    feedFrom(r, 0);
}

qint64 DirectSource::addFeed(Store &store, const QUrl &url, const QString &folder)
{
    const QString u = url.toString();
    for (const Feed &f : store.feeds())
        if (f.url == u)
            return f.id;
    Feed f;
    f.url = u;
    f.title = u;
    f.categoryId = folder.isEmpty() ? 0 : store.addCategory(folder);
    return store.addFeed(f);
}

int DirectSource::importOpml(Store &store, const QByteArray &opml)
{
    int added = 0;
    QStringList known;
    for (const Feed &f : store.feeds())
        known << f.url;
    for (const OpmlFeed &o : parseOpml(opml)) {
        if (known.contains(o.url))
            continue;
        Feed f;
        f.url = o.url;
        f.title = o.title;
        f.categoryId = o.folder.isEmpty() ? 0 : store.addCategory(o.folder);
        if (store.addFeed(f)) {
            known << o.url;
            ++added;
        }
    }
    return added;
}

QString DirectSource::findOpml(const QString &storageRoot)
{
    const QString own = storageRoot + QStringLiteral("/system/inkrss/subscriptions.opml");
    if (QFileInfo::exists(own))
        return own;
    const QFileInfoList files = QDir(storageRoot).entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files)
        if (fi.suffix().compare(QLatin1String("opml"), Qt::CaseInsensitive) == 0)
            return fi.filePath();
    return QString();
}
