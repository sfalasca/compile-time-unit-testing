#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert(expect_ne(val<3>, val<3>));
