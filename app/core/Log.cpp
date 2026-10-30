#include "Log.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

namespace {

QMutex g_mutex;
QString g_path;
bool g_debug = false;

QString redact(QString line)
{
    static const QRegularExpression bearer(QStringLiteral("((?:Bearer|Basic)\\s+|X-Auth-Token:\\s*)[^\\s\"']+"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression jsonToken(
        QStringLiteral("(\"(?:token|password|api_key)\"\\s*:\\s*\")[^\"]*"));
    line.replace(bearer, QStringLiteral("\\1***"));
    line.replace(jsonToken, QStringLiteral("\\1***"));
    return line;
}

} // namespace

void Log::init(const QString &filePath, qint64 rotateAtBytes)
{
    QMutexLocker lock(&g_mutex);
    g_path = filePath;
    g_debug = !g_path.isEmpty() && QFileInfo::exists(QFileInfo(g_path).absolutePath() + QStringLiteral("/debug"));
    if (g_path.isEmpty() || QFileInfo(g_path).size() <= rotateAtBytes)
        return;
    const QFileInfo fi(g_path);
    const QString old = fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".1.") + fi.suffix();
    QFile::remove(old);
    QFile::rename(g_path, old);
}

namespace {

QStringList lastLines(const QString &path, int maxLines)
{
    QFile f(path);
    if (path.isEmpty() || !f.open(QIODevice::ReadOnly))
        return {};
    const qint64 window = 64 * 1024;
    const bool cut = f.size() > window;
    if (cut)
        f.seek(f.size() - window);
    QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), QString::SkipEmptyParts);
    if (cut && !lines.isEmpty())
        lines.removeFirst(); // the window starts in the middle of a line
    return lines.mid(qMax(0, lines.size() - maxLines));
}

} // namespace

QStringList Log::tail(int maxLines)
{
    QMutexLocker lock(&g_mutex);
    QStringList lines = lastLines(g_path, maxLines);
    // Right after a rotation the run with the problem is in the previous file: show its end too.
    if (lines.size() < maxLines && !g_path.isEmpty()) {
        const QFileInfo fi(g_path);
        const QString previous = fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".1.") + fi.suffix();
        lines = lastLines(previous, maxLines - lines.size()) + lines;
    }
    return lines;
}

void Log::write(const QString &line)
{
    const QString stamped = QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        + QLatin1Char(' ') + redact(line) + QLatin1Char('\n');
    QMutexLocker lock(&g_mutex);
    if (g_path.isEmpty())
        return;
    QFile f(g_path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write(stamped.toUtf8());
}

void Log::debug(const QString &line)
{
    if (g_debug)
        write(QStringLiteral("[debug] ") + line);
}

bool Log::debugEnabled()
{
    return g_debug;
}

void Log::setDebug(bool on)
{
    QMutexLocker lock(&g_mutex);
    if (g_path.isEmpty())
        return;
    const QString flag = QFileInfo(g_path).absolutePath() + QStringLiteral("/debug");
    if (on) {
        QFile f(flag);
        if (f.open(QIODevice::WriteOnly))
            f.close();
    } else {
        QFile::remove(flag);
    }
    g_debug = on;
}
