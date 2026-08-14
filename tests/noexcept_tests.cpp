// Built with -fno-exceptions (see tests/CMakeLists.txt): proves the
// entire compile-time surface (expect_XXX, CTUT_EXPECT_XXX, and
// preconditions/postconditions/invariants) compiles without exceptions
// at all, and that a genuine runtime failure reports and aborts instead
// of relying on throw/catch.
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
    CTUT_EXPECT_TRUE(square(3) == 9);
    CTUT_EXPECT_EQ(square(3), 9);
    return true;
}

static_assert(test_expect_family());

constexpr bool test_contracts() {
    precondition_safe(true);
    precondition(true);
    precondition_aggressive(true);
    postcondition_safe(true);
    postcondition(true);
    postcondition_aggressive(true);
    invariant_safe(true);
    invariant(true);
    invariant_aggressive(true);
    testcase(true);
    return true;
}

static_assert(test_contracts());

int main() {
    // Deliberately fails at genuine runtime: CTUT_EXPECT_EQ's arguments
    // (square(3), 8) only need to be constant expressions to determine
    // the NTTP<9, 8> instantiation of expect_eq<A, B>() — the resulting
    // call itself still executes as ordinary runtime code here, since
    // main() is not constant-evaluated. Without exceptions available,
    // fail_at_runtime() prints the message to stderr and calls
    // std::abort() — verified by the test harness via a regex match on
    // this process's output.
    CTUT_EXPECT_EQ(square(3), 8);
    return 0;
}
