TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/main.cpp

LIBS += -L/usr/local/lib -lgtest \
        -lpthread \
        -lwolfssl

HEADERS += \
    src/tunnel_mgr_test.hpp \
    src/ip_manager_test.hpp
