TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

win32-g++ {
   QMAKE_CXXFLAGS += -Werror
}
win32-msvc*{
   QMAKE_CXXFLAGS += /WX
}

SOURCES += src/main.cpp \
    src/vpn_server.cpp \
    src/tunnel_mgr.cpp \
    src/ip_manager.cpp \
    src/utils/utils.cpp \
    src/tunnel.cpp

HEADERS += \
    src/ip_manager.hpp \
    src/vpn_server.hpp \
    src/client_parameters.hpp \
    src/tunnel_mgr.hpp \
    src/utils/utils.hpp \
    src/tunnel.hpp

LIBS += -lpthread \
        -lwolfssl \

DISTFILES += \
    other.txt
