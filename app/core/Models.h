#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include <functional>

struct Category {
    qint64 id = 0;
    QString title;
};

struct Feed {
    qint64 id = 0;
    qint64 categoryId = 0;
    QString title;
    QString url;
    QString error; // last update failure, empty when fine
};

struct Entry {
    qint64 id = 0;
    qint64 feedId = 0;
    QString title;
    QString url;
    QString author;
    QDateTime published;
    QString html; // as received from the source
    bool read = false;
    bool starred = false;
    QDateTime changed; // last status change, UTC, whole seconds
    int readingMinutes = 0;
};

// A status change made on the reader that the source has not been told about yet.
struct StatusChange {
    enum Kind { Read, Unread, Star, Unstar, AllRead };
    qint64 entryId = 0;
    Kind kind = Read;
    qint64 feedId = -1;     // AllRead: the feed, or -1
    qint64 categoryId = -1; // AllRead: the folder, or -1
    QDateTime at;
};

// Length of the text a reader would see in an HTML fragment (tags, scripts and styles dropped,
// entities decoded, whitespace runs counted once). Used to tell a teaser from a full article.
int textLength(const QString &html);
// Estimated reading time: 200 words a minute, rounded up; 0 for an empty article.
int readingMinutes(const QString &html);
// The visible text itself, with the same rules.
QString plainText(const QString &html);
