QT-=gui
QT+=network
HEADERS=\
        process.h \
        portlist.h \

SOURCES=\
        main.cpp \
        process.cpp \
        portlist.cpp \

android {
    target.path = $$[INSTALL_ROOT]/system/bin
} else {
    target.path = $$[INSTALL_ROOT]/usr/bin
}
INSTALLS+=target
