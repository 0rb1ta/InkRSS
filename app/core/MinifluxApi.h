#pragma once

#include "HttpTransport.h"
#include "Models.h"

#include <QUrl>

#include <functional>

struct MinifluxAuth {
    QUrl server;      // with or without a sub-path, e.g. https://host/miniflux/
    QString token;    // API key; used instead of the login when set
    QString user;
    QString password;
};

// Rejected: the server refused a change (4xx other than 401/403 on PUT), e.g. an article it no
// longer has. Sending it again would fail the same way.
enum class ApiFail { None, Network, Auth, NotMiniflux, Server, Rejected };

// Message for the screen; empty for ApiFail::None.
QString failText(ApiFail fail);

// Miniflux REST API (/v1), only the calls the reader needs.
class MinifluxApi {
public:
    MinifluxApi(HttpTransport &transport, MinifluxAuth auth);

    void me(std::function<void(ApiFail, QString user)> done);
    void categories(std::function<void(ApiFail, QList<Category>)> done);
    void feeds(std::function<void(ApiFail, QList<Feed>)> done);
    // Newest unread articles; `total` is how many unread the server has in all.
    void unread(int limit, std::function<void(ApiFail, QList<Entry>, int total)> done);
    // Starred articles, read or not, newest first.
    void starred(int limit, std::function<void(ApiFail, QList<Entry>)> done);
    // Asks the server to download the original page and extract the article.
    void fetchContent(qint64 id, std::function<void(ApiFail, QString html)> done);
    void setStatus(const QList<qint64> &ids, bool read, std::function<void(ApiFail)> done);
    // Sets (not toggles) the star, so a change made elsewhere is never flipped back.
    void setStarred(const QList<qint64> &ids, bool starred, std::function<void(ApiFail)> done);
    void markAllRead(bool category, qint64 id, std::function<void(ApiFail)> done);

private:
    QUrl url(const QString &path, const QList<QPair<QString, QString>> &query = {}) const;
    void call(const QByteArray &method, const QUrl &url, const QByteArray &body,
              std::function<void(ApiFail, const QByteArray &)> done);
    void get(const QString &path, const QList<QPair<QString, QString>> &query,
             std::function<void(ApiFail, const QByteArray &)> done);

    HttpTransport &m_transport;
    MinifluxAuth m_auth;
};
