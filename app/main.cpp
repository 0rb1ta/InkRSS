// InkRSS: RSS reader with Miniflux support for PocketBook.

#include "AppController.h"
#include "Device.h"
#include "Log.h"
#include "Paths.h"
#include "Theme.h"

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QFontDatabase>
#include <QTextCodec>

namespace {

// Firmware 6.8 passes QT_QPA_PLATFORM=pocketbook, but only
// the pocketbook2 plugin exists.
void prepareQtEnvironment()
{
    if (QFileInfo::exists(QStringLiteral("/ebrmain/lib/qtplugins/platforms/libqpocketbook2.so"))) {
        qputenv("QT_QPA_PLATFORM", "pocketbook2");
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", "/ebrmain/lib/qtplugins/platforms");
        qputenv("QT_PLUGIN_PATH", "/ebrmain/lib/qtplugins");
    }
}

// Feed and file names can be Cyrillic. If the firmware starts us with a non-UTF-8 locale, Qt 5.12
// would encode them through that codec and FAT32 rejects the result ('?' replacements).
QString ensureUtf8FileNames()
{
    const QByteArray before = QTextCodec::codecForLocale()->name();
    if (before != "UTF-8")
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    return QStringLiteral("locale codec %1 -> %2, LANG=%3 LC_ALL=%4")
        .arg(QString::fromLatin1(before), QString::fromLatin1(QTextCodec::codecForLocale()->name()),
             QString::fromLocal8Bit(qgetenv("LANG")), QString::fromLocal8Bit(qgetenv("LC_ALL")));
}

} // namespace

int main(int argc, char **argv)
{
    prepareQtEnvironment();
    QApplication app(argc, argv);
    const QString codecInfo = ensureUtf8FileNames();

    const QString data = Paths::dataDir();
    Log::init(data + QStringLiteral("/log.txt"));
    Log::write(QStringLiteral("==== InkRSS %1 (build %2) start, %3, platform %4")
                   .arg(QStringLiteral(APP_VERSION), QStringLiteral(APP_BUILD), device::firmwareInfo(), QApplication::platformName()));
    Log::write(codecInfo);
    device::installCrashLog(data + QStringLiteral("/log.txt"));

    // Extra article fonts: any .ttf/.otf the user copies into system/inkrss/fonts.
    QStringList families;
    const QDir fonts(data + QStringLiteral("/fonts"));
    for (const QString &name : fonts.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files)) {
        const int id = QFontDatabase::addApplicationFont(fonts.filePath(name));
        if (id >= 0)
            families += QFontDatabase::applicationFontFamilies(id);
    }
    app.setProperty("inkrssFonts", families);

    app.setStyleSheet(Theme::styleSheet());
    AppController window;
    window.showFullScreen();
    window.start();
    device::requireFullUpdate();

    const int rc = app.exec();
    Log::write(QStringLiteral("==== exit %1").arg(rc));
    return rc;
}
