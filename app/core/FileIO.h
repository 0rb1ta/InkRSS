#pragma once

#include <QByteArray>
#include <QString>

// Atomic save that skips the write when the file already holds the same bytes (spares the flash).
// true: written or already equal.
bool writeFileIfChanged(const QString &path, const QByteArray &data);
