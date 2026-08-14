#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_near(val<10>, val<20>, val<5>));
