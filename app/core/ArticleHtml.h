#pragma once

#include "MinifluxSource.h"
#include "Models.h"

#include <QSize>
#include <QUrl>

#include <functional>

class ImageCache;

struct CleanArticle {
    QString html;        // safe subset: no scripts, frames, forms, styles or event handlers
    QList<QUrl> images;  // absolute image addresses in document order
};

// Prepares article HTML for the reader's own text view: drops what it cannot or must not show,
// makes links and images absolute and picks the real image from lazy-loading attributes.
CleanArticle cleanHtml(const QString &html, const QUrl &base);
// Points images at their downloaded copies; images that were not downloaded are dropped.
QString withLocalImages(const QString &cleanHtml, const ImageCache &cache);

// Turns HTML5 containers the text engine does not know (<figure>, <section>…) into blocks it does:
// otherwise an image in a <figure> joins the line before it.
QString blockLayout(const QString &html);
// Keeps the teaser's lead image only when the extracted full text has no images of its own.
QString keepLeadImage(const QString &teaserHtml, const QString &fullHtml);

QString articlePath(const QString &articlesDir, qint64 id);
constexpr int kMaxImageAttempts = 3;
bool articleReady(const QString &articlesDir, qint64 id);
void removeArticleFiles(const QString &articlesDir, const QList<qint64> &ids);
// Deletes the images in `imagesDir` that no ready article in `articlesDir` refers to; returns how
// many were deleted.
int removeUnusedImages(const QString &articlesDir, const QString &imagesDir);
int removeUnusedImages(const QList<QString> &articlesDirs, const QString &imagesDir);
// Gives images an explicit size within the text column and `maxHeight`, keeping proportions
// (a tall picture would push most of a page to the next one). Unknown sizes are left alone.
QString fitImages(const QString &html, int maxWidth, int maxHeight,
                  const std::function<QSize(const QString &src)> &sizeOf);
// Drops every image (the reader turned images off).
QString withoutImages(const QString &html);
QString articleStyleSheet();
int articleImageMaxHeight(int pageHeight);
// Cleans the article, downloads its images (unless `images` is false: then none are fetched or
// kept) and writes the ready page to articlesDir.
PrepareFn makePrepare(ImageCache &cache, const QString &articlesDir, bool images = true);
