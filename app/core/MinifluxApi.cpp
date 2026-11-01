#include "MinifluxApi.h"
#include "I18n.h"

#include "Log.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

namespace {

// Miniflux writes RFC 3339 times, sometimes with microseconds and an offset. Kept to whole
// seconds in UTC so times compare reliably.
QDateTime parseTime(const QString &s)
{
    QString t = s;
    static const QRegularExpression fraction(QStringLiteral("\\.\\d+"));
    t.remove(fraction);
    QDateTime d = QDateTime::fromString(t, Qt::ISODate);
    if (!d.isValid())
        return QDateTime();
    return QDateTime::fromSecsSinceEpoch(d.toSecsSinceEpoch(), Qt::UTC);
}

Entry parseEntry(const QJsonObject &o)
{
    Entry e;
    e.id = qint64(o.value("id").toDouble());
    e.feedId = qint64(o.value("feed_id").toDouble());
    e.title = o.value("title").toString();
    e.url = o.value("url").toString();
    e.author = o.value("author").toString();
    e.published = parseTime(o.value("published_at").toString());
    e.html = o.value("content").toString();
    e.read = o.value("status").toString() != QLatin1String("unread");
    e.starred = o.value("starred").toBool();
    e.changed = parseTime(o.value("changed_at").toString());
    e.readingMinutes = o.value("reading_time").toInt();
    if (e.readingMinutes <= 0)
        e.readingMinutes = readingMinutes(e.html);
    return e;
}

QByteArray idsBody(const QList<qint64> &ids, const char *field, const QJsonValue &value)
{
    QJsonArray arr;
    for (qint64 id : ids)
        arr.append(double(id));
    QJsonObject o;
    o.insert("entry_ids", arr);
    o.insert(QLatin1String(field), value);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

} // namespace

QString failText(ApiFail fail)
{
    switch (fail) {
    case ApiFail::None: return QString();
    case ApiFail::Network: return tx("Нет связи с сервером Miniflux");
    case ApiFail::Auth: return tx("Сервер не принял логин, пароль или ключ API");
    case ApiFail::NotMiniflux: return tx("По этому адресу не отвечает Miniflux");
    case ApiFail::Server: return tx("Ошибка на сервере Miniflux");
    case ApiFail::Rejected: return tx("Сервер отклонил изменение");
    }
    return QString();
}

MinifluxApi::MinifluxApi(HttpTransport &transport, MinifluxAuth auth)
    : m_transport(transport)
    , m_auth(std::move(auth))
{
}

QUrl MinifluxApi::url(const QString &path, const QList<QPair<QString, QString>> &query) const
{
    QUrl u = m_auth.server;
    QString base = u.path();
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    u.setPath(base + QStringLiteral("v1/") + path);
    if (!query.isEmpty()) {
        QUrlQuery q;
        q.setQueryItems(query);
        u.setQuery(q);
    }
    return u;
}

void MinifluxApi::call(const QByteArray &method, const QUrl &u, const QByteArray &body,
                       std::function<void(ApiFail, const QByteArray &)> done)
{
    HttpRequest r;
    r.method = method;
    r.url = u;
    r.body = body;
    if (!m_auth.token.isEmpty())
        r.headers << qMakePair(QByteArray("X-Auth-Token"), m_auth.token.toUtf8());
    else
        r.headers << qMakePair(QByteArray("Authorization"),
                               "Basic " + (m_auth.user + QLatin1Char(':') + m_auth.password).toUtf8().toBase64());
    if (!body.isEmpty())
        r.headers << qMakePair(QByteArray("Content-Type"), QByteArray("application/json"));
    r.headers << qMakePair(QByteArray("Accept"), QByteArray("application/json"));
    const QString what = QString::fromLatin1(method) + QLatin1Char(' ') + u.path();
    const bool change = method != "GET";
    m_transport.send(r, [done, what, change](const HttpResponse &resp) {
        ApiFail fail = ApiFail::None;
        if (!resp.networkError.isEmpty() || resp.status == 0)
            fail = ApiFail::Network;
        else if (resp.status == 401 || resp.status == 403)
            fail = ApiFail::Auth;
        else if (change && resp.status >= 400 && resp.status < 500)
            fail = ApiFail::Rejected;
        else if (resp.status == 404)
            fail = ApiFail::NotMiniflux;
        else if (resp.status >= 300)
            fail = ApiFail::Server;
        else if (resp.status != 204 && !resp.contentType.startsWith("application/json"))
            fail = ApiFail::NotMiniflux; // a login page or some other site
        if (fail != ApiFail::None)
            Log::write(QStringLiteral("miniflux: %1 -> %2 %3").arg(what).arg(resp.status).arg(resp.networkError));
        done(fail, resp.body);
    });
}

void MinifluxApi::get(const QString &path, const QList<QPair<QString, QString>> &query,
                      std::function<void(ApiFail, const QByteArray &)> done)
{
    call("GET", url(path, query), QByteArray(), std::move(done));
}

void MinifluxApi::me(std::function<void(ApiFail, QString)> done)
{
    get(QStringLiteral("me"), {}, [done](ApiFail f, const QByteArray &body) {
        if (f != ApiFail::None)
            return done(f, QString());
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject() || !doc.object().contains("username"))
            return done(ApiFail::NotMiniflux, QString());
        done(ApiFail::None, doc.object().value("username").toString());
    });
}

void MinifluxApi::categories(std::function<void(ApiFail, QList<Category>)> done)
{
    get(QStringLiteral("categories"), {}, [done](ApiFail f, const QByteArray &body) {
        QList<Category> out;
        if (f != ApiFail::None)
            return done(f, out);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray())
            return done(ApiFail::NotMiniflux, out);
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            out << Category{qint64(o.value("id").toDouble()), o.value("title").toString()};
        }
        done(ApiFail::None, out);
    });
}

void MinifluxApi::feeds(std::function<void(ApiFail, QList<Feed>)> done)
{
    get(QStringLiteral("feeds"), {}, [done](ApiFail f, const QByteArray &body) {
        QList<Feed> out;
        if (f != ApiFail::None)
            return done(f, out);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray())
            return done(ApiFail::NotMiniflux, out);
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            Feed feed;
            feed.id = qint64(o.value("id").toDouble());
            feed.categoryId = qint64(o.value("category").toObject().value("id").toDouble());
            feed.title = o.value("title").toString();
            feed.url = o.value("feed_url").toString();
            feed.error = o.value("parsing_error_message").toString();
            out << feed;
        }
        done(ApiFail::None, out);
    });
}

void MinifluxApi::unread(int limit, std::function<void(ApiFail, QList<Entry>, int)> done)
{
    get(QStringLiteral("entries"),
        {{QStringLiteral("status"), QStringLiteral("unread")},
         {QStringLiteral("limit"), QString::number(limit)},
         {QStringLiteral("order"), QStringLiteral("published_at")},
         {QStringLiteral("direction"), QStringLiteral("desc")}},
        [done](ApiFail f, const QByteArray &body) {
            QList<Entry> out;
            if (f != ApiFail::None)
                return done(f, out, 0);
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            if (!doc.isObject())
                return done(ApiFail::NotMiniflux, out, 0);
            for (const QJsonValue &v : doc.object().value("entries").toArray())
                out << parseEntry(v.toObject());
            done(ApiFail::None, out, doc.object().value("total").toInt());
        });
}

void MinifluxApi::starred(int limit, int offset, std::function<void(ApiFail, QList<Entry>, int)> done)
{
    get(QStringLiteral("entries"),
        {{QStringLiteral("starred"), QStringLiteral("true")},
         {QStringLiteral("limit"), QString::number(limit)},
         {QStringLiteral("offset"), QString::number(offset)},
         {QStringLiteral("order"), QStringLiteral("published_at")},
         {QStringLiteral("direction"), QStringLiteral("desc")}},
        [done](ApiFail f, const QByteArray &body) {
            QList<Entry> out;
            if (f != ApiFail::None)
                return done(f, out, 0);
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            if (!doc.isObject())
                return done(ApiFail::NotMiniflux, out, 0);
            for (const QJsonValue &v : doc.object().value("entries").toArray())
                out << parseEntry(v.toObject());
            done(ApiFail::None, out, doc.object().value("total").toInt());
        });
}

void MinifluxApi::fetchContent(qint64 id, std::function<void(ApiFail, QString)> done)
{
    // update_content: the server keeps the extracted article, so the next sync gets it directly.
    get(QStringLiteral("entries/%1/fetch-content").arg(id), {{QStringLiteral("update_content"), QStringLiteral("true")}}, [done](ApiFail f, const QByteArray &body) {
        if (f != ApiFail::None)
            return done(f, QString());
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject())
            return done(ApiFail::NotMiniflux, QString());
        done(ApiFail::None, doc.object().value("content").toString());
    });
}

void MinifluxApi::setStatus(const QList<qint64> &ids, bool read, std::function<void(ApiFail)> done)
{
    call("PUT", url(QStringLiteral("entries")),
         idsBody(ids, "status", read ? QStringLiteral("read") : QStringLiteral("unread")),
         [done](ApiFail f, const QByteArray &) { done(f); });
}

void MinifluxApi::setStarred(const QList<qint64> &ids, bool starred, std::function<void(ApiFail)> done)
{
    call("PUT", url(QStringLiteral("entries")), idsBody(ids, "starred", starred),
         [done](ApiFail f, const QByteArray &) { done(f); });
}

void MinifluxApi::markAllRead(bool category, qint64 id, std::function<void(ApiFail)> done)
{
    const QString path = QStringLiteral("%1/%2/mark-all-as-read")
                             .arg(category ? QStringLiteral("categories") : QStringLiteral("feeds"))
                             .arg(id);
    call("PUT", url(path), QByteArray(), [done](ApiFail f, const QByteArray &) { done(f); });
}
