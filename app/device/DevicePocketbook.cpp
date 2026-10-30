#include "Device.h"

#include "Log.h"

#include <common_utilities/einkinterface.h>
#include <inkview.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>
#include <QSet>
#include <QTimer>

QString device::firmwareLanguage()
{
    const char *lang = currentLang();
    return lang ? QString::fromLatin1(lang) : QString();
}

bool device::isPocketbook()
{
    return true;
}

QString device::firmwareInfo()
{
    auto str = [](const char *s) { return s ? QString::fromUtf8(s) : QStringLiteral("?"); };
    return QStringLiteral("model=%1 hw=%2 fw=%3").arg(str(GetDeviceModel()), str(GetHardwareType()), str(GetSoftwareVersion()));
}

void device::openBook(const QString &path, int flags)
{
    const int rc = OpenBook(path.toUtf8().constData(), "", flags);
    Log::write(QStringLiteral("OpenBook(%1, flags=0x%2) -> %3").arg(path).arg(flags, 0, 16).arg(rc));
}

void device::openBookAt(const QString &path, const QString &parameters)
{
    const int rc = OpenBook(path.toUtf8().constData(), parameters.toUtf8().constData(), 0);
    Log::write(QStringLiteral("OpenBook(%1, \"%2\", 0) -> %3").arg(path, parameters).arg(rc));
}

void device::requireFullUpdate()
{
    Log::debug(QStringLiteral("full e-ink refresh"));
    if (auto *eink = pocketbook::utilities::einkPlatform())
        eink->requireFullUpdate();
}

bool device::networkConnected()
{
    return (QueryNetwork() & NET_CONNECTED) != 0;
}

namespace {
// NetConnectAsync takes a plain function pointer: the answer goes to the latest caller. One attempt
// at a time and the timeout are NetworkGate's job.
std::function<void(bool)> g_connectDone;
QTimer *g_connectPoll = nullptr;
qint64 g_connectStarted = 0;

void finishConnect(bool ok, const char *how)
{
    if (g_connectPoll) {
        g_connectPoll->stop();
        g_connectPoll->deleteLater();
        g_connectPoll = nullptr;
    }
    auto done = std::move(g_connectDone);
    g_connectDone = nullptr;
    if (!done)
        return;
    Log::write(QStringLiteral("wifi: %1 after %2 ms (%3)")
                   .arg(ok ? QStringLiteral("up") : QStringLiteral("failed"))
                   .arg(QDateTime::currentMSecsSinceEpoch() - g_connectStarted)
                   .arg(QLatin1String(how)));
    done(ok);
}

int onNetConnected(int status)
{
    Log::write(QStringLiteral("NetConnectAsync -> %1").arg(status));
    QMetaObject::invokeMethod(qApp, [status] { finishConnect(status == NET_OK, "firmware answer"); }, Qt::QueuedConnection);
    return 0;
}
} // namespace

void device::connectNetworkAsync(std::function<void(bool)> done)
{
    g_connectDone = std::move(done);
    g_connectStarted = QDateTime::currentMSecsSinceEpoch();
    // The firmware answers from another thread and the posted call waits for the next event:
    // polling the state twice a second wakes the loop and notices the network.
    if (!g_connectPoll) {
        g_connectPoll = new QTimer(qApp);
        g_connectPoll->setInterval(500);
        QObject::connect(g_connectPoll, &QTimer::timeout, qApp, [] {
            if (device::networkConnected())
                finishConnect(true, "state check");
            else if (QDateTime::currentMSecsSinceEpoch() - g_connectStarted > 120000)
                finishConnect(false, "gave up");
        });
        g_connectPoll->start();
    }
    NetConnectAsync(onNetConnected);
}

void device::openNetworkDialog()
{
    const int rc = NetConnect(nullptr);
    Log::write(QStringLiteral("NetConnect(dialog) -> %1").arg(rc));
}

void device::installKeyHandler(std::function<void(int)> handler)
{
    auto *eink = pocketbook::utilities::einkPlatform();
    if (!eink) {
        Log::write(QStringLiteral("installKeyHandler: einkPlatform() is null"));
        return;
    }
    eink->addHandlerForInkvewEvents([handler](int type, int key, int) {
        // Hardware keys are logged (codes differ between models). Codes from IV_KEY_MAX up are
        // on-screen keyboard text, passwords included, and never reach the log.
        const bool keyEvent = type == EVT_KEYPRESS || type == EVT_KEYPRESS_EXT || type == EVT_KEYRELEASE
            || type == EVT_KEYRELEASE_EXT || type == EVT_KEYREPEAT || type == EVT_KEYREPEAT_EXT;
        if (keyEvent && key < IV_KEY_MAX)
            Log::write(QStringLiteral("hardware key event %1 key 0x%2").arg(type).arg(key, 0, 16));
        if (type != EVT_KEYPRESS && type != EVT_KEYPRESS_EXT)
            return;
        int k;
        if (key == IV_KEY_NEXT || key == IV_KEY_NEXT2)
            k = KeyNext;
        else if (key == IV_KEY_PREV || key == IV_KEY_PREV2)
            k = KeyPrev;
        else if (key == IV_KEY_BACK)
            k = KeyBack;
        else
            return;
        QMetaObject::invokeMethod(qApp, [handler, k] { handler(k); }, Qt::QueuedConnection);
    });
}

void device::installBackFilter(std::function<bool()> canGoBack)
{
    auto *eink = pocketbook::utilities::einkPlatform();
    if (!eink)
        return;
    eink->setKeyFilter([canGoBack](int event, int key) {
        if (key != IV_KEY_BACK)
            return false;
        const bool keep = canGoBack();
        Log::write(QStringLiteral("back key event %1, kept from firmware: %2").arg(event).arg(keep));
        return keep;
    });
}

void device::installForegroundHandler(std::function<void()> handler)
{
    auto *eink = pocketbook::utilities::einkPlatform();
    if (!eink)
        return;
    eink->addHandlerForInkvewEvents([handler](int type, int, int) {
        switch (type) {
        case EVT_KEYPRESS: case EVT_KEYRELEASE: case EVT_KEYPRESS_EXT: case EVT_KEYRELEASE_EXT:
        case EVT_POINTERUP: case EVT_POINTERDOWN: case EVT_POINTERMOVE: case EVT_REPAINT:
            return;
        default:
            break;
        }
        static QSet<int> seen;
        if (!seen.contains(type)) {
            seen.insert(type);
            QMetaObject::invokeMethod(qApp, [type] { Log::write(QStringLiteral("inkview event %1").arg(type)); }, Qt::QueuedConnection);
        }
        if (type == EVT_FOREGROUND || type == EVT_SHOW || type == EVT_ACTIVATE)
            QMetaObject::invokeMethod(qApp, [handler] { handler(); }, Qt::QueuedConnection);
    });
}

int device::screenDpi()
{
    return get_screen_dpi();
}

int device::panelHeight()
{
    return PanelHeight();
}
