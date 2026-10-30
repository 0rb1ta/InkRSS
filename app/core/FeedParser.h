#pragma once

#include "Models.h"

#include <QByteArray>

struct ParsedFeed {
    QString title;
    QList<Entry> entries; // feedId is left 0 for the caller
    QString error;        // non-empty when the document is not a readable feed
};

// RSS 2.0, RSS 1.0 (RDF) and Atom. The encoding comes from the XML declaration.
ParsedFeed parseFeed(const QByteArray &xml);
// RFC 822 dates (RSS) and ISO 8601 (Atom, Dublin Core), as UTC with whole seconds.
QDateTime parseFeedDate(const QString &text);
// Stable positive id for an article key (guid, Atom id or link): the same key gives the same id
// in every run, unlike qHash, which is seeded per process.
qint64 entryId(const QString &key);
