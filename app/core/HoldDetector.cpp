#include "HoldDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfoList>

void HoldDetector::press(int key, qint64 ms)
{
    m_key = key;
    m_pressedAt = ms;
}

bool HoldDetector::release(int key, qint64 ms)
{
    if (key != m_key)
        return false;
    m_key = -1;
    return ms - m_pressedAt >= m_holdMs;
}

QString shotFileName(const QDateTime &t)
{
    return t.toString(QStringLiteral("yyyyMMdd-HHmmsszzz")) + QStringLiteral(".png");
}

int pruneShots(const QString &dirPath, int keep)
{
    const QDir dir(dirPath);
    QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
    int removed = 0;
    while (files.size() > qMax(0, keep)) {
        if (QFile::remove(files.takeFirst().absoluteFilePath()))
            ++removed;
    }
    return removed;
}
