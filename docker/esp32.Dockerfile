# syntax=docker/dockerfile:1
# Embedded target: ESP32 (Xtensa), via the official Espressif ESP-IDF
# toolchain image (which bundles xtensa-esp32-elf-g++).
#
# Header-only library: this compiles (-c, no link) rather than building a
# full flashable ESP-IDF app image. A real firmware build needs an
# idf.py project (sdkconfig, component registration, partition table),
# none of which is this library's concern; -c is the standard way to
# validate a header against a cross-compiler.
#
# ESP-IDF firmware conventionally disables C++ exceptions
# (CONFIG_COMPILER_CXX_EXCEPTIONS is off by default) to save flash, so
# this is exactly the -fno-exceptions path exercised elsewhere in the
# matrix (see include/compile_time_ut.hpp's detail::fail_at_runtime()).
#
# Build: `docker build -f docker/esp32.Dockerfile -t ctut-esp32 .`
FROM espressif/idf:release-v5.4

SHELL ["/bin/bash", "-c"]
WORKDIR /src
COPY include/ include/
COPY tests/noexcept_tests.cpp tests/noexcept_tests.cpp

RUN . /opt/esp/idf/export.sh > /dev/null \
    && xtensa-esp32-elf-g++ --version \
    && xtensa-esp32-elf-g++ -std=c++23 -fno-exceptions -fno-rtti \
         -Wall -Wextra -Wpedantic \
         -Iinclude -c tests/noexcept_tests.cpp -o /tmp/noexcept_tests.esp32.o
