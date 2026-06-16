QT       += core gui widgets

TARGET = RTMon
TEMPLATE = app

CONFIG += c++20

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/latencyworker.cpp \
    src/latencygraph.cpp \
    src/latencyanalyzer.cpp

HEADERS += \
    src/mainwindow.h \
    src/latencyworker.h \
    src/latencygraph.h \
    src/latencyanalyzer.h

RESOURCES += \
    resources/resources.qrc
