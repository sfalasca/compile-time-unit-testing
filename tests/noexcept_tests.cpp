// Built with -fno-exceptions (see tests/CMakeLists.txt): proves the
// entire compile-time expect_XXX surface compiles without exceptions
// at all.
//
// Every expect_* function is consteval, so it can only ever be invoked
// from a constant expression — there is no genuine-runtime call path
// left to exercise (attempting one, e.g. from main(), is a compile
// error, not a runtime failure). detail::fail_constant_eval's
// __cpp_exceptions-guarded branches are compiled either way regardless
// of whether either is ever reachable, since they're not templates: a
// `throw` statement is a hard compile error under -fno-exceptions
// independent of reachability, which is what this file actually
// guards against.
#include "compile_time_ut.hpp"

using namespace CompileTimeUnitTesting;

constexpr int square(int x) { return x * x; }

constexpr bool test_expect_family() {
    expect_true<true>();
    expect_false<false>();
    expect_eq<9, 9>();
    expect_ne<9, 8>();
    expect_lt<1, 2>();
    expect_le<1, 1>();
    expect_gt<2, 1>();
    expect_ge<2, 2>();
    expect_eq(val<square(3)>, val<9>);
    expect_true(val<square(3) == 9>);
    return true;
}

static_assert(test_expect_family());
