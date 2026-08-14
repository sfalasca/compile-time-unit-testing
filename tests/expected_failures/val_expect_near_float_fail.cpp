#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_near(val<1.0>, val<2.0>, val<0.001>));
