TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    flat_map.cpp

HEADERS += \
    dunique_ptr.hpp \
    flat_map.hpp

QMAKE_CXXFLAGS += -std=c++1y

LIBS += -lgtest
LIBS += -lboost_context
LIBS += -lboost_thread
LIBS += -lboost_system
