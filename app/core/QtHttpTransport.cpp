#include "QtHttpTransport.h"

#include "Log.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>

#include <memory>

namespace {
const int kIdleTimeoutMs = 30000; // abort when no data arrives for this long (not a whole-request cap)
}

QtHttpTransport::QtHttpTransport(const QString &caBundlePath, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    if (caBundlePath.isEmpty())
        return;
    const QList<QSslCertificate> cas = QSslCertificate::fromPath(caBundlePath, QSsl::Pem);
    Log::write(QStringLiteral("CA bundle %1: %2 certificates").arg(caBundlePath).arg(cas.size()));
    if (cas.isEmpty())
        return;
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setCaCertificates(cas);
    QSslConfiguration::setDefaultConfiguration(conf);
}

QNetworkRequest toQtRequest(const HttpRequest &request)
{
    QNetworkRequest req(request.url);
    bool hasAuth = false;
    for (const auto &h : request.headers) {
        req.setRawHeader(h.first, h.second);
        hasAuth |= h.first.compare("Authorization", Qt::CaseInsensitive) == 0
                   || h.first.compare("X-Auth-Token", Qt::CaseInsensitive) == 0;
    }
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     hasAuth ? QNetworkRequest::SameOriginRedirectPolicy : QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

void QtHttpTransport::send(const HttpRequest &request, std::function<void(const HttpResponse &)> done)
{
    // After the firmware switches Wi-Fi off, Qt's bearer layer may keep the manager "not accessible"
    // forever ("Network access is disabled" on the reader); our own code checks connectivity.
    if (m_nam->networkAccessible() != QNetworkAccessManager::Accessible)
        m_nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    const QNetworkRequest req = toQtRequest(request);
    QNetworkReply *reply = m_nam->sendCustomRequest(req, request.method, request.body);

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    auto timedOut = std::make_shared<bool>(false);
    QObject::connect(timer, &QTimer::timeout, reply, [reply, timedOut] {
        *timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, timer, [timer] { timer->start(kIdleTimeoutMs); });
    timer->start(kIdleTimeoutMs);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done, timedOut] {
        HttpResponse r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        r.body = reply->readAll();
        r.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        // Qt keeps the HTTP status after an abort or a dropped connection once headers arrived;
        // errors below 200 are transport-level (connection, timeout, cancel, proxy), not HTTP 4xx/5xx.
        const QNetworkReply::NetworkError err = reply->error();
        if (r.status == 0 || (err != QNetworkReply::NoError && err < QNetworkReply::ContentAccessDenied))
            r.networkError = reply->errorString();
        r.timedOut = *timedOut;
        reply->deleteLater();
        done(r);
    });
}
