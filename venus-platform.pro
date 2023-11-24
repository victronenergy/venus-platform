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

TRANSLATIONS = \
    lang/venus_ar.ts \
    lang/venus_cs.ts \
    lang/venus_da.po \
    lang/venus_de.ts \
    lang/venus_es.ts \
    lang/venus_fr.ts \
    lang/venus_it.ts \
    lang/venus_nl.ts \
    lang/venus_pl.ts \
    lang/venus_ru.ts \
    lang/venus_ro.ts \
    lang/venus_sv.ts \
    lang/venus_tr.ts \
    lang/venus_zh-CN.ts \

HEADERS = \
    src/alarmbusitem.h \
    src/alarmmonitor.h \
    src/application.hpp \
    src/buzzer.h \
    src/dbus_service.h \
    src/dbus_services.h \
    src/mqtt.hpp \
    src/mqtt_bridge_registrar.hpp \
    src/time.hpp \
    src/updater.hpp \
    /usr/include/libudev.h \
    src/notification_center.h \
    src/notification.h \
    src/relay.h \

SOURCES = \
    src/alarmbusitem.cpp \
    src/alarmmonitor.cpp \
    src/application.cpp \
    src/buzzer.cpp \
    src/dbus_service.cpp \
    src/dbus_services.cpp \
    src/main.cpp \
    src/mqtt.cpp \
    src/time.cpp \
    src/updater.cpp \
    src/notification_center.cpp \
    src/notification.cpp \
    src/relay.cpp \

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
