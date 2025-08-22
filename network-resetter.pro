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


QMAKE_CXXFLAGS += -std=c++17

HEADERS = \
	src/networkresetter.h \
	src/network-resetter-app.h \
	src/utils.hpp \

SOURCES = \
        src/network-resetter-main.cpp \
	src/networkresetter.cpp \
	src/network-resetter-app.cpp \
	src/utils.cpp \



unix {
    CONFIG += link_pkgconfig
}


VE_CONFIG += udev
include("ext/veutil/veutil.pri")

QMAKE_CXXFLAGS *= -ffunction-sections
QMAKE_LFLAGS *= -Wl,--gc-sections

QMAKE_CXXFLAGS += "-Wsuggest-override"
CONFIG(debug, debug|release) {
	QMAKE_CXXFLAGS += "-Werror=suggest-override"
}

gcc {
	QMAKE_CXXFLAGS += -Wno-psabi
}
