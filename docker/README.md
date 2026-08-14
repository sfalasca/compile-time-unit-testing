# Compiler / platform test matrix

Every file here is a standalone `docker build -f docker/<name>.Dockerfile .`
from the repository root — no build-args needed. `build-matrix.sh` builds
and reports on all (or a subset) of them.

```
docker/build-matrix.sh                       # everything
docker/build-matrix.sh gcc-13 clang-17 esp32  # a subset
docker/build-matrix.sh --list                 # list target names, do nothing
```

## Desktop compilers (gcc-\*, clang-\*)

Each image configures, builds, and runs the **full ctest suite**
(`ctest --test-dir build`) — a passing `docker build` means all 48 tests
passed on that exact compiler version. `CMAKE_CXX_STANDARD=23` is
requested everywhere; CMake caps it to whatever the compiler actually
supports (verified down to GCC 10, which gets silently capped to
`gnu++2a`). This exercises the `<format>`/`std::formattable` feature gate
in `include/compile_time_ut.hpp` both ways: present + working (GCC 13+,
Clang 17+ with libc++) and absent (older compilers — falls back to
plain, valueless messages, verified by `tests/noexcept_tests.cpp`).

| Family | Versions | Base image |
|---|---|---|
| GCC | 10, 11, 12, 13, 14, 15 | `gcc:<N>` (official) |
| Clang | 13–21 | `silkeh/clang:<N>`, built against that image's matching libc++ |

Clang images build against `-stdlib=libc++` rather than the distro's
system libstdc++, because the latter's version is tied to the base
Debian release rather than the Clang version under test and would
otherwise silently hide whether `<format>` actually works for a given
Clang.

**Known issue:** on Clang 13 (only), the two `ctut_case2_*`
tests — which grep the *compiler diagnostic text* for the literal
argument values (e.g. `7331`, `8442`) — fail even though the underlying
compile-time value-diagnostic mechanism (`CTUT_EXPECT_EQ` →
`expect_eq<A, B>()` NTTP) works correctly; Clang 13's older
template-argument pretty-printer formats the failing instantiation
differently from Clang 14+. This is a diagnostic-formatting difference,
not a functional break — verified separately that Clang 13 compiles and
runs the rest of the suite (46/48) cleanly.

## Embedded / bare-metal targets

These compile-check `tests/noexcept_tests.cpp` (exercises `expect_XXX`,
`CTUT_EXPECT_XXX`, and preconditions/postconditions/invariants, including
the genuine-runtime `-fno-exceptions` abort path) with `-c` — object
file only, **no link**. Producing a flashable image needs a linker
script, startup code, and a vendor HAL/BSP/project structure (ESP-IDF
project, STM32Cube, etc.), none of which is this header-only library's
concern; `-c` against the real cross-compiler is the standard way to
validate portability without that machinery.

| File | Target | Toolchain | Notes |
|---|---|---|---|
| `esp32.Dockerfile` | ESP32 (Xtensa) | `xtensa-esp32-elf-g++` (GCC 14.2.0) via official `espressif/idf` image | `-fno-exceptions` matches ESP-IDF's own default (`CONFIG_COMPILER_CXX_EXCEPTIONS` off) |
| `stm32.Dockerfile` | Cortex-M4 (STM32F4-class) | `arm-none-eabi-g++` 13.2.1 (apt) | Needs `libstdc++-arm-none-eabi-newlib`/`libnewlib-dev` explicitly — they're apt `Recommends`, not `Depends`, of `gcc-arm-none-eabi` |
| `riscv32.Dockerfile` | rv32imac/ilp32 (CH32V/GD32VF103-class) | xPack prebuilt `riscv-none-elf-gcc` 15.2.0 | Ubuntu's `gcc-riscv64-unknown-elf` apt package ships **no libstdc++ at all** (not even `<cstddef>` resolves) — had to switch to xPack's prebuilt tarball |

## Platforms investigated and deliberately not included

Evidence, not guesses — each was actually tried before being ruled out.

- **AVR**: not viable at all, on any toolchain build. Checked three
  independent AVR-GCC builds — Ubuntu apt (GCC 7.3.0), a same-day-fresh
  community image (`sbcr/avr-gcc`, GCC 15.2.0), and Microchip's own
  official toolchain (`martinstej/avr-gcc`, GCC 5.4.0) — and **none of
  them ship libstdc++ for the `avr` target**; `<cstddef>` doesn't resolve
  on any of them regardless of GCC version. This is an ecosystem-wide gap
  (AVR toolchains are built C-only), not a version problem, so there's no
  version bump that fixes it.
- **MSP430**: `gcc-msp430` isn't an installable Ubuntu package despite
  `apt-cache show` describing it — only `binutils-msp430`,
  `msp430mcu`, and `mspdebug` actually exist; TI ships the real compiler
  outside apt. Not wired up.
- **PIC32 (MIPS and Cortex-M/PIC32C)**: tested Microchip's current
  official compiler, MPLAB XC32 v4.60 (downloaded and silently installed
  with `--mode unattended --LicenseType FreeMode`). Both device families
  it supports (`pic32m-g++`, `pic32c-g++`) are GCC **8.3.1** — old enough
  that `std::is_constant_evaluated()` isn't in that libstdc++ at all
  (needs GCC 9+), so `include/compile_time_ut.hpp` fails to compile
  there. This is Microchip's current, latest-available release, not a
  deprecated one — there's no newer XC32 to try.
