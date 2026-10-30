#include "RetryTransport.h"

#include "Log.h"

#include <QTimer>

RetryTransport::RetryTransport(HttpTransport &inner, int pauseMs)
    : m_inner(inner)
    , m_pauseMs(pauseMs)
{
}

void RetryTransport::send(const HttpRequest &request, std::function<void(const HttpResponse &)> done)
{
    attempt(request, std::move(done), 1);
}

void RetryTransport::attempt(const HttpRequest &request, std::function<void(const HttpResponse &)> done, int tried)
{
    m_inner.send(request, [this, request, done, tried](const HttpResponse &r) {
        const bool noAnswer = r.status == 0 && !r.networkError.isEmpty() && !r.timedOut;
        if (!noAnswer || tried >= kTries)
            return done(r);
        Log::write(QStringLiteral("net: %1 %2: %3, try %4 of %5")
                       .arg(QString::fromLatin1(request.method), request.url.path(), r.networkError)
                       .arg(tried + 1).arg(kTries));
        QTimer::singleShot(m_pauseMs, [this, request, done, tried] { attempt(request, done, tried + 1); });
    });
}
