#include "Device.h"

#include <QLocale>
#include <QTimer>

#include "Log.h"

QString device::firmwareLanguage()
{
    return QLocale::system().name().left(2);
}

void device::installBackFilter(std::function<bool()>)
{
}

bool device::isPocketbook()
{
    return false;
}

QString device::firmwareInfo()
{
    return QStringLiteral("host (no PocketBook)");
}

void device::openBookAt(const QString &path, const QString &parameters)
{
    Log::write(QStringLiteral("[stub] openBookAt %1 %2").arg(path, parameters));
}

void device::openBook(const QString &path, int flags)
{
    Log::write(QStringLiteral("[stub] openBook %1 flags=%2").arg(path).arg(flags));
}

void device::requireFullUpdate()
{
}

bool device::networkConnected()
{
    return true;
}

void device::connectNetworkAsync(std::function<void(bool)> done)
{
    // Answer later, as the firmware does: callers must not rely on a synchronous answer.
    QTimer::singleShot(0, [done] { done(true); });
}

void device::openNetworkDialog()
{
    Log::write(QStringLiteral("[stub] openNetworkDialog"));
}

void device::installKeyHandler(std::function<void(int)>)
{
}

int device::screenDpi()
{
    return 0;
}

int device::panelHeight()
{
    return 0;
}

void device::installForegroundHandler(std::function<void()>)
{
}
