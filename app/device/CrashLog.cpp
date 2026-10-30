#include "Device.h"

#include <QFile>

#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

namespace {

int g_fd = -1;

// Only async-signal-safe calls here: the app is already broken when this runs.
void onCrash(int sig)
{
    if (g_fd >= 0) {
        char line[64];
        const char *name = sig == SIGSEGV ? "SIGSEGV" : sig == SIGABRT ? "SIGABRT" : sig == SIGBUS ? "SIGBUS" : sig == SIGFPE ? "SIGFPE" : "signal";
        const int n = snprintf(line, sizeof line, "==== crash: %s, backtrace:\n", name);
        if (n > 0)
            (void)!write(g_fd, line, size_t(n));
        void *frames[48];
        const int count = backtrace(frames, 48);
        backtrace_symbols_fd(frames, count, g_fd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

} // namespace

void device::installCrashLog(const QString &logPath)
{
    g_fd = open(QFile::encodeName(logPath).constData(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE})
        signal(sig, onCrash);
}
