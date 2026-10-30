#pragma once

#include "Models.h"

#include <QDateTime>

// One line of the main screen, which reads like a morning paper: a lead article, then the
// folders as sections with their headlines.
struct IssueItem {
    enum Kind { Lead, Section, Headline };
    Kind kind = Headline;
    qint64 entryId = 0;     // Lead, Headline
    qint64 categoryId = -1; // Section: the folder (0 = feeds without a folder)
    qint64 feedId = -1;     // Section: the feed, when feeds stand in for missing folders
    QString title;          // Section
    int count = 0;          // Section: articles in it
};

// `entries` newest first. Lead: the newest article of at least kLeadMinutes (else the newest).
// Sections follow the folders; with one folder or none the feeds become the sections.
enum class IssueLayout { Sections, Flat };
// Flat: every article as a headline, newest first, no lead and no sections.
QList<IssueItem> buildIssue(const QList<Entry> &entries, const QList<Feed> &feeds, const QList<Category> &categories,
                           IssueLayout layout = IssueLayout::Sections);
constexpr int kLeadMinutes = 3;

// "N + 1 · 3 мин": the feed and the reading time next to a headline; either part may be missing.
QString headlineMeta(const QString &feedTitle, int minutes);
// The text of the first non-empty paragraph (or the start of the text when there are none), cut at
// a word boundary with "…" to at most `maxChars`. Bylines and captions before it are skipped.
QString leadExcerpt(const QString &html, int maxChars);

// The bottom line of the issue: "08:12 · 57 новых" on the left, the total reading time on the right.
struct IssueFooter {
    QString left;
    QString right;
};
IssueFooter issueFooter(qint64 lastSyncSecs, int unread, int minutes, const QDateTime &now);

// "≈ 1 ч 40 мин"; empty for 0.
QString formatMinutes(int minutes);

// First item index of each page when items of these heights are laid out top to bottom.
QList<int> pageStarts(const QList<int> &heights, int pageHeight);
int articlePageCount(int documentHeight, int pageHeight);
int articlePageTop(int page, int pageHeight);
