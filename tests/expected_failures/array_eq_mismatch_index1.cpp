#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert([] { int a[]{1, 9, 3}; int b[]{1, 2, 3}; expect_eq(a, b); return true; }());
