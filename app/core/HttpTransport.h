#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

#include <functional>

using HttpHeaders = QList<QPair<QByteArray, QByteArray>>;

struct HttpRequest {
    QByteArray method;
    QUrl url;
    HttpHeaders headers;
    QByteArray body;
};

struct HttpResponse {
    int status = 0;
    QByteArray body;
    QByteArray contentType;
    QString networkError; // non-empty when the request never got an HTTP answer
    bool timedOut = false; // no data for too long: the transport gave up
};

// Network seam: the real implementation wraps QNetworkAccessManager, tests use FakeTransport.
class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual void send(const HttpRequest &request, std::function<void(const HttpResponse &)> done) = 0;
};
