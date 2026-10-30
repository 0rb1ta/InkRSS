#include "Shots.h"

#include "HoldDetector.h"
#include "Log.h"
#include "Paths.h"

#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QWidget>

void Shots::save(QWidget *window, const QString &screen)
{
    const QString dir = Paths::dataDir() + QStringLiteral("/shots");
    QDir().mkpath(dir);
    const QString name = shotFileName(QDateTime::currentDateTime());
    const bool ok = window->grab().save(dir + QLatin1Char('/') + name, "PNG");
    Log::write(QStringLiteral("shot: %1 screen=%2%3").arg(name, screen, ok ? QString() : QStringLiteral(" (not saved)")));
}
