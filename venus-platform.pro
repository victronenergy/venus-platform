QT = core dbus

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
    src/velib/velib_config_app.h \

SOURCES = \
    src/application.cpp \
    src/main.cpp \

INCLUDEPATH += src
INCLUDEPATH += ext/velib/inc

HEADERS += \
    ext/velib/inc/velib/qt/daemontools_service.hpp \
    ext/velib/inc/velib/qt/v_busitems.h \
    ext/velib/inc/velib/qt/ve_qitem.hpp \
    ext/velib/inc/velib/qt/ve_qitems_dbus.hpp \

SOURCES += \
    ext/velib/src/qt/daemontools_service.cpp \
    ext/velib/src/qt/v_busitems.cpp \
    ext/velib/src/qt/ve_qitem.cpp \
    ext/velib/src/qt/ve_qitems_dbus.cpp \

*g++* {
    QMAKE_CXX += -Wno-class-memaccess -Wno-deprecated-copy
}
