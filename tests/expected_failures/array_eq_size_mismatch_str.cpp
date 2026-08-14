#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert([] { expect_eq("hi", "hi!"); return true; }());
