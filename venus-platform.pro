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

translations.path = $${target.path}/translations
translations.files = translations/*.qm
INSTALLS += translations

HEADERS = \
    src/alarm_item.hpp \
    src/alarm_monitor.hpp \
    src/application.hpp \
    src/buzzer.hpp \
    src/mqtt.hpp \
    src/notification.hpp \
    src/notifications.hpp \
    src/relay.hpp \
    src/time.hpp \
    src/updater.hpp \
    src/venus_service.hpp \
    src/venus_services.hpp \

SOURCES = \
    src/alarm_item.cpp \
    src/alarm_monitor.cpp \
    src/application.cpp \
    src/buzzer.cpp \
    src/main.cpp \
    src/mqtt.cpp \
    src/notification.cpp \
    src/notifications.cpp \
    src/relay.cpp \
    src/time.cpp \
    src/updater.cpp \
    src/venus_service.cpp \
    src/venus_services.cpp \

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
