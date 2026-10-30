#pragma once

#include "HttpTransport.h"

#include <QObject>

#include <QNetworkRequest>

class QNetworkAccessManager;

// Our request as a QNetworkRequest. Redirects are followed, but a request that carries the
// Authorization or X-Auth-Token header never follows one to another host (the key stays on the server).
QNetworkRequest toQtRequest(const HttpRequest &request);

// HttpTransport over QNetworkAccessManager. `caBundlePath` (PEM) replaces the system CA list:
// the reader's firmware ships no usable CA store for Qt.
class QtHttpTransport : public QObject, public HttpTransport {
public:
    explicit QtHttpTransport(const QString &caBundlePath = {}, QObject *parent = nullptr);
    void send(const HttpRequest &request, std::function<void(const HttpResponse &)> done) override;

private:
    QNetworkAccessManager *m_nam;
};
