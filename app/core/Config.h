#pragma once

#include "MinifluxApi.h"

// Connection settings, system/inkrss/config.json. The same file can be written on a computer and
// copied over USB, so nothing has to be typed on the reader.
struct Config {
    enum Mode { None, Miniflux, Direct };
    Mode mode = None;
    MinifluxAuth auth;
    int limit = 200; // newest unread articles fetched per sync
    int fontPt = 0;  // article font size; 0 = default
    QString fontFamily; // article font; empty = default
    QString language;       // "ru", "en"; empty = as the firmware
    bool images = true;     // download and show article images
    bool flatIssue = false; // main screen as one plain list, without the lead and folder sections
    // Size of the headlines on the main screen, in steps.
    static constexpr int kHeadlineSizeMin = 1;
    static constexpr int kHeadlineSizeMax = 7;
    static constexpr int kHeadlineSizeDefault = 4;
    int headlineSize = kHeadlineSizeDefault;
};

Config loadConfig(const QString &path);
bool saveConfig(const QString &path, const Config &config);
