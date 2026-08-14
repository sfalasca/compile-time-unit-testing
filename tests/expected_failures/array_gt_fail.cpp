#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert([] { expect_gt("abc", "abc"); return true; }());
