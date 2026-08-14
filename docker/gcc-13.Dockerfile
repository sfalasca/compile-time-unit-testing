# syntax=docker/dockerfile:1
# Compiler matrix: GCC 13 (bookworm)
#
# Standalone image: `docker build -f docker/gcc-13.Dockerfile -t ctut-gcc13 .`
# from the repository root. Configures, builds, and runs the full ctest
# suite. CMAKE_CXX_STANDARD=23 is a *request*: CMake caps it to whatever
# this GCC actually supports, exercising the <format>/std::formattable
# feature gate in include/compile_time_ut.hpp either way (see there).
FROM gcc:13

# The base image's apt-provided cmake is frequently older than this
# project's cmake_minimum_required(3.20); install a current one via pip
# instead. --break-system-packages is needed on newer (PEP 668) pip and
# unrecognized by older pip, hence the fallback.
RUN apt-get update && apt-get install -y --no-install-recommends python3-pip ninja-build \
    && rm -rf /var/lib/apt/lists/* \
    && (pip3 install --no-cache-dir --break-system-packages cmake \
        || pip3 install --no-cache-dir cmake)

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja -DCMAKE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Debug \
    && cmake --build build -j \
    && ctest --test-dir build --output-on-failure
