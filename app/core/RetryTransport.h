#pragma once

#include "HttpTransport.h"

// Repeats a request that got no HTTP answer (name lookups fail for a few seconds after Wi-Fi
// comes up), up to kTries times. A request that timed out is not repeated.
class RetryTransport : public HttpTransport {
public:
    static constexpr int kTries = 4;
    explicit RetryTransport(HttpTransport &inner, int pauseMs = 3000);
    void send(const HttpRequest &request, std::function<void(const HttpResponse &)> done) override;

private:
    void attempt(const HttpRequest &request, std::function<void(const HttpResponse &)> done, int tried);

    HttpTransport &m_inner;
    int m_pauseMs;
};
