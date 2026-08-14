#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_near<1.0, 2.0, 0.001>());
