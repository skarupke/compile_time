TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp

HEADERS += \
    dunique_ptr.hpp

QMAKE_CXXFLAGS += -std=c++1y

