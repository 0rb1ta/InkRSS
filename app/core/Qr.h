#pragma once

#include <QImage>
#include <QString>

// Black-on-white QR code for links shown on the reader (buy / share / site-only books).
QImage makeQrImage(const QString &text, int modulePx, int quietModules = 4);
