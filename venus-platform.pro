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

equals(QT_MAJOR_VERSION, 6): QMAKE_CXXFLAGS += -std=c++17

HEADERS = \
	src/alarm_item.hpp \
	src/alarm_monitor.hpp \
	src/application.hpp \
	src/buzzer.hpp \
	src/display_controller.hpp \
	src/led_controller.hpp \
	src/modifications_check.hpp \
	src/mqtt_bridge_registrator.hpp \
	src/network_controller.hpp \
	src/notification.hpp \
	src/notifications.hpp \
	src/relay.hpp \
	src/security_profiles.hpp \
	src/token_users.hpp \
	src/time.hpp \
	src/updater.hpp \
	src/venus_service.hpp \
	src/venus_services.hpp \
	src/vebus_backup.hpp \

SOURCES = \
	src/alarm_item.cpp \
	src/alarm_monitor.cpp \
	src/application.cpp \
	src/buzzer.cpp \
	src/display_controller.cpp \
	src/led_controller.cpp \
	src/main.cpp \
	src/modifications_check.cpp \
	src/mqtt_bridge_registrator.cpp \
	src/network_controller.cpp \
	src/notification.cpp \
	src/notifications.cpp \
	src/relay.cpp \
	src/security_profiles.cpp \
	src/time.cpp \
	src/token_users.cpp \
	src/updater.cpp \
	src/venus_service.cpp \
	src/venus_services.cpp \
	src/vebus_backup.cpp \

TRANSLATIONS = \
	translations/venus_ar.ts \
	translations/venus_cs.ts \
	translations/venus_da.ts \
	translations/venus_de.ts \
	translations/venus_es.ts \
	translations/venus_fr.ts \
	translations/venus_it.ts \
	translations/venus_nl.ts \
	translations/venus_pl.ts \
	translations/venus_ru.ts \
	translations/venus_ro.ts \
	translations/venus_sv.ts \
	translations/venus_tr.ts \
	translations/venus_zh-CN.ts \

VE_CONFIG += udev
include("ext/veutil/veutil.pri")
include("connman/connman.pri")

QMAKE_CXXFLAGS *= -ffunction-sections
QMAKE_LFLAGS *= -Wl,--gc-sections

!lessThan(QT_VERSION, 5) {
	QMAKE_CXXFLAGS += "-Wsuggest-override"
	CONFIG(debug, debug|release) {
		QMAKE_CXXFLAGS += "-Werror=suggest-override"
	}
}

gcc {
	QMAKE_CXXFLAGS += -Wno-psabi
}
