# compile-time-unit-testing

A single-header C++20 library for two related things:

1. **Lakos-style contracts** (`precondition`/`postcondition`/`invariant`) that run identically at compile time (inside `constexpr` evaluation) and at genuine runtime.
2. **Compile-time unit tests**: write an ordinary `constexpr bool test_xxx()` function using the `expect_*` assertion family, then `static_assert(test_xxx())` — failures are compiler errors, at build time, with the actual argument values shown in the diagnostic.

MIT licensed. Header-only: `#include "compile_time_ut.hpp"`, nothing to link.

## Requirements

- **C++20 minimum.** `target_compile_features(compile_time_ut INTERFACE cxx_std_20)` in this repo's `CMakeLists.txt`.
- **`<source_location>`** (GCC 11+ / Clang 17ish+ with libc++) is used opportunistically for contract-violation file:line reporting; gated behind `__has_include`, falls back cleanly without it.
- Verified against GCC 10–15 and Clang 13–21, plus ESP32 (Xtensa), STM32 (Cortex-M4), and RISC-V (rv32imac) bare-metal cross-compilers — see [`docker/README.md`](docker/README.md) for the full matrix and how to run it yourself.
- Works with `-fno-exceptions` (embedded/no-exceptions builds get `abort()` instead of `throw`; see below).

## Quick start

```cpp
#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;

constexpr int square(int x) { return x * x; }

constexpr bool test_square() {
    CTUT_EXPECT_EQ(square(3), 9);
    CTUT_EXPECT_TRUE(square(-2) > 0);
    return true;
}

static_assert(test_square());   // fails the build if test_square() returns false
```

## `expect_*` is compile-time-only, on purpose

`expect_true`, `expect_false`, `expect_eq`, `expect_ne`, `expect_lt`, `expect_le`, `expect_gt`, `expect_ge`, `expect_near` (epsilon-tolerant, for floats) — **there is no plain `expect_eq(a, b)` overload that accepts arbitrary runtime values.** Every call form requires its arguments to be constant expressions:

| Form | Example |
|---|---|
| Explicit NTTP | `expect_eq<f(), 42>()` |
| `val<>`-wrapped | `expect_eq(val<f()>, val<42>)` |
| `CTUT_EXPECT_*` macro | `CTUT_EXPECT_EQ(f(), 42)` |

This isn't an oversight — it's the only way to get argument values into a compiler diagnostic at all. A function parameter is never usable as a template argument, in any C++ context, even inside `consteval` functions manifestly constant-evaluated at the call site (verified directly against both GCC and Clang before settling on this design, not assumed). The only mechanism that works is making the arguments template arguments *at the call site itself* — which is what the NTTP, `val<>`, and macro forms each do, and what a plain `expect_eq(const T&, const U&)` overload structurally cannot.

**`CTUT_EXPECT_*` is the ergonomic default**: it's a macro that textually rewrites `CTUT_EXPECT_EQ(a, b)` into `expect_eq(val<(a)>, val<(b)>)` before the compiler ever sees a function call, so ordinary-looking test code gets full value diagnostics without hand-wrapping every argument.

Available macros: `CTUT_EXPECT_TRUE`, `CTUT_EXPECT_FALSE`, `CTUT_EXPECT_EQ`, `CTUT_EXPECT_NE`, `CTUT_EXPECT_LT`, `CTUT_EXPECT_LE`, `CTUT_EXPECT_GT`, `CTUT_EXPECT_GE`, `CTUT_EXPECT_NEAR`.

A local variable used inside one of these macros or NTTP forms must itself be `constexpr` — an ordinary (non-`constexpr`) local, even inside a function being evaluated as a constant expression, is not usable as a template argument either:

```cpp
constexpr ConstArray a("abc");   // must be constexpr, not just a plain local
constexpr ConstArray b("abd");
CTUT_EXPECT_TRUE(a < b);
```

### Ranges: C arrays, `std::array`, `std::span`

`expect_eq`/`ne`/`lt`/`le`/`gt`/`ge` all have overloads for C arrays, `std::array`, and `std::span`, taking the containers directly (not NTTP-wrapped). C arrays and `std::array` (compile-time size) get full per-index mismatch diagnostics. `std::span` (size may be a runtime value) reports mismatches without a per-index diagnostic but still correctly fails compile-time evaluation. Containers aren't implicitly converted to `std::span` — construct it explicitly: `expect_eq(std::span(v1), std::span(v2))`.

### `expect_near`

```cpp
CTUT_EXPECT_NEAR(computed, 1.0, 0.0001);  // |computed - 1.0| <= 0.0001
expect_near<10, 12, 5>();                 // NTTP form
```

## Contracts: preconditions, postconditions, invariants

Unlike `expect_*`, contracts are explicitly dual-mode — full checking at compile time (free, since a `constexpr` violation just fails the enclosing `static_assert`) and configurable checking at genuine runtime:

```cpp
constexpr int divide(int a, int b) {
    precondition(b != 0);
    int result = a / b;
    postcondition(result * b == a);
    return result;
}
```

Three severity tiers, each independently toggleable at runtime:

| Tier | Suffix | Default runtime behavior |
|---|---|---|
| `safe` | `_safe` | always on |
| `normal` | *(no suffix)* | on (this is the **default** `CTUT_ASSERTION_LEVEL`) |
| `aggressive` | `_aggressive` | off by default |

Override before including the header: `#define CTUT_ASSERTION_LEVEL 3` (or `1`, or `0` to disable everything at runtime — compile-time checking is unaffected regardless).

`testcase(cond)` is the same mechanism at the `safe` tier, meant for lightweight in-function sanity checks.

### Violation handler

```cpp
void my_handler(const char* kind, const char* file, int line) {
    log_error("{} at {}:{}", kind, file, line);
}
set_violation_handler(my_handler);
```

`file`/`line` come from `std::source_location::current()`, captured at *your* call site (not inside this header), when the toolchain has `<source_location>`; otherwise they're `""`/`0` — the handler signature never changes, so code written against it is portable regardless of which toolchain built it.

The handler is stored in a `std::atomic<ViolationHandler>` — `set_violation_handler()` and concurrent contract checks from multiple threads are race-free. Verified with `tests/thread_safety_tests.cpp` under ThreadSanitizer (both GCC's and Clang's), stressing 4 concurrent writers against 4 concurrent readers; confirmed (by temporarily reverting to a bare pointer) that TSan actually catches a regression here within the first few iterations.

### Testing contracts and exceptions: `expect_throws` / `expect_violation`

Exceptions-only (see below) — for use in ordinary runtime unit tests, not inside `constexpr`:

```cpp
check(expect_violation([] { precondition(false); }, "precondition violated"),
      "precondition(false) violates");
check(expect_no_throw([] { precondition(true); }),
      "precondition(true) passes");
check(expect_throws([] { some_throwing_call(); }));
check(expect_no_violation([] { precondition(true); }));
```

## `-fno-exceptions`

Every failure path funnels through one function, `detail::fail_at_runtime()`. At compile time, calling it is what fails the enclosing `static_assert` (calling any non-`constexpr` function is ill-formed in a constant expression — this doesn't depend on exceptions being enabled at all). At genuine runtime: `throw`s when `__cpp_exceptions` is defined (the default, catchable, as you'd expect); otherwise prints the message to `stderr` and calls `std::abort()`. Verified directly against `xtensa-esp32-elf-g++`, `arm-none-eabi-g++`, and `riscv-none-elf-gcc` with `-fno-exceptions -fno-rtti` — see `docker/{esp32,stm32,riscv32}.Dockerfile`.

`expect_throws`/`expect_no_throw`/`expect_violation`/`expect_no_violation` are contracts-only helpers, unavailable under `-fno-exceptions` (there's nothing for `try`/`catch` to do there) and compiled out entirely (`#if defined(__cpp_exceptions)`).

## Building and testing this library

```sh
cmake -S . -B build -DCMAKE_CXX_STANDARD=23   # optional; the library only requires C++20
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`CMAKE_CXX_STANDARD` is a request, not a requirement — CMake caps it to whatever the compiler actually supports (verified down to GCC 10, silently capped to `gnu++2a`).

Static analysis: `cmake -DCTUT_ENABLE_CLANG_TIDY=ON -DCTUT_ENABLE_CPPCHECK=ON ..` (both opt-in, auto-skip with a warning if the tool isn't installed), plus an always-registered `cmake --build build --target static_analysis` running the Clang Static Analyzer (`scan-build`) over the whole build.

Compiler/platform matrix: see [`docker/README.md`](docker/README.md) — `docker/build-matrix.sh` builds every Dockerfile in `docker/` and reports pass/fail.

## License

MIT — see [`LICENSE`](LICENSE).
