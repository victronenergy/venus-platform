QT = core dbus network

unix {
    bindir = $$(bindir)
    DESTDIR = $$(DESTDIR)
    isEmpty(bindir) {
        bindir = /opt/victronenergy/venus-platform
    }
    target.path = $${DESTDIR}$${bindir}
}
!isEmpty(target.path): INSTALLS += target

HEADERS = \
    src/application.hpp \
    src/mqtt.hpp \
    src/time.hpp \
    src/updater.hpp \

SOURCES = \
    src/application.cpp \
    src/main.cpp \
    src/time.cpp \
    src/updater.cpp \

VE_CONFIG += udev
include("ext/veutil/veutil.pri")

QMAKE_CXXFLAGS *= -ffunction-sections
QMAKE_LFLAGS *= -Wl,--gc-sections

!lessThan(QT_VERSION, 5) {
    QMAKE_CXXFLAGS += "-Wsuggest-override"
    CONFIG(debug, debug|release) {
        QMAKE_CXXFLAGS += "-Werror=suggest-override"
    }
}
