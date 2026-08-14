#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;

constexpr int my_f() { return 6217; }

constexpr bool test_my_f() {
    CTUT_EXPECT_GE(my_f(), 9999);
    return true;
}

static_assert(test_my_f());
