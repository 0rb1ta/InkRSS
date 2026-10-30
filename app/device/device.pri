# PocketBook firmware glue on ARM, a logging stub on the host.
INCLUDEPATH += $$PWD
HEADERS += $$PWD/Device.h
SOURCES += $$PWD/CrashLog.cpp

PB_XSPEC = $$[QMAKE_XSPEC]
contains(PB_XSPEC, linux-arm.*) {
    SOURCES += $$PWD/DevicePocketbook.cpp
    INCLUDEPATH += $$[QT_SYSROOT]/usr/local/include
    LIBS += -L$$[QT_SYSROOT]/usr/local/lib -linkview -lcommon_utilities
    # Libraries live in /ebrmain/lib on the reader, not in the SDK path qmake bakes in.
    CONFIG += no_qt_rpath
    QMAKE_RPATHDIR =
    QMAKE_LFLAGS += -Wl,-rpath,/ebrmain/lib:/usr/lib:/usr/local/lib
} else {
    SOURCES += $$PWD/DeviceStub.cpp
}
