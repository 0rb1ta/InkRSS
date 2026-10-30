#include "NetworkGate.h"

#include <QTimer>

NetworkGate::NetworkGate(std::function<bool()> connected, Connect connect, int timeoutMs, int settleMs)
    : m_state(std::make_shared<State>())
{
    m_state->connected = std::move(connected);
    m_state->connect = std::move(connect);
    m_state->timeoutMs = timeoutMs;
    m_state->settleMs = settleMs;
}

void NetworkGate::run(std::function<void(bool)> done) const
{
    const std::shared_ptr<State> s = m_state;
    if (s->connected()) {
        done(true);
        return;
    }
    s->waiters << std::move(done);
    if (s->pending)
        return;
    s->pending = true;
    const int attempt = ++s->attempt;
    std::weak_ptr<State> weak = s;
    QTimer::singleShot(s->timeoutMs, [weak, attempt] {
        if (auto st = weak.lock())
            finish(st, attempt, false);
    });
    s->connect([weak, attempt](bool ok) {
        if (auto st = weak.lock())
            finish(st, attempt, ok);
    });
}

void NetworkGate::finish(const std::shared_ptr<State> &s, int attempt, bool ok)
{
    if (attempt != s->attempt || !s->pending)
        return; // an attempt that already timed out, or a second answer
    s->pending = false;
    const auto waiters = s->waiters;
    s->waiters.clear();
    auto answer = [waiters, ok] {
        for (const auto &done : waiters)
            done(ok);
    };
    if (ok && s->settleMs > 0)
        QTimer::singleShot(s->settleMs, answer);
    else
        answer();
}
