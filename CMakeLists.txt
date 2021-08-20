cmake_minimum_required(VERSION 3.16)
include(.cmake.conf)

project(appcontroller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Network)

qt6_add_executable(appcontroller
    main.cpp
    process.cpp
    portlist.cpp
    perfprocesshandler.cpp
)

target_link_libraries(appcontroller PUBLIC
    Qt::Network
)

install(TARGETS appcontroller
        RUNTIME DESTINATION bin
)
