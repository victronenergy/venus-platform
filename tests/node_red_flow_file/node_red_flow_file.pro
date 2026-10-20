QT = core testlib
CONFIG += c++17 testcase
TARGET = tst_node_red_flow_file

INCLUDEPATH += ../../src

SOURCES += \
    tst_node_red_flow_file.cpp \
    ../../src/node_red_flow_file.cpp

HEADERS += \
    ../../src/node_red_flow_file.hpp
