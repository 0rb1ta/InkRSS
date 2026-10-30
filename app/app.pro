# InkRSS: RSS reader with Miniflux support for PocketBook.
# Device build: app/package/build-device.sh. Host build (stub device layer) works with the SDK's x86 Qt.
QT += widgets network svg
TARGET = inkrss

include(common.pri)
include(core/core.pri)
include(device/device.pri)

INCLUDEPATH += $$PWD/ui $$PWD/ui/kit $$PWD/ui/screens
SOURCES += main.cpp ui/AppController.cpp \
    ui/kit/Theme.cpp ui/kit/Widgets.cpp ui/kit/RowPager.cpp ui/kit/Shots.cpp \
    ui/screens/ConnectScreen.cpp ui/screens/IssueScreen.cpp ui/screens/ArticleScreen.cpp \
    ui/screens/FeedsScreen.cpp ui/screens/SettingsScreen.cpp
HEADERS += ui/AppController.h \
    ui/kit/Theme.h ui/kit/Widgets.h ui/kit/RowPager.h ui/kit/Shots.h \
    ui/screens/ConnectScreen.h ui/screens/IssueScreen.h ui/screens/ArticleScreen.h \
    ui/screens/FeedsScreen.h ui/screens/SettingsScreen.h
RESOURCES += resources.qrc
