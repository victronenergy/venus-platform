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
    src/updater.hpp \
    src/velib/velib_config_app.h \

SOURCES = \
    src/application.cpp \
    src/main.cpp \
    src/updater.cpp \

INCLUDEPATH += src
INCLUDEPATH += ext/velib/inc

HEADERS += \
    ext/velib/inc/velib/qt/canbus_interfaces.hpp \
    ext/velib/inc/velib/qt/canbus_monitor.hpp \
    ext/velib/inc/velib/qt/daemontools_service.hpp \
    ext/velib/inc/velib/qt/firmware_updater_data.hpp \
    ext/velib/inc/velib/qt/q_udev.hpp \
    ext/velib/inc/velib/qt/v_busitems.h \
    ext/velib/inc/velib/qt/ve_qitem.hpp \
    ext/velib/inc/velib/qt/ve_qitems_dbus.hpp \
    ext/velib/inc/velib/qt/ve_qitem_exported_dbus_services.hpp \
    ext/velib/inc/velib/qt/ve_qitem_utils.hpp \
    ext/velib/src/qt/ve_qitem_exported_dbus_service.hpp \

SOURCES += \
    ext/velib/src/qt/canbus_interfaces.cpp \
    ext/velib/src/qt/canbus_monitor.cpp \
    ext/velib/src/qt/daemontools_service.cpp \
    ext/velib/src/qt/q_udev.cpp \
    ext/velib/src/qt/v_busitems.cpp \
    ext/velib/src/qt/ve_qitem.cpp \
    ext/velib/src/qt/ve_qitems_dbus.cpp \
    ext/velib/src/qt/ve_qitem_exported_dbus_service.cpp \
    ext/velib/src/qt/ve_qitem_exported_dbus_services.cpp \

*g++* {
    QMAKE_CXX += -Wno-class-memaccess -Wno-deprecated-copy
}

LIBS += -ludev
