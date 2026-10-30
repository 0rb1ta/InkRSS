#include "Config.h"

#include "FileIO.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

Config loadConfig(const QString &path)
{
    Config c;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return c;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QString mode = o.value("mode").toString();
    if (mode == QLatin1String("miniflux"))
        c.mode = Config::Miniflux;
    else if (mode == QLatin1String("direct"))
        c.mode = Config::Direct;
    c.auth.server = QUrl(o.value("server").toString().trimmed());
    c.auth.token = o.value("token").toString().trimmed();
    c.auth.user = o.value("user").toString().trimmed();
    c.auth.password = o.value("password").toString();
    c.limit = o.value("limit").toInt(c.limit);
    if (c.limit <= 0)
        c.limit = 200;
    c.fontPt = o.value("fontPt").toInt(0);
    c.fontFamily = o.value("fontFamily").toString();
    c.flatIssue = o.value("flatIssue").toBool();
    c.images = o.value("images").toBool(true);
    c.language = o.value("language").toString();
    c.headlineSize = qBound(Config::kHeadlineSizeMin, o.value("headlineSize").toInt(Config::kHeadlineSizeDefault),
                            Config::kHeadlineSizeMax);
    return c;
}

bool saveConfig(const QString &path, const Config &c)
{
    QJsonObject o;
    switch (c.mode) {
    case Config::None: break;
    case Config::Miniflux: o.insert("mode", "miniflux"); break;
    case Config::Direct: o.insert("mode", "direct"); break;
    }
    o.insert("server", c.auth.server.toString());
    o.insert("token", c.auth.token);
    o.insert("user", c.auth.user);
    o.insert("password", c.auth.password);
    o.insert("limit", c.limit);
    o.insert("fontPt", c.fontPt);
    o.insert("fontFamily", c.fontFamily);
    o.insert("flatIssue", c.flatIssue);
    o.insert("images", c.images);
    o.insert("language", c.language);
    o.insert("headlineSize", c.headlineSize);
    return writeFileIfChanged(path, QJsonDocument(o).toJson());
}
