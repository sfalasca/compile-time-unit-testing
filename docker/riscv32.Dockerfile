# syntax=docker/dockerfile:1
# Embedded target: bare-metal RISC-V (rv32imac/ilp32 — e.g. CH32V,
# GD32VF103-class MCUs), via the xPack prebuilt riscv-none-elf-gcc.
#
# Ubuntu's apt-installable gcc-riscv64-unknown-elf package ships no C++
# standard library at all (not even <cstddef> resolves) — it's a bare C
# cross-compiler despite having a g++ binary. The xPack prebuilt tarball
# (built from real GCC releases with newlib + libstdc++) does provide one
# and is what this Dockerfile uses instead.
#
# Header-only library: this compiles (-c, no link) rather than producing
# a flashable image. Linking real firmware needs a linker script, startup
# code, and vendor HAL/BSP, none of which is this library's concern; -c
# is the standard way to validate a header against a cross-compiler.
#
# Build: `docker build -f docker/riscv32.Dockerfile -t ctut-riscv32 .`
FROM ubuntu:24.04

ARG XPACK_RISCV_VERSION=15.2.0-1

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL \
      "https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v${XPACK_RISCV_VERSION}/xpack-riscv-none-elf-gcc-${XPACK_RISCV_VERSION}-linux-x64.tar.gz" \
      -o /tmp/riscv-gcc.tar.gz \
    && mkdir -p /opt/riscv-none-elf-gcc \
    && tar xzf /tmp/riscv-gcc.tar.gz -C /opt/riscv-none-elf-gcc --strip-components=1 \
    && rm /tmp/riscv-gcc.tar.gz
ENV PATH="/opt/riscv-none-elf-gcc/bin:${PATH}"

WORKDIR /src
COPY include/ include/
COPY tests/noexcept_tests.cpp tests/noexcept_tests.cpp

# rv32imac/ilp32: base integer + multiply/divide + atomics + compressed
# instructions, the common baseline for real rv32 MCUs.
RUN riscv-none-elf-g++ --version \
    && riscv-none-elf-g++ -std=c++23 -march=rv32imac -mabi=ilp32 \
         -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic \
         -Iinclude -c tests/noexcept_tests.cpp -o /tmp/noexcept_tests.riscv32.o
