#include "HoldDetector.h"

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
    return t.toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".png");
}
