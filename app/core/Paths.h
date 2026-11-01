#pragma once

#include <QList>
#include <QString>

// Where the app keeps its own data.
// App data lives under <storage>/system so the reader's library never indexes it.
namespace Paths {

QString dataDir(const QString &storageRoot = QStringLiteral("/mnt/ext1"));
QString minifluxDatabase(const QString &dataDir, const QString &profile);
QString articlesDir(const QString &dataDir, const QString &profile = QString());
QList<QString> allArticlesDirs(const QString &dataDir);
// Copies the legacy single-account data without deleting it; false leaves the legacy paths in use.
bool migrateMinifluxAccount(const QString &dataDir, const QString &profile);
// Removes legacy data only after the migrated database contains every old article.
bool cleanupLegacyMinifluxAccount(const QString &dataDir, const QString &profile);

} // namespace Paths
