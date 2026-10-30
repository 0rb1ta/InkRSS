#include "Paths.h"

#include <QDir>
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
