TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/main.cpp

LIBS += -L/usr/local/lib -lgtest \
        -lpthread \
        -lwolfssl

HEADERS += \
    src/ip_manager_test.hpp \
    src/vpn_server_test.hpp
