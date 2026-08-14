# Compiler / platform test matrix

Every file here is a standalone `docker build -f docker/<name>.Dockerfile .`
from the repository root, no build-args needed. `build-matrix.sh` builds
and reports on all (or a subset) of them.

```
docker/build-matrix.sh                       # everything
docker/build-matrix.sh gcc-13 clang-17 esp32  # a subset
docker/build-matrix.sh --list                 # list target names, do nothing
```

## Desktop compilers (gcc-\*, clang-\*)

Each image configures, builds, and runs the **full ctest suite**
(`ctest --test-dir build`), so a passing `docker build` means the whole
library-behavior suite passed on that exact compiler version.
`CMAKE_CXX_STANDARD=23` is requested everywhere; CMake caps it to
whatever the compiler actually supports (down to GCC 10, which gets
silently capped to `gnu++2a`).

| Family | Versions | Base image |
|---|---|---|
| GCC | 10, 11, 12, 13, 14, 15 | `gcc:<N>` (official) |
| Clang | 13–21 | `silkeh/clang:<N>`, built against that image's matching libc++ |

Clang images build against `-stdlib=libc++` rather than the distro's
system libstdc++, because the latter's version is tied to the base
Debian release rather than the Clang version under test.

Every `expect_*` is `consteval` (see the top-level README); a failing
assertion is a compiler error at the call site, and the argument
values reach the diagnostic text on every tested compiler, GCC 10-15
and Clang 13-21 alike — verified directly via `val_case2_*`, which
greps the raw compiler output for the literal argument values.

## Embedded / bare-metal targets

These compile-check `tests/noexcept_tests.cpp` (proves the whole
`expect_XXX` surface compiles under `-fno-exceptions`) with `-c`:
object file only, **no link**. Producing a
flashable image needs a linker script, startup code, and a vendor
HAL/BSP/project structure (ESP-IDF project, STM32Cube, etc.), none of
which is this header-only library's concern. `-c` against the real
cross-compiler is the standard way to validate portability without
that machinery.

| File | Target | Toolchain | Notes |
|---|---|---|---|
| `esp32.Dockerfile` | ESP32 (Xtensa) | `xtensa-esp32-elf-g++` (GCC 14.2.0) via official `espressif/idf` image | `-fno-exceptions` matches ESP-IDF's own default (`CONFIG_COMPILER_CXX_EXCEPTIONS` off) |
| `stm32.Dockerfile` | Cortex-M4 (STM32F4-class) | `arm-none-eabi-g++` 13.2.1 (apt) | Needs `libstdc++-arm-none-eabi-newlib`/`libnewlib-dev` explicitly; they're apt `Recommends`, not `Depends`, of `gcc-arm-none-eabi` |
| `riscv32.Dockerfile` | rv32imac/ilp32 (CH32V/GD32VF103-class) | xPack prebuilt `riscv-none-elf-gcc` 15.2.0 | Ubuntu's `gcc-riscv64-unknown-elf` apt package ships **no libstdc++ at all** (not even `<cstddef>` resolves), which is why this uses xPack's prebuilt tarball instead |


