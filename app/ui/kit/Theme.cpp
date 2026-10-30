#include "Theme.h"

#include "Device.h"

#include <QFile>
#include <QApplication>
#include <QFontDatabase>

#include "I18n.h"
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

int Theme::mm(double millimetres)
{
    static const double dpi = [] {
        // Qt's physicalDotsPerInch is wrong on PocketBook (sizes came out ~1.7x on the 970):
        // the firmware's own value comes first.
        const int fw = device::screenDpi();
        if (fw >= 50 && fw <= 600)
            return double(fw);
        const QScreen *s = QGuiApplication::primaryScreen();
        const double d = s ? s->physicalDotsPerInch() : 0;
        return d >= 50 && d <= 600 ? d : 150.0;
    }();
    return qRound(millimetres * dpi / 25.4);
}

QString Theme::fontFamily()
{
    // The firmware's own UI fonts first (the thin default font is hard to read at 150 dpi).
    static const QString family = [] {
        const QStringList available = QFontDatabase().families();
        for (const char *name : {"Roboto", "PT Sans", "Open Sans", "Noto Sans", "Liberation Sans", "DejaVu Sans"})
            if (available.contains(QLatin1String(name)))
                return QString::fromLatin1(name);
        return QString();
    }();
    return family;
}

static QFont sized(double mmHeight, QFont::Weight weight)
{
    QFont f;
    if (!Theme::fontFamily().isEmpty())
        f.setFamily(Theme::fontFamily());
    f.setPixelSize(Theme::mm(mmHeight));
    f.setWeight(weight);
    return f;
}

QFont Theme::titleFont() { return sized(4.6, QFont::Bold); }
QFont Theme::bodyFont() { return sized(3.6, QFont::DemiBold); }
QFont Theme::smallFont() { return sized(3.0, QFont::DemiBold); }

QFont Theme::articleFont(const QString &family, double mmHeight, bool bold)
{
    QFont f = bodyFont();
    if (!family.isEmpty())
        f.setFamily(family);
    else if (QFontDatabase().families().contains(QStringLiteral("PT Serif")))
        f.setFamily(QStringLiteral("PT Serif"));
    f.setPixelSize(mm(mmHeight));
    f.setWeight(bold ? QFont::Bold : QFont::Normal);
    return f;
}

QString Theme::styleSheet()
{
    const int b = 2;
    const int r = mm(1.0);
    return QStringLiteral(
               "* { color: #000; background: #fff; }"
               "QPushButton { border: %1px solid #000; border-radius: %2px; padding: 0 %3px; min-height: %4px; outline: none; }"
               "QPushButton[variant=\"dark\"] { background: #000; color: #fff; }"
               "QPushButton[variant=\"muted\"] { border-style: dashed; color: #444; }"
               "QPushButton:disabled { color: #444; }"
               "QLineEdit { border: %1px solid #000; border-radius: %2px; padding: 0 %3px; min-height: %4px; }"
               "QLineEdit:focus { border-width: 3px; }"
               "QFrame[frame=\"box\"] { border: %1px solid #000; border-radius: %2px; }"
               "QLabel[role=\"badge\"] { background: #000; color: #fff; padding: 1px 4px; border-radius: 3px; }"
               "QLabel[role=\"muted\"] { color: #333; }")
        .arg(b)
        .arg(r)
        .arg(mm(2))
        .arg(mm(8));
}

QIcon Theme::icon(const QString &name)
{
    if (name.startsWith(QLatin1String("regular:")))
        return QIcon(QStringLiteral(":/icons/regular-%1.svg").arg(name.mid(8)));
    return QIcon(QStringLiteral(":/icons/solid-%1.svg").arg(name));
}

QIcon Theme::icon(const QString &name, const QColor &color)
{
    const QString path = name.startsWith(QLatin1String("regular:"))
        ? QStringLiteral(":/icons/regular-%1.svg").arg(name.mid(8))
        : QStringLiteral(":/icons/solid-%1.svg").arg(name);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QIcon();
    // Font Awesome paths have no fill of their own: a fill on the root element colours them all.
    QByteArray svg = f.readAll();
    svg.replace("<svg ", "<svg fill=\"" + color.name().toLatin1() + "\" ");
    QSvgRenderer renderer(svg);
    QPixmap pm(128, 128);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    const QSizeF s = renderer.defaultSize().scaled(pm.size(), Qt::KeepAspectRatio);
    renderer.render(&p, QRectF((pm.width() - s.width()) / 2, (pm.height() - s.height()) / 2, s.width(), s.height()));
    p.end();
    return QIcon(pm);
}

QStringList Theme::articleFamilies()
{
    const QStringList available = QFontDatabase().families();
    QStringList out{tx("По умолчанию")};
    for (const char *name : {"PT Serif", "Literata", "Charis SIL", "Noto Serif", "DejaVu Serif", "Liberation Serif",
                             "Georgia", "Roboto", "PT Sans", "Open Sans", "Noto Sans", "DejaVu Sans"})
        if (available.contains(QLatin1String(name)))
            out << QLatin1String(name);
    for (const QString &family : qApp->property("inkrssFonts").toStringList())
        if (!out.contains(family))
            out << family;
    return out;
}
