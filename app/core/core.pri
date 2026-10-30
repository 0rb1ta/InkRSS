# Platform-independent core: built for the device and for host unit tests.
QT += network gui sql
INCLUDEPATH += $$PWD $$PWD/../third_party/qrcodegen
SOURCES += \
    $$PWD/Paths.cpp $$PWD/Log.cpp $$PWD/FileIO.cpp $$PWD/Models.cpp $$PWD/Store.cpp $$PWD/MinifluxApi.cpp $$PWD/MinifluxSource.cpp $$PWD/FeedParser.cpp $$PWD/Opml.cpp $$PWD/DirectSource.cpp $$PWD/ArticleHtml.cpp $$PWD/ImageCache.cpp $$PWD/Config.cpp $$PWD/Issue.cpp $$PWD/I18n.cpp $$PWD/HoldDetector.cpp \
    $$PWD/QtHttpTransport.cpp $$PWD/NetworkGate.cpp $$PWD/RetryTransport.cpp \
    $$PWD/DesktopIcon.cpp $$PWD/UpdateCheck.cpp $$PWD/Qr.cpp \
    $$PWD/../third_party/qrcodegen/qrcodegen.cpp
HEADERS += \
    $$PWD/Paths.h $$PWD/Log.h $$PWD/FileIO.h $$PWD/Models.h $$PWD/Source.h $$PWD/Store.h $$PWD/MinifluxApi.h $$PWD/MinifluxSource.h $$PWD/FeedParser.h $$PWD/Opml.h $$PWD/DirectSource.h $$PWD/ArticleHtml.h $$PWD/ImageCache.h $$PWD/Config.h $$PWD/Issue.h $$PWD/I18n.h $$PWD/HoldDetector.h \
    $$PWD/HttpTransport.h $$PWD/QtHttpTransport.h $$PWD/NetworkGate.h $$PWD/RetryTransport.h \
    $$PWD/DesktopIcon.h $$PWD/UpdateCheck.h $$PWD/Qr.h
