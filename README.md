# compile-time-unit-testing

[Project page](https://falasca.engineering/compile-time-unit-testing/)

A single-header C++20 library for compile-time unit testing: write an
ordinary `constexpr bool test_xxx()` function using the `expect_*`
assertion family, then `static_assert(test_xxx())`. Failures are
compiler errors, at build time, with the actual argument values shown
in the diagnostic.

MIT licensed. Header-only: `#include "compile_time_ut.hpp"`, nothing to
link.

## Requirements

- **C++20 minimum.** `target_compile_features(compile_time_ut INTERFACE cxx_std_20)` in this repo's `CMakeLists.txt`.
- Tested against GCC 10–15 and Clang 13–21, plus ESP32 (Xtensa), STM32 (Cortex-M4), and RISC-V (rv32imac) bare-metal cross-compilers. See [`docker/README.md`](docker/README.md) for the full matrix and how to run it yourself. **`expect_near` needs GCC 11+ or Clang 18+** (floating-point non-type template parameters, P1907R1); it isn't declared at all below that, on any argument types, since forming its template-id is ill-formed without that support. Everything else works down to GCC 10 / Clang 13.
- Works with `-fno-exceptions` (embedded/no-exceptions builds get `abort()` instead of `throw`; see below).

## Adding this to your project

The recommended way to consume this library is CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    compile_time_ut
    GIT_REPOSITORY <this-repo-url>
    GIT_TAG        <commit-or-tag>
)
FetchContent_MakeAvailable(compile_time_ut)

target_link_libraries(your_target PRIVATE compile_time_ut)
```

`compile_time_ut` is an `INTERFACE` library, header-only with nothing to build or install, so that's the entire integration: no `install()`/`find_package()` step, no prebuilt artifact to manage, and it pins to a specific commit like any other dependency. If you'd rather not take a CMake dependency at all, vendoring `include/compile_time_ut.hpp` directly and adding it to your own include path works just as well.

## Quick start

```cpp
#include "compile_time_ut.hpp"

constexpr int square(int x) { return x * x; }

constexpr bool test_square() {
    using namespace CompileTimeUnitTesting;   // scoped to this function, not the whole file
    expect_eq<square(3), 9>();
    expect_true<(square(-2) > 0)>();
    return true;
}

static_assert(test_square());   // fails the build if any expect_* inside fails
```

## `expect_*` is compile-time-only, on purpose

`expect_true`, `expect_false`, `expect_eq`, `expect_ne`, `expect_lt`, `expect_le`, `expect_gt`, `expect_ge`, `expect_near` (epsilon-tolerant, for floats). **There is no plain `expect_eq(a, b)` overload that accepts arbitrary runtime values, and every `expect_*` is `consteval`** — calling one outside a constant expression (e.g. from ordinary runtime code) is a compile error, not something that happens to compile and just work. Every call form requires its arguments to be constant expressions:

| Form | Example |
|---|---|
| Explicit NTTP | `expect_eq<f(), 42>()` |
| `val<>`-wrapped | `expect_eq(val<f()>, val<42>)` |

This isn't an oversight, it's the only way to get argument values into a compiler diagnostic at all. A function parameter is never usable as a template argument, in any C++ context, not even inside `consteval` functions that are manifestly constant-evaluated at the call site. The only thing that works is turning the arguments into template arguments *at the call site itself*, which is exactly what the NTTP and `val<>` forms each do, and what a plain `expect_eq(const T&, const U&)` overload structurally can't.

**`val<>` is the form to reach for by default.** `expect_eq<f(), 42>()` only works when both arguments are directly nameable as template arguments; `val<>` accepts any constant expression, including one that reads a local `constexpr` variable, so it's the more general form: `expect_eq(val<f()>, val<42>)`.

A relational operator inside a `val<...>` expression needs its own parens: `val<(a < b)>`, not `val<a < b>`. The compiler otherwise reads the first bare `<`/`>` it meets as the template-argument-list delimiter, not part of the expression — `val<b > a>` fails to parse for exactly this reason. `==`/`!=` don't have this problem and don't need the extra parens.

A local variable used inside `val<>` or the NTTP form must itself be `constexpr`. An ordinary (non-`constexpr`) local, even inside a function being evaluated as a constant expression, is not usable as a template argument either:

```cpp
constexpr int n = square(3);   // must be constexpr, not just a plain local
expect_eq(val<n>, val<9>);
```

### Ranges: C arrays, `std::array`, `std::span`

`expect_eq`/`ne`/`lt`/`le`/`gt`/`ge` all have overloads for C arrays, `std::array`, and `std::span` that take the containers directly, no NTTP-wrapping needed. C arrays and `std::array` (compile-time size) get full per-index mismatch diagnostics. Internally these recursively split the index range in half until each piece is small enough to fold directly, so mismatch reporting scales to arbitrarily large arrays without hitting either `-ftemplate-depth` (one level per recursive split, `O(log2(N))`, not per element) or Clang's `-fbracket-depth` (which caps flat fold-expression size at 256 by default and would otherwise fail to compile past that on Clang specifically); a 2,000-element mismatch is a permanent regression test in `tests/basic_tests.cpp`/`tests/expected_failures/array_eq_mismatch_large.cpp`. Ordinary `-fconstexpr-steps`/`-fconstexpr-ops-limit` limits still apply for pathologically large arrays, same as any other constexpr code. `std::span` (size may be a runtime value) reports mismatches without a per-index diagnostic but still correctly fails compile-time evaluation. Containers aren't implicitly converted to `std::span` (that needs a non-deduced parameter type), so construct it explicitly: `expect_eq(std::span(v1), std::span(v2))`.

### `expect_near`

```cpp
expect_near(val<computed>, val<1.0>, val<0.0001>);  // |computed - 1.0| <= 0.0001
expect_near<10, 12, 5>();                           // NTTP form
```

## `-fno-exceptions`

Every failure path funnels through one function, `detail::fail_constant_eval()`. Calling it is what fails the enclosing `static_assert`: every `expect_*` is `consteval`, so it can only ever be invoked from a constant expression in the first place — attempting to call one at genuine runtime is a compile error, not a runtime failure, so there's no remaining call path that reaches `fail_constant_eval()`'s body at actual program runtime. It still needs an `__cpp_exceptions`-guarded `throw`/`std::abort()` split internally, because a `throw` statement is a hard compile error under `-fno-exceptions` at the point it's written, independent of whether it's ever reached — that's a build-configuration concern, not a runtime one. Cross-compiled and checked against `xtensa-esp32-elf-g++`, `arm-none-eabi-g++`, and `riscv-none-elf-gcc` with `-fno-exceptions -fno-rtti`; see `docker/{esp32,stm32,riscv32}.Dockerfile`.

## Building and testing this library

```sh
cmake -S . -B build -DCMAKE_CXX_STANDARD=23   # optional; the library only requires C++20
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`CMAKE_CXX_STANDARD` is a request, not a requirement: CMake caps it to whatever the compiler actually supports (down to GCC 10, which gets silently capped to `gnu++2a`).

Static analysis: `cmake -DCTUT_ENABLE_CLANG_TIDY=ON -DCTUT_ENABLE_CPPCHECK=ON ..` (both opt-in, auto-skip with a warning if the tool isn't installed), plus an always-registered `cmake --build build --target static_analysis` running the Clang Static Analyzer (`scan-build`) over the whole build.

Compiler/platform matrix: see [`docker/README.md`](docker/README.md). `docker/build-matrix.sh` builds every Dockerfile in `docker/` and reports pass/fail.

## Author

Written by [Stefano Falasca](https://falasca.engineering), an embedded software consultant
focused on safety-critical systems and agent-ready verification.

## License

MIT — see [`LICENSE`](LICENSE).
