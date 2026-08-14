#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_ge(val<1>, val<2>));
