#pragma once

#include <QString>
#include <QStringList>

// Append-only diagnostic log. Anything that looks like an auth token is replaced with "***".
namespace Log {

// A file over `rotateAtBytes` is moved to "<name>.1.txt" (the previous one is dropped).
void init(const QString &filePath, qint64 rotateAtBytes = 1024 * 1024);
void write(const QString &line);
// Verbose lines (screen changes, full e-ink refreshes) only when the file "debug" lies next to the log.
void debug(const QString &line);
bool debugEnabled();
// Turns verbose lines (and the screen pictures) on or off by writing or deleting the "debug" file.
void setDebug(bool on);
// The last lines, reading at most 64 KB from the end (the "Журнал" screen).
QStringList tail(int maxLines);

} // namespace Log
