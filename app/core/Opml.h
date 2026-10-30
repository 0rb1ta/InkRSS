#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct OpmlFeed {
    QString title;
    QString url;
    QString folder; // top-level folder name; deeper nesting is flattened into it
};

QList<OpmlFeed> parseOpml(const QByteArray &xml);
