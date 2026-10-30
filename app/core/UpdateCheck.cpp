#include "UpdateCheck.h"

#include "HttpTransport.h"
#include "Log.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

const char *kLatestRelease = "https://api.github.com/repos/0rb1ta/InkRSS/releases/latest";

QStringList parts(QString v)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        v.remove(0, 1);
    return v.split(QLatin1Char('.'));
}

} // namespace

UpdateCheck::UpdateCheck(HttpTransport *transport, const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_current(currentVersion)
{
}

int UpdateCheck::compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = parts(a), pb = parts(b);
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int x = pa.value(i).toInt(), y = pb.value(i).toInt();
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

void UpdateCheck::run()
{
    HttpRequest req;
    req.method = "GET";
    req.url = QUrl(QString::fromLatin1(kLatestRelease));
    req.headers = {{"Accept", "application/vnd.github+json"},
                   {"User-Agent", "InkRSS/" + m_current.toUtf8()}};
    m_transport->send(req, [this](const HttpResponse &r) {
        if (r.status != 200) {
            Log::write(QStringLiteral("update check: %1 %2").arg(r.status).arg(r.networkError));
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        const QString tag = o.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty() || o.value(QStringLiteral("draft")).toBool() || o.value(QStringLiteral("prerelease")).toBool())
            return;
        Log::write(QStringLiteral("update check: latest %1, running %2").arg(tag, m_current));
        if (compareVersions(tag, m_current) <= 0)
            return;
        m_latest = tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? tag.mid(1) : tag;
        m_url = o.value(QStringLiteral("html_url")).toString();
        emit found();
    });
}
