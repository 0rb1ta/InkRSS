#pragma once

#include <QString>

#include <functional>

class Store;

// Where articles come from: a Miniflux server or feeds fetched directly. Screens only talk to
// this interface and to Store, so another backend can be added without touching them.
class Source {
public:
    virtual ~Source() = default;
    using Progress = std::function<void(int done, int total)>;
    // Sends queued status changes, then downloads new articles into `store`.
    // `done` gets an empty string on success, otherwise a message for the screen.
    virtual void sync(Store &store, Progress progress, std::function<void(QString error)> done) = 0;
    virtual bool fullText() const = 0; // whether articles come complete, not as teasers
};
