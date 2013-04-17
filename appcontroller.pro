QT-=gui
QT+=network
HEADERS=\
        process.h \
        portlist.h \

SOURCES=\
        main.cpp \
        process.cpp \
        portlist.cpp \

target.path = $$[INSTALL_ROOT]/system/bin
INSTALLS+=target
