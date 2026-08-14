// Reproduces the compile-time unit test pattern from the README:
//   constexpr bool test_my_f() { CTUT_EXPECT_EQ(my_f(), ...); return true; }
//   static_assert(test_my_f());
// my_f() and the expected value are chosen to be improbable line/column
// numbers so the CMake grep check below unambiguously confirms the actual
// argument values (not incidental diagnostic text) reached the compiler.
#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;

constexpr int my_f() { return 7331; }

constexpr bool test_my_f() {
    CTUT_EXPECT_EQ(my_f(), 8442);
    return true;
}

static_assert(test_my_f());
