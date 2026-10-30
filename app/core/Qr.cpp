#include "Qr.h"

#include "qrcodegen.hpp"

#include <QPainter>

QImage makeQrImage(const QString &text, int modulePx, int quietModules)
{
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = qr.getSize() + 2 * quietModules;
    QImage img(modules * modulePx, modules * modulePx, QImage::Format_Grayscale8);
    img.fill(Qt::white);
    QPainter p(&img);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y))
                p.fillRect((x + quietModules) * modulePx, (y + quietModules) * modulePx, modulePx, modulePx, Qt::black);
    return img;
}
