#include "MinifluxSource.h"

#include "Log.h"
#include "Store.h"

#include <QElapsedTimer>
#include <QSet>

#include <memory>

namespace {

struct SyncRun {
    MinifluxApi *api = nullptr;
    Store *store = nullptr;
    int limit = 0;
    PrepareFn prepare;
    RemovedFn removed;
    ReadyFn ready;
    Source::Progress progress;
    std::function<void(QString)> done;

    // Status changes are sent as a list of steps; each step reports which queued changes it delivered.
    QList<std::function<void(std::function<void(ApiFail)>)>> sends;
    QList<QList<StatusChange>> sendChanges;
    QList<Entry> entries;
    QList<qint64> toPrepare;
    int total = 0;
    int fullTextAsked = 0;
    int fullTextOk = 0;
    int fullTextFailed = 0;
    int fullTextSkipped = 0; // failed in an earlier sync, or over the per-sync cap
    QString sendCounts;
    QString step = QStringLiteral("send"); // for the failure line in the log
    QElapsedTimer started;
    QElapsedTimer stepStarted;
};

// Milliseconds since the step began; starts the next one.
qint64 lap(SyncRun &r)
{
    const qint64 ms = r.stepStarted.elapsed();
    r.stepStarted.restart();
    return ms;
}

using Run = std::shared_ptr<SyncRun>;

void finish(const Run &r, ApiFail fail)
{
    if (fail == ApiFail::None)
        Log::write(QStringLiteral("sync: done in %1 ms").arg(r->started.elapsed()));
    else
        Log::write(QStringLiteral("sync: failed at %1: %2").arg(r->step, failText(fail)));
    auto done = r->done;
    done(failText(fail));
}

void prepareFrom(const Run &r, int i);
void prepareChanged(const Run &r)
{
    // Only new or changed articles are prepared again (cleaned, images fetched, written).
    r->toPrepare.clear();
    for (const Entry &e : r->entries) {
        const Entry stored = r->store->entry(e.id);
        if (stored.id == 0 || stored.html != e.html || (r->ready && !r->ready(e.id)))
            r->toPrepare << e.id;
    }
    r->store->upsertEntries(r->entries);
    r->step = QStringLiteral("prepare");
    lap(*r);
    if (r->progress)
        r->progress(0, r->toPrepare.size());
    return prepareFrom(r, 0);
}

// Starred on the phone but already read: not among the unread, still wanted under "Starred".
// A failure here only leaves that list as it was.
void fetchStarred(const Run &r)
{
    r->api->starred(MinifluxSource::kStarredLimit, [r](ApiFail fail, QList<Entry> starred) {
        if (fail == ApiFail::None) {
            QSet<qint64> have;
            for (const Entry &e : r->entries)
                have.insert(e.id);
            for (const Entry &e : starred)
                if (!have.contains(e.id))
                    r->entries << e;
        }
        prepareChanged(r);
    });
}

void fetchFrom(const Run &r, int i);

void complete(const Run &r)
{
    Log::write(QStringLiteral("sync: prepare %1 articles (%2 ms)").arg(r->toPrepare.size()).arg(lap(*r)));
    if (r->total <= r->limit) {
        // Everything unread on the server came down, so what is unread here but missing there
        // was read somewhere else.
        QSet<qint64> fresh;
        for (const Entry &e : r->entries)
            fresh.insert(e.id);
        QList<qint64> readElsewhere;
        for (qint64 id : r->store->unreadIds())
            if (!fresh.contains(id))
                readElsewhere << id;
        r->store->setReadFromSource(readElsewhere);
    }
    r->store->setMeta(QStringLiteral("lastSync"), QString::number(QDateTime::currentSecsSinceEpoch()));
    const QList<qint64> removed = r->store->removeReadExcept(MinifluxSource::kKeepRead);
    if (r->removed && !removed.isEmpty())
        r->removed(removed);
    Log::write(QStringLiteral("sync: cleanup articles=%1").arg(removed.size()));
    finish(r, ApiFail::None);
}

void prepareFrom(const Run &r, int i)
{
    if (i >= r->toPrepare.size())
        return complete(r);
    auto next = [r, i] {
        if (r->progress)
            r->progress(i + 1, r->toPrepare.size());
        prepareFrom(r, i + 1);
    };
    if (!r->prepare)
        return next();
    r->prepare(r->store->entry(r->toPrepare[i]), next);
}

// Whether the article still needs the server to extract its full text. A full text kept from an
// earlier sync is reused: the server may hand out the teaser again, or fail to extract this time.
bool needsFullText(const Run &r, Entry &e)
{
    if (textLength(e.html) >= MinifluxSource::kTeaserLength)
        return false;
    const Entry stored = r->store->entry(e.id);
    if (textLength(stored.html) > textLength(e.html)) {
        e.html = stored.html;
        e.readingMinutes = stored.readingMinutes;
        return false;
    }
    if (r->fullTextAsked >= MinifluxSource::kMaxFullTextFetches || r->store->noFullText(e.id)) {
        ++r->fullTextSkipped;
        return false;
    }
    return true;
}

void fetchFrom(const Run &r, int i)
{
    while (i < r->entries.size() && !needsFullText(r, r->entries[i]))
        ++i;
    if (i >= r->entries.size()) {
        Log::write(QStringLiteral("sync: fulltext asked=%1 ok=%2 failed=%3 skipped=%4 (%5 ms)")
                       .arg(r->fullTextAsked).arg(r->fullTextOk).arg(r->fullTextFailed).arg(r->fullTextSkipped)
                       .arg(lap(*r)));
        r->step = QStringLiteral("starred");
        return fetchStarred(r);
    }
    ++r->fullTextAsked;
    r->api->fetchContent(r->entries[i].id, [r, i](ApiFail fail, QString html) {
        // A page the server could not extract keeps its teaser; it is not worth failing the sync.
        if (fail == ApiFail::None && textLength(html) > textLength(r->entries[i].html)) {
            r->entries[i].html = html;
            r->entries[i].readingMinutes = readingMinutes(html);
            ++r->fullTextOk;
        } else if (fail != ApiFail::None) {
            ++r->fullTextFailed;
            Log::write(QStringLiteral("miniflux: no full text for %1").arg(r->entries[i].id));
            r->store->setNoFullText(r->entries[i].id);
        }
        fetchFrom(r, i + 1);
    });
}

void download(const Run &r)
{
    Log::write(QStringLiteral("sync: send %1 (%2 ms)").arg(r->sendCounts).arg(lap(*r)));
    r->step = QStringLiteral("folders");
    r->api->categories([r](ApiFail fail, QList<Category> cats) {
        if (fail != ApiFail::None)
            return finish(r, fail);
        r->api->feeds([r, cats](ApiFail fail, QList<Feed> feeds) {
            if (fail != ApiFail::None)
                return finish(r, fail);
            r->store->replaceCategories(cats);
            r->store->replaceFeeds(feeds);
            Log::write(QStringLiteral("sync: categories=%1 feeds=%2 (%3 ms)").arg(cats.size()).arg(feeds.size()).arg(lap(*r)));
            r->step = QStringLiteral("entries");
            r->api->unread(r->limit, [r](ApiFail fail, QList<Entry> entries, int total) {
                if (fail != ApiFail::None)
                    return finish(r, fail);
                r->entries = entries;
                r->total = total;
                Log::write(QStringLiteral("sync: entries got=%1 total=%2 limit=%3 (%4 ms)")
                               .arg(entries.size()).arg(total).arg(r->limit).arg(lap(*r)));
                r->step = QStringLiteral("fulltext");
                fetchFrom(r, 0);
            });
        });
    });
}

void sendFrom(const Run &r, int i)
{
    if (i >= r->sends.size())
        return download(r);
    r->sends[i]([r, i](ApiFail fail) {
        if (fail == ApiFail::Rejected) {
            // Sending it again would be refused again and block every later sync.
            QStringList what;
            for (const StatusChange &c : r->sendChanges[i])
                what << QString::number(c.entryId);
            Log::write(QStringLiteral("miniflux: change refused, dropped: %1").arg(what.join(QLatin1Char(','))));
        } else if (fail != ApiFail::None) {
            return finish(r, fail);
        }
        r->store->clearChanges(r->sendChanges[i]);
        sendFrom(r, i + 1);
    });
}

} // namespace

MinifluxSource::MinifluxSource(MinifluxApi &api, int limit, PrepareFn prepare, RemovedFn removed, ReadyFn ready)
    : m_api(api)
    , m_limit(limit)
    , m_prepare(std::move(prepare))
    , m_removed(std::move(removed))
    , m_ready(std::move(ready))
{
}

void MinifluxSource::sync(Store &store, Progress progress, std::function<void(QString)> done)
{
    auto r = std::make_shared<SyncRun>();
    r->api = &m_api;
    r->store = &store;
    r->limit = m_limit;
    r->prepare = m_prepare;
    r->removed = m_removed;
    r->ready = m_ready;
    r->progress = std::move(progress);
    r->done = std::move(done);

    // "All read" steps are queued first: per-article marks still waiting were made after them.
    QList<StatusChange> read, unread, star, unstar;
    QList<qint64> readIds, unreadIds, starIds, unstarIds;
    for (const StatusChange &c : store.pendingChanges()) {
        switch (c.kind) {
        case StatusChange::Read: read << c; readIds << c.entryId; break;
        case StatusChange::Unread: unread << c; unreadIds << c.entryId; break;
        case StatusChange::Star: star << c; starIds << c.entryId; break;
        case StatusChange::Unstar: unstar << c; unstarIds << c.entryId; break;
        case StatusChange::AllRead: {
            MinifluxApi *api = &m_api;
            const bool category = c.feedId < 0;
            const qint64 id = category ? c.categoryId : c.feedId;
            r->sends << [api, category, id](std::function<void(ApiFail)> cb) { api->markAllRead(category, id, cb); };
            r->sendChanges << QList<StatusChange>{c};
            break;
        }
        }
    }
    MinifluxApi *api = &m_api;
    auto add = [&](const QList<StatusChange> &changes, const QList<qint64> &ids, bool status, bool value) {
        if (ids.isEmpty())
            return;
        if (status)
            r->sends << [api, ids, value](std::function<void(ApiFail)> cb) { api->setStatus(ids, value, cb); };
        else
            r->sends << [api, ids, value](std::function<void(ApiFail)> cb) { api->setStarred(ids, value, cb); };
        r->sendChanges << changes;
    };
    add(read, readIds, true, true);
    add(unread, unreadIds, true, false);
    add(star, starIds, false, true);
    add(unstar, unstarIds, false, false);
    int allRead = 0;
    for (const auto &changes : r->sendChanges)
        for (const StatusChange &c : changes)
            allRead += c.kind == StatusChange::AllRead ? 1 : 0;
    r->sendCounts = QStringLiteral("read=%1 unread=%2 star=%3 unstar=%4 allread=%5")
                        .arg(read.size()).arg(unread.size()).arg(star.size()).arg(unstar.size()).arg(allRead);
    r->started.start();
    r->stepStarted.start();
    sendFrom(r, 0);
}
