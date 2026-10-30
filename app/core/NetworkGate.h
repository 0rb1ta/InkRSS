#pragma once

#include <QList>

#include <functional>
#include <memory>

// Brings Wi-Fi up before a network call; an attempt that hangs past the timeout fails its waiters
// and the next call starts a new one.
class NetworkGate {
public:
    using Connect = std::function<void(std::function<void(bool ok)> done)>;
    // `settleMs`: after the network had to be brought up, the answer waits this long, because name
    // lookups fail for a moment after the firmware reports Wi-Fi up.
    NetworkGate(std::function<bool()> connected, Connect connect, int timeoutMs = 30000, int settleMs = 0);

    // `done` runs exactly once: true when the network is (or came) up.
    void run(std::function<void(bool ok)> done) const;

private:
    struct State {
        std::function<bool()> connected;
        Connect connect;
        int timeoutMs = 0;
        int settleMs = 0;
        QList<std::function<void(bool)>> waiters;
        int attempt = 0;
        bool pending = false;
    };
    static void finish(const std::shared_ptr<State> &s, int attempt, bool ok);

    std::shared_ptr<State> m_state;
};
