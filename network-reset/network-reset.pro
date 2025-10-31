QT = core dbus

unix {
	bindir = $$(bindir)
	isEmpty(bindir) {
		bindir = /opt/victronenergy/network-reset
	}
	target.path = $${bindir}
}
!isEmpty(target.path): INSTALLS += target

QMAKE_CXXFLAGS += -std=c++17

HEADERS = \
	../src/utils.hpp \
	network_reset.hpp \
	main.hpp \

SOURCES = \
	../src/utils.cpp \
	network_reset.cpp \
	main.cpp \

INCLUDEPATH += $$PWD .. ../src

include("../ext/veutil/veutil.pri")

QMAKE_CXXFLAGS *= -ffunction-sections
QMAKE_LFLAGS *= -Wl,--gc-sections

QMAKE_CXXFLAGS += "-Wsuggest-override"
CONFIG(debug, debug|release) {
	QMAKE_CXXFLAGS += "-Werror=suggest-override"
}

gcc {
	QMAKE_CXXFLAGS += -Wno-psabi
}
