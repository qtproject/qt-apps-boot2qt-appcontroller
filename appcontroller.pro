QT-=gui
QT+=network
HEADERS=\
        process.h \

SOURCES=\
        main.cpp \
        process.cpp \

target.path = $$[INSTALL_ROOT]/system/bin
INSTALLS+=target
