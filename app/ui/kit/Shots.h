#pragma once

#include <QString>

class QWidget;

namespace Shots {
// Saves the whole window as system/inkrss/shots/<time>.png and logs the file with the screen name.
void save(QWidget *window, const QString &screen);
} // namespace Shots
