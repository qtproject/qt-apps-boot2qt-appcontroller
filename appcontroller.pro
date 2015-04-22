QT-=gui
QT+=network
HEADERS=\
        process.h \
        portlist.h \
        perfprocesshandler.h

SOURCES=\
        main.cpp \
        process.cpp \
        portlist.cpp \
        perfprocesshandler.cpp

android {
    target.path = $$[INSTALL_ROOT]/system/bin
} else {
    target.path = $$[INSTALL_ROOT]/usr/bin
}
INSTALLS+=target

# Find out git hash
exists(.git) {
    unix:system(which git):HAS_GIT=TRUE
    win32:system(where git.exe):HAS_GIT=TRUE
    contains(HAS_GIT, TRUE) {
        GIT_HASH=$$system(git log -1 --format=%H)
        !system(git diff-index --quiet HEAD): GIT_HASH="$$GIT_HASH-dirty"
        GIT_VERSION=$$system(git describe --tags --exact-match)
        isEmpty(GIT_VERSION) : GIT_VERSION="unknown"
    }
} else {
    GIT_HASH="unknown"
    GIT_VERSION="unknown"
}

isEmpty(GIT_VERSION) : error("No suitable tag found")
isEmpty(GIT_HASH) : error("No hash available")

DEFINES+="GIT_HASH=\\\"$$GIT_HASH\\\""
DEFINES+="GIT_VERSION=\\\"$$GIT_VERSION\\\""
