#pragma once

#include "Models.h"

#include <QSet>
#include <QString>

// Local copy of folders, feeds and articles plus the queue of status changes not yet sent to the
// source. One SQLite file per mode, so ids from different sources never meet.
class Store {
public:
    Store();
    ~Store();
    Store(const Store &) = delete;
    Store &operator=(const Store &) = delete;

    bool open(const QString &path);

    void replaceCategories(const QList<Category> &categories);
    qint64 addCategory(const QString &title); // existing id when the title is taken
    // Feeds missing from the new list are removed together with their articles; returns those article ids.
    QList<qint64> replaceFeeds(const QList<Feed> &feeds);
    qint64 addFeed(const Feed &feed); // id 0: a new id is assigned
    QList<qint64> removeFeed(qint64 feedId); // returns the ids of the articles removed with it
    void setFeedError(qint64 feedId, const QString &error);
    void setFeedTitle(qint64 feedId, const QString &title);

    // New articles are inserted; known ones get fresh text. The source's read/starred status wins
    // unless a local change for that article is still waiting in the queue.
    void upsertEntries(const QList<Entry> &entries);

    QList<Category> categories() const;
    QList<Feed> feeds(qint64 categoryId = -1) const;
    int unreadCount(qint64 feedId = -1, qint64 categoryId = -1) const;
    // Newest first; -1 means "any".
    QList<Entry> entries(qint64 feedId, qint64 categoryId, bool unreadOnly, int limit) const;
    QList<Entry> starred() const;
    QList<qint64> unreadIds() const;
    QList<qint64> allIds() const;
    Entry entry(qint64 id) const; // id 0 when missing
    // In the store now, or dropped by removeReadExcept() earlier (a read article that a feed may
    // still carry must not come back as new).
    bool known(qint64 id) const;

    void setRead(qint64 id, bool read);
    void setStarred(qint64 id, bool starred);
    void markAllRead(qint64 feedId, qint64 categoryId);
    // Applies statuses learned from the source without queueing or overriding pending local changes.
    void setReadFromSource(const QList<qint64> &ids);
    void setStarredFromSource(const QSet<qint64> &ids);

    QList<StatusChange> pendingChanges() const;
    void clearChanges(const QList<StatusChange> &sent);

    // Drops read, non-starred articles beyond the newest `keep`; returns the removed ids.
    QList<qint64> removeReadExcept(int keep);

    // Articles whose full text the server failed to extract: not asked for again.
    void setNoFullText(qint64 id);
    bool noFullText(qint64 id) const;
    void noteFullTextFailure(qint64 id);
    void clearFullTextFailures(qint64 id);
    int fullTextFailures(qint64 id) const;

    QString meta(const QString &key) const;
    void setMeta(const QString &key, const QString &value);

private:
    void queue(const StatusChange &change);
    QString m_conn;
};
