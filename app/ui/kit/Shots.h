#pragma once

#include <QString>

class QWidget;

namespace Shots {
// Saves the whole window, keeps the newest 50 screenshots and logs the file with the screen name.
void save(QWidget *window, const QString &screen);
} // namespace Shots
