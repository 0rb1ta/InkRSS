#include "Store.h"

#include "Log.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// Kinds are stored by name: a number tied to the enum order would silently change meaning when
// a value is added in the middle.
QString kindName(StatusChange::Kind kind)
{
    switch (kind) {
    case StatusChange::Read: return QStringLiteral("read");
    case StatusChange::Unread: return QStringLiteral("unread");
    case StatusChange::Star: return QStringLiteral("star");
    case StatusChange::Unstar: return QStringLiteral("unstar");
    case StatusChange::AllRead: return QStringLiteral("allread");
    }
    return QString();
}

bool kindFromName(const QString &name, StatusChange::Kind *kind)
{
    if (name == QLatin1String("read")) *kind = StatusChange::Read;
    else if (name == QLatin1String("unread")) *kind = StatusChange::Unread;
    else if (name == QLatin1String("star")) *kind = StatusChange::Star;
    else if (name == QLatin1String("unstar")) *kind = StatusChange::Unstar;
    else if (name == QLatin1String("allread")) *kind = StatusChange::AllRead;
    else return false;
    return true;
}

qint64 secs(const QDateTime &t)
{
    return t.isValid() ? t.toSecsSinceEpoch() : 0;
}

QDateTime fromSecs(qint64 s)
{
    return s ? QDateTime::fromSecsSinceEpoch(s, Qt::UTC) : QDateTime();
}

const char *kEntryColumns = "e.id, e.feed_id, e.title, e.url, e.author, e.published, e.html, e.read, e.starred, e.changed, e.reading_min";

Entry readEntry(const QSqlQuery &q)
{
    Entry e;
    e.id = q.value(0).toLongLong();
    e.feedId = q.value(1).toLongLong();
    e.title = q.value(2).toString();
    e.url = q.value(3).toString();
    e.author = q.value(4).toString();
    e.published = fromSecs(q.value(5).toLongLong());
    e.html = q.value(6).toString();
    e.read = q.value(7).toBool();
    e.starred = q.value(8).toBool();
    e.changed = fromSecs(q.value(9).toLongLong());
    e.readingMinutes = q.value(10).toInt();
    return e;
}

bool exec(QSqlQuery &q)
{
    if (q.exec())
        return true;
    Log::write(QStringLiteral("store: %1").arg(q.lastError().text()));
    return false;
}

bool exec(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    if (q.exec(sql))
        return true;
    Log::write(QStringLiteral("store: %1").arg(q.lastError().text()));
    return false;
}

} // namespace

Store::Store()
    : m_conn(QStringLiteral("store-%1").arg(quintptr(this)))
{
}

Store::~Store()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_conn, false);
        if (db.isValid())
            db.close();
    }
    if (QSqlDatabase::contains(m_conn))
        QSqlDatabase::removeDatabase(m_conn);
}

bool Store::open(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conn);
    db.setDatabaseName(path);
    if (!db.open()) {
        Log::write(QStringLiteral("store: cannot open %1: %2").arg(path, db.lastError().text()));
        return false;
    }
    return exec(db, "CREATE TABLE IF NOT EXISTS categories (id INTEGER PRIMARY KEY, title TEXT)")
        && exec(db, "CREATE TABLE IF NOT EXISTS feeds (id INTEGER PRIMARY KEY, category_id INTEGER, title TEXT, url TEXT, error TEXT)")
        && exec(db, "CREATE TABLE IF NOT EXISTS entries (id INTEGER PRIMARY KEY, feed_id INTEGER, title TEXT, url TEXT,"
                    " author TEXT, published INTEGER, html TEXT, read INTEGER, starred INTEGER, changed INTEGER, reading_min INTEGER)")
        && exec(db, "CREATE INDEX IF NOT EXISTS entries_feed ON entries (feed_id, published)")
        && exec(db, "CREATE TABLE IF NOT EXISTS changes (seq INTEGER PRIMARY KEY AUTOINCREMENT, entry_id INTEGER,"
                    " kind TEXT, feed_id INTEGER, category_id INTEGER, at INTEGER)")
        && exec(db, "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)")
        && exec(db, "CREATE TABLE IF NOT EXISTS gone (id INTEGER PRIMARY KEY)")
        && exec(db, "CREATE TABLE IF NOT EXISTS nofulltext (id INTEGER PRIMARY KEY)")
        && exec(db, "CREATE TABLE IF NOT EXISTS fulltextfails (id INTEGER PRIMARY KEY, attempts INTEGER)");
}

void Store::replaceCategories(const QList<Category> &categories)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    exec(db, "DELETE FROM categories");
    QSqlQuery q(db);
    q.prepare("INSERT INTO categories (id, title) VALUES (?, ?)");
    for (const Category &c : categories) {
        q.addBindValue(c.id);
        q.addBindValue(c.title);
        exec(q);
    }
    db.commit();
}

qint64 Store::addCategory(const QString &title)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.prepare("SELECT id FROM categories WHERE title = ?");
    q.addBindValue(title);
    exec(q);
    if (q.next())
        return q.value(0).toLongLong();
    q.prepare("INSERT INTO categories (id, title) VALUES ((SELECT IFNULL(MAX(id), 0) + 1 FROM categories), ?)");
    q.addBindValue(title);
    if (!exec(q))
        return 0;
    return q.lastInsertId().toLongLong();
}

QList<qint64> Store::replaceFeeds(const QList<Feed> &feeds)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    exec(db, "CREATE TEMP TABLE IF NOT EXISTS keep (id INTEGER PRIMARY KEY)");
    exec(db, "DELETE FROM keep");
    QSqlQuery k(db);
    k.prepare("INSERT OR IGNORE INTO keep (id) VALUES (?)");
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO feeds (id, category_id, title, url, error) VALUES (?, ?, ?, ?, ?)");
    for (const Feed &f : feeds) {
        k.addBindValue(f.id);
        exec(k);
        q.addBindValue(f.id);
        q.addBindValue(f.categoryId);
        q.addBindValue(f.title);
        q.addBindValue(f.url);
        q.addBindValue(f.error);
        exec(q);
    }
    QList<qint64> removed;
    QSqlQuery old(db);
    old.prepare("SELECT id FROM entries WHERE feed_id NOT IN (SELECT id FROM keep)");
    exec(old);
    while (old.next())
        removed << old.value(0).toLongLong();
    old.finish();
    QSqlQuery gone(db);
    gone.prepare("INSERT OR IGNORE INTO gone (id) VALUES (?)");
    for (qint64 id : removed) {
        gone.addBindValue(id);
        exec(gone);
    }
    exec(db, "DELETE FROM entries WHERE feed_id NOT IN (SELECT id FROM keep)");
    exec(db, "DELETE FROM feeds WHERE id NOT IN (SELECT id FROM keep)");
    db.commit();
    return removed;
}

qint64 Store::addFeed(const Feed &feed)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("INSERT OR REPLACE INTO feeds (id, category_id, title, url, error) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(feed.id ? QVariant(feed.id) : QVariant(QVariant::LongLong));
    q.addBindValue(feed.categoryId);
    q.addBindValue(feed.title);
    q.addBindValue(feed.url);
    q.addBindValue(feed.error);
    if (!exec(q))
        return 0;
    return feed.id ? feed.id : q.lastInsertId().toLongLong();
}

QList<qint64> Store::removeFeed(qint64 feedId)
{
    QList<qint64> ids;
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.prepare("SELECT id FROM entries WHERE feed_id = ?");
    q.addBindValue(feedId);
    exec(q);
    while (q.next())
        ids << q.value(0).toLongLong();
    q.prepare("DELETE FROM entries WHERE feed_id = ?");
    q.addBindValue(feedId);
    exec(q);
    q.prepare("DELETE FROM feeds WHERE id = ?");
    q.addBindValue(feedId);
    exec(q);
    return ids;
}

void Store::setFeedError(qint64 feedId, const QString &error)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("UPDATE feeds SET error = ? WHERE id = ?");
    q.addBindValue(error);
    q.addBindValue(feedId);
    exec(q);
}

void Store::setFeedTitle(qint64 feedId, const QString &title)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("UPDATE feeds SET title = ? WHERE id = ?");
    q.addBindValue(title);
    q.addBindValue(feedId);
    exec(q);
}

void Store::upsertEntries(const QList<Entry> &entries)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    QSqlQuery find(db);
    find.prepare("SELECT 1 FROM entries WHERE id = ?");
    // Queued local changes that must survive a refresh.
    QSqlQuery pendingRead(db);
    pendingRead.prepare("SELECT 1 FROM changes c WHERE (c.entry_id = ? AND c.kind IN ('read', 'unread'))"
                        " OR (c.kind = 'allread' AND (c.feed_id = ?"
                        "  OR c.category_id = (SELECT category_id FROM feeds WHERE id = ?)))");
    QSqlQuery pendingStar(db);
    pendingStar.prepare("SELECT 1 FROM changes WHERE entry_id = ? AND kind IN ('star', 'unstar')");
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO entries (id, feed_id, title, url, author, published, html, read, starred, changed, reading_min)"
                   " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    QSqlQuery update(db);
    update.prepare("UPDATE entries SET feed_id = ?, title = ?, url = ?, author = ?, published = ?, html = ?, reading_min = ? WHERE id = ?");
    QSqlQuery setRead(db);
    setRead.prepare("UPDATE entries SET read = ?, changed = ? WHERE id = ?");
    QSqlQuery setStar(db);
    setStar.prepare("UPDATE entries SET starred = ? WHERE id = ?");

    for (const Entry &e : entries) {
        pendingRead.addBindValue(e.id);
        pendingRead.addBindValue(e.feedId);
        pendingRead.addBindValue(e.feedId);
        exec(pendingRead);
        const bool keepRead = pendingRead.next();
        pendingRead.finish();
        pendingStar.addBindValue(e.id);
        exec(pendingStar);
        const bool keepStar = pendingStar.next();
        pendingStar.finish();

        find.addBindValue(e.id);
        exec(find);
        const bool known = find.next();
        find.finish();
        if (!known) {
            insert.addBindValue(e.id);
            insert.addBindValue(e.feedId);
            insert.addBindValue(e.title);
            insert.addBindValue(e.url);
            insert.addBindValue(e.author);
            insert.addBindValue(secs(e.published));
            insert.addBindValue(e.html);
            insert.addBindValue(e.read || keepRead);
            insert.addBindValue(e.starred);
            insert.addBindValue(secs(e.changed));
            insert.addBindValue(e.readingMinutes);
            exec(insert);
            continue;
        }
        update.addBindValue(e.feedId);
        update.addBindValue(e.title);
        update.addBindValue(e.url);
        update.addBindValue(e.author);
        update.addBindValue(secs(e.published));
        update.addBindValue(e.html);
        update.addBindValue(e.readingMinutes);
        update.addBindValue(e.id);
        exec(update);
        if (!keepRead) {
            setRead.addBindValue(e.read);
            setRead.addBindValue(secs(e.changed));
            setRead.addBindValue(e.id);
            exec(setRead);
        }
        if (!keepStar) {
            setStar.addBindValue(e.starred);
            setStar.addBindValue(e.id);
            exec(setStar);
        }
    }
    db.commit();
}

QList<Category> Store::categories() const
{
    QList<Category> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT id, title FROM categories ORDER BY title COLLATE NOCASE");
    exec(q);
    while (q.next())
        out << Category{q.value(0).toLongLong(), q.value(1).toString()};
    return out;
}

QList<Feed> Store::feeds(qint64 categoryId) const
{
    QList<Feed> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT id, category_id, title, url, error FROM feeds WHERE ? < 0 OR category_id = ?"
              " ORDER BY title COLLATE NOCASE");
    q.addBindValue(categoryId);
    q.addBindValue(categoryId);
    exec(q);
    while (q.next())
        out << Feed{q.value(0).toLongLong(), q.value(1).toLongLong(), q.value(2).toString(),
                    q.value(3).toString(), q.value(4).toString()};
    return out;
}

int Store::unreadCount(qint64 feedId, qint64 categoryId) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT COUNT(*) FROM entries e LEFT JOIN feeds f ON f.id = e.feed_id WHERE e.read = 0"
              " AND (? < 0 OR e.feed_id = ?) AND (? < 0 OR f.category_id = ?)");
    q.addBindValue(feedId);
    q.addBindValue(feedId);
    q.addBindValue(categoryId);
    q.addBindValue(categoryId);
    exec(q);
    return q.next() ? q.value(0).toInt() : 0;
}

QList<Entry> Store::entries(qint64 feedId, qint64 categoryId, bool unreadOnly, int limit) const
{
    QList<Entry> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare(QStringLiteral("SELECT %1 FROM entries e LEFT JOIN feeds f ON f.id = e.feed_id"
                             " WHERE (? < 0 OR e.feed_id = ?) AND (? < 0 OR f.category_id = ?) AND (? = 0 OR e.read = 0)"
                             " ORDER BY e.published DESC, e.id DESC LIMIT ?")
                  .arg(QLatin1String(kEntryColumns)));
    q.addBindValue(feedId);
    q.addBindValue(feedId);
    q.addBindValue(categoryId);
    q.addBindValue(categoryId);
    q.addBindValue(unreadOnly);
    q.addBindValue(limit);
    exec(q);
    while (q.next())
        out << readEntry(q);
    return out;
}

QList<Entry> Store::starred() const
{
    QList<Entry> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare(QStringLiteral("SELECT %1 FROM entries e WHERE e.starred = 1 ORDER BY e.published DESC, e.id DESC")
                  .arg(QLatin1String(kEntryColumns)));
    exec(q);
    while (q.next())
        out << readEntry(q);
    return out;
}

QList<qint64> Store::unreadIds() const
{
    QList<qint64> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT id FROM entries WHERE read = 0");
    exec(q);
    while (q.next())
        out << q.value(0).toLongLong();
    return out;
}

QList<qint64> Store::allIds() const
{
    QList<qint64> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT id FROM entries");
    exec(q);
    while (q.next())
        out << q.value(0).toLongLong();
    return out;
}

void Store::setNoFullText(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("INSERT OR IGNORE INTO nofulltext (id) VALUES (?)");
    q.addBindValue(id);
    exec(q);
}

bool Store::noFullText(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT 1 FROM nofulltext WHERE id = ?");
    q.addBindValue(id);
    exec(q);
    return q.next();
}

void Store::noteFullTextFailure(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("INSERT OR REPLACE INTO fulltextfails (id, attempts)"
              " VALUES (?, COALESCE((SELECT attempts FROM fulltextfails WHERE id = ?), 0) + 1)");
    q.addBindValue(id);
    q.addBindValue(id);
    exec(q);
}

void Store::clearFullTextFailures(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("DELETE FROM fulltextfails WHERE id = ?");
    q.addBindValue(id);
    exec(q);
}

int Store::fullTextFailures(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT attempts FROM fulltextfails WHERE id = ?");
    q.addBindValue(id);
    exec(q);
    return q.next() ? q.value(0).toInt() : 0;
}

Entry Store::entry(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare(QStringLiteral("SELECT %1 FROM entries e WHERE e.id = ?").arg(QLatin1String(kEntryColumns)));
    q.addBindValue(id);
    exec(q);
    return q.next() ? readEntry(q) : Entry();
}

void Store::setRead(qint64 id, bool read)
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), Qt::UTC);
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("UPDATE entries SET read = ?, changed = ? WHERE id = ?");
    q.addBindValue(read);
    q.addBindValue(secs(now));
    q.addBindValue(id);
    exec(q);
    StatusChange c;
    c.entryId = id;
    c.kind = read ? StatusChange::Read : StatusChange::Unread;
    c.at = now;
    queue(c);
}

void Store::setStarred(qint64 id, bool starred)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("UPDATE entries SET starred = ? WHERE id = ?");
    q.addBindValue(starred);
    q.addBindValue(id);
    exec(q);
    StatusChange c;
    c.entryId = id;
    c.kind = starred ? StatusChange::Star : StatusChange::Unstar;
    c.at = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), Qt::UTC);
    queue(c);
}

void Store::markAllRead(qint64 feedId, qint64 categoryId)
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), Qt::UTC);
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("UPDATE entries SET read = 1, changed = ? WHERE read = 0 AND (? < 0 OR feed_id = ?)"
              " AND (? < 0 OR feed_id IN (SELECT id FROM feeds WHERE category_id = ?))");
    q.addBindValue(secs(now));
    q.addBindValue(feedId);
    q.addBindValue(feedId);
    q.addBindValue(categoryId);
    q.addBindValue(categoryId);
    exec(q);
    // Earlier per-article marks in the same scope are superseded; the source gets "all read"
    // first and then only the marks made after it.
    q.prepare("DELETE FROM changes WHERE kind IN ('read', 'unread') AND entry_id IN (SELECT id FROM entries"
              " WHERE (? < 0 OR feed_id = ?) AND (? < 0 OR feed_id IN (SELECT id FROM feeds WHERE category_id = ?)))");
    q.addBindValue(feedId);
    q.addBindValue(feedId);
    q.addBindValue(categoryId);
    q.addBindValue(categoryId);
    exec(q);
    StatusChange c;
    c.kind = StatusChange::AllRead;
    c.feedId = feedId;
    c.categoryId = categoryId;
    c.at = now;
    queue(c);
}

void Store::setReadFromSource(const QList<qint64> &ids)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    QSqlQuery q(db);
    q.prepare("UPDATE entries SET read = 1 WHERE id = ? AND NOT EXISTS ("
              " SELECT 1 FROM changes c WHERE (c.entry_id = entries.id AND c.kind IN ('read', 'unread'))"
              " OR (c.kind = 'allread' AND (c.feed_id = entries.feed_id"
              " OR c.category_id = (SELECT category_id FROM feeds WHERE feeds.id = entries.feed_id))))");
    for (qint64 id : ids) {
        q.addBindValue(id);
        exec(q);
    }
    db.commit();
}

void Store::setStarredFromSource(const QSet<qint64> &ids)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    QSqlQuery local(db);
    local.prepare("SELECT id FROM entries WHERE starred = 1");
    exec(local);
    QList<qint64> starred;
    while (local.next())
        starred << local.value(0).toLongLong();
    local.finish();
    QSqlQuery pending(db);
    pending.prepare("SELECT 1 FROM changes WHERE entry_id = ? AND kind IN ('star', 'unstar')");
    QSqlQuery unstar(db);
    unstar.prepare("UPDATE entries SET starred = 0 WHERE id = ?");
    for (qint64 id : starred) {
        if (ids.contains(id))
            continue;
        pending.addBindValue(id);
        exec(pending);
        const bool keep = pending.next();
        pending.finish();
        if (!keep) {
            unstar.addBindValue(id);
            exec(unstar);
        }
    }
    db.commit();
}

void Store::queue(const StatusChange &change)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    // A newer mark replaces an older one of the same kind group for the same target.
    switch (change.kind) {
    case StatusChange::Read:
    case StatusChange::Unread:
        q.prepare("DELETE FROM changes WHERE entry_id = ? AND kind IN ('read', 'unread')");
        q.addBindValue(change.entryId);
        break;
    case StatusChange::Star:
    case StatusChange::Unstar:
        q.prepare("DELETE FROM changes WHERE entry_id = ? AND kind IN ('star', 'unstar')");
        q.addBindValue(change.entryId);
        break;
    case StatusChange::AllRead:
        q.prepare("DELETE FROM changes WHERE kind = 'allread' AND feed_id = ? AND category_id = ?");
        q.addBindValue(change.feedId);
        q.addBindValue(change.categoryId);
        break;
    }
    exec(q);
    q.prepare("INSERT INTO changes (entry_id, kind, feed_id, category_id, at) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(change.entryId);
    q.addBindValue(kindName(change.kind));
    q.addBindValue(change.feedId);
    q.addBindValue(change.categoryId);
    q.addBindValue(secs(change.at));
    exec(q);
}

QList<StatusChange> Store::pendingChanges() const
{
    QList<StatusChange> out;
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT seq, entry_id, kind, feed_id, category_id, at FROM changes ORDER BY seq");
    exec(q);
    while (q.next()) {
        StatusChange c;
        if (!kindFromName(q.value(2).toString(), &c.kind))
            continue;
        c.seq = q.value(0).toLongLong();
        c.entryId = q.value(1).toLongLong();
        c.feedId = q.value(3).toLongLong();
        c.categoryId = q.value(4).toLongLong();
        c.at = fromSecs(q.value(5).toLongLong());
        out << c;
    }
    return out;
}

void Store::clearChanges(const QList<StatusChange> &sent)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    db.transaction();
    QSqlQuery q(db);
    q.prepare("DELETE FROM changes WHERE seq = ?");
    for (const StatusChange &c : sent) {
        q.addBindValue(c.seq);
        exec(q);
    }
    db.commit();
}

QList<qint64> Store::removeReadExcept(int keep)
{
    QList<qint64> ids;
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.prepare("SELECT id FROM entries WHERE read = 1 AND starred = 0 ORDER BY published DESC, id DESC LIMIT -1 OFFSET ?");
    q.addBindValue(keep);
    exec(q);
    while (q.next())
        ids << q.value(0).toLongLong();
    db.transaction();
    QSqlQuery del(db);
    del.prepare("DELETE FROM entries WHERE id = ?");
    QSqlQuery gone(db);
    gone.prepare("INSERT OR IGNORE INTO gone (id) VALUES (?)");
    for (qint64 id : ids) {
        del.addBindValue(id);
        exec(del);
        gone.addBindValue(id);
        exec(gone);
    }
    db.commit();
    return ids;
}

bool Store::known(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT 1 FROM entries WHERE id = ? UNION ALL SELECT 1 FROM gone WHERE id = ?");
    q.addBindValue(id);
    q.addBindValue(id);
    exec(q);
    return q.next();
}

QString Store::meta(const QString &key) const
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("SELECT value FROM meta WHERE key = ?");
    q.addBindValue(key);
    exec(q);
    return q.next() ? q.value(0).toString() : QString();
}

void Store::setMeta(const QString &key, const QString &value)
{
    QSqlQuery q(QSqlDatabase::database(m_conn));
    q.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    exec(q);
}
