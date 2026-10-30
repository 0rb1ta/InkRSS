#pragma once

#include "MinifluxApi.h"
#include "Source.h"

#include <functional>

// Readies an article for offline reading (images, cleaned HTML); calls `done` when finished.
using PrepareFn = std::function<void(const Entry &entry, std::function<void()> done)>;
// Told which articles the store dropped, so their files can go too.
using RemovedFn = std::function<void(const QList<qint64> &ids)>;
// Whether the article's ready page is on the reader (a sync cut short may have left it out).
using ReadyFn = std::function<bool(qint64 id)>;

class MinifluxSource : public Source {
public:
    MinifluxSource(MinifluxApi &api, int limit, PrepareFn prepare = {}, RemovedFn removed = {}, ReadyFn ready = {});
    void sync(Store &store, Progress progress, std::function<void(QString error)> done) override;
    bool fullText() const override { return true; }

    // Below this many visible characters an article is taken for a teaser and the server is
    // asked to fetch the original page.
    static constexpr int kTeaserLength = 500;
    // Read articles kept on the reader besides starred ones.
    static constexpr int kKeepRead = 500;
    static constexpr int kStarredLimit = 100;
    // Full-text requests per sync: each one the server cannot serve costs many seconds.
    static constexpr int kMaxFullTextFetches = 20;
    void setLimit(int limit) { m_limit = limit; }

private:
    MinifluxApi &m_api;
    int m_limit;
    PrepareFn m_prepare;
    RemovedFn m_removed;
    ReadyFn m_ready;
};
