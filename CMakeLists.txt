cmake_minimum_required(VERSION 3.18.4)
project(exporter485 C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)
pkg_check_modules(LIBYAML REQUIRED IMPORTED_TARGET libcyaml)
pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

add_executable(exporter485 main.c modules.c exporter485.h http.c collect.c)
target_link_libraries(exporter485 PUBLIC
        PkgConfig::LIBEVENT
        PkgConfig::LIBYAML
        PkgConfig::LIBMODBUS)
