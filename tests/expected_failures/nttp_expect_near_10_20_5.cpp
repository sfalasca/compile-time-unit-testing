#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_near<10, 20, 5>());
