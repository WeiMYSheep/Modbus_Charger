QT += core

CONFIG += c++17 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ProtocolSelfTest

INCLUDEPATH += src

SOURCES += \
    tests/protocol_selftest.cpp \
    src/core/crc16.cpp \
    src/core/alarmpolicy.cpp \
    src/core/chargingsession.cpp \
    src/core/modbusframe.cpp \
    src/core/registermap.cpp \
    src/core/batterysimulator.cpp \
    src/services/collectorservice.cpp \
    src/services/controllerservice.cpp \
    src/services/exportservice.cpp \
    src/services/modbuslabservice.cpp \
    src/transport/virtualbus.cpp

HEADERS += \
    src/core/crc16.h \
    src/core/alarmpolicy.h \
    src/core/chargingsession.h \
    src/core/modbusframe.h \
    src/core/registermap.h \
    src/core/batterysimulator.h \
    src/services/collectorservice.h \
    src/services/controllerservice.h \
    src/services/exportservice.h \
    src/services/modbuslabservice.h \
    src/transport/virtualbus.h
