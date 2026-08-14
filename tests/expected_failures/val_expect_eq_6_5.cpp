#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_eq(val<6>, val<5>));
