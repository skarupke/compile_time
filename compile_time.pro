TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    flat_map.cpp \
    await/await.cpp \
    await/boost_await.cpp \
    await/coroutine.cpp \
    await/includeOnly.cpp \
    await/stack_swap.cpp \
    await/then_future.cpp

HEADERS += \
    dunique_ptr.hpp \
    flat_map.hpp \
    await/await.h \
    await/boost_await.h \
    await/coroutine.h \
    await/function.hpp \
    await/stack_swap.h \
    await/then_future.h

QMAKE_CXXFLAGS += -std=c++1y

LIBS += -lgtest
LIBS += -lboost_context
LIBS += -lboost_thread
LIBS += -lboost_system
LIBS += -lpthread

OTHER_FILES += \
    await/stack_swap_asm.asm
