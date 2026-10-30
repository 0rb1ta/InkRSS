#pragma once

#include <QString>

// Where the app keeps its own data.
// App data lives under <storage>/system so the reader's library never indexes it.
namespace Paths {

QString dataDir(const QString &storageRoot = QStringLiteral("/mnt/ext1"));

} // namespace Paths
