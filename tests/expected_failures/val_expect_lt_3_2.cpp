#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_lt(val<3>, val<2>));
