#pragma once

#include <QString>

#include <functional>

// Everything that talks to PocketBook firmware. DevicePocketbook.cpp on the reader, DeviceStub.cpp on a PC.
namespace device {

bool isPocketbook();
// The firmware's interface language code ("ru", "en", …); empty if unknown.
QString firmwareLanguage();
QString firmwareInfo();
// Opens in the reader's own book viewer. flags: InkView OB_* values (0 = default).
void openBook(const QString &path, int flags = 0);
// Opens at a page: the firmware's reader takes a page number as the parameter; a position in its
// own CFI format, or the same with other OpenBook flags or OpenBook2, is ignored.
void openBookAt(const QString &path, const QString &parameters);
void requireFullUpdate();            // full (flashing) e-ink refresh

// Physical screen density from the firmware (Qt reports a wrong value on PocketBook); 0 if unknown.
int screenDpi();
// Height of the system status panel (clock, battery) drawn above the app window.
int panelHeight();

bool networkConnected();
// Connects to the last Wi-Fi network without dialogs; `done` runs on the GUI thread.
void connectNetworkAsync(std::function<void(bool ok)> done);
void openNetworkDialog(); // system Wi-Fi picker
// Hardware page keys (next = +1, prev = -1, back = 0); `handler` runs on the GUI thread.
void installKeyHandler(std::function<void(int key)> handler);
// Called when the app is back on screen (the firmware's EVT_FOREGROUND / EVT_SHOW), e.g. after the
// reader; Qt itself gets no event then.
void installForegroundHandler(std::function<void()> handler);
// The Back key normally leaves (minimizes) the app. While `canGoBack()` is true the key is kept
// from the firmware and only reaches the app's own key handler.
void installBackFilter(std::function<bool()> canGoBack);
// Writes the signal and a backtrace to `logPath` if the app crashes.
void installCrashLog(const QString &logPath);
enum Key { KeyPrev = -1, KeyBack = 0, KeyNext = 1 };

} // namespace device
