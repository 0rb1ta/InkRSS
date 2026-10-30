#pragma once

#include <QString>

// Optional icon in the reader's Applications menu (system/config/desktop/view.json, firmware 6.x).
namespace DesktopIcon {

enum class Result { Installed, Removed, NoViewJson, BadJson, NoIcons, WriteFailed };

QString viewJsonPath(const QString &storageRoot = QStringLiteral("/mnt/ext1"));
bool isInstalled(const QString &viewJson);
Result install(const QString &viewJson, const QString &storageRoot = QStringLiteral("/mnt/ext1"));
Result remove(const QString &viewJson);
QString message(Result r);

} // namespace DesktopIcon
