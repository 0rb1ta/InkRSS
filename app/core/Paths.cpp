#include "Paths.h"

#include "ArticleHtml.h"
#include "Store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

QString ensured(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

} // namespace

QString Paths::dataDir(const QString &storageRoot)
{
    if (!QFileInfo(storageRoot).isDir())
        return ensured(QDir::homePath() + QStringLiteral("/.inkrss"));
    const QString dir = ensured(storageRoot + QStringLiteral("/system/inkrss"));
    return dir;
}

QString Paths::minifluxDatabase(const QString &dataDir, const QString &profile)
{
    return profile.isEmpty() ? dataDir + QStringLiteral("/feeds-miniflux.db")
                             : dataDir + QStringLiteral("/accounts/") + profile + QStringLiteral("/feeds.db");
}

QString Paths::articlesDir(const QString &dataDir, const QString &profile)
{
    return profile.isEmpty() ? dataDir + QStringLiteral("/articles")
                             : dataDir + QStringLiteral("/accounts/") + profile + QStringLiteral("/articles");
}

QList<QString> Paths::allArticlesDirs(const QString &dataDir)
{
    QList<QString> dirs{articlesDir(dataDir)};
    const QDir accounts(dataDir + QStringLiteral("/accounts"));
    for (const QString &profile : accounts.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        dirs << articlesDir(dataDir, profile);
    return dirs;
}

bool Paths::migrateMinifluxAccount(const QString &dataDir, const QString &profile)
{
    if (profile.isEmpty())
        return false;
    const QString legacyDb = minifluxDatabase(dataDir, QString());
    if (!QFileInfo::exists(legacyDb))
        return true;
    const QString db = minifluxDatabase(dataDir, profile);
    if (!QDir().mkpath(QFileInfo(db).absolutePath()))
        return false;
    if (!QFileInfo::exists(db) && !QFile::copy(legacyDb, db))
        return false;

    const QDir oldArticles(articlesDir(dataDir, QString()));
    const QDir newArticles(articlesDir(dataDir, profile));
    if (oldArticles.exists() && !QDir().mkpath(newArticles.absolutePath()))
        return false;
    for (const QString &name : oldArticles.entryList({QStringLiteral("*.html")}, QDir::Files)) {
        const QString target = newArticles.filePath(name);
        if (!QFileInfo::exists(target) && !QFile::copy(oldArticles.filePath(name), target))
            return false;
    }
    return true;
}

bool Paths::cleanupLegacyMinifluxAccount(const QString &dataDir, const QString &profile)
{
    if (profile.isEmpty())
        return false;
    const QString legacyDb = minifluxDatabase(dataDir, QString());
    if (!QFileInfo::exists(legacyDb))
        return true;
    QList<qint64> ids;
    {
        Store legacy;
        if (!legacy.open(legacyDb))
            return false;
        ids = legacy.allIds();
    }
    {
        Store migrated;
        if (!migrated.open(minifluxDatabase(dataDir, profile)))
            return false;
        for (qint64 id : ids)
            if (!migrated.known(id))
                return false;
    }
    removeArticleFiles(articlesDir(dataDir), ids);
    const bool removed = QFile::remove(legacyDb);
    for (const char *suffix : {"-journal", "-wal", "-shm"})
        QFile::remove(legacyDb + QLatin1String(suffix));
    return removed;
}
