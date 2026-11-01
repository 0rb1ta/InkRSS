#pragma once

#include <QDateTime>
#include <QString>

// Tells a long press of a key from a short one, from press and release times in milliseconds.
class HoldDetector {
public:
    explicit HoldDetector(int holdMs = 1500)
        : m_holdMs(holdMs)
    {
    }
    void press(int key, qint64 ms);
    // True when `key` was held at least holdMs; a release of another key is ignored.
    bool release(int key, qint64 ms);

private:
    int m_holdMs;
    int m_key = -1;
    qint64 m_pressedAt = 0;
};

// "20261031-140512000.png": file name of a screenshot taken at `t`, with milliseconds.
QString shotFileName(const QDateTime &t);
// Keeps only the newest `keep` screenshots and returns how many were removed.
int pruneShots(const QString &dirPath, int keep);
