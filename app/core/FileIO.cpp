#include "FileIO.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>

namespace {

QMutex g_mutex;
QHash<QString, QByteArray> g_written; // path -> SHA-1 of the bytes last on disk

QByteArray digest(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
}

} // namespace

bool writeFileIfChanged(const QString &path, const QByteArray &data)
{
    QMutexLocker lock(&g_mutex);
    const QByteArray sum = digest(data);
    if (!g_written.contains(path)) {
        QFile current(path);
        if (current.open(QIODevice::ReadOnly))
            g_written.insert(path, digest(current.readAll()));
    }
    if (g_written.value(path) == sum && QFile::exists(path))
        return true;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        return false;
    g_written.insert(path, sum);
    return true;
}
