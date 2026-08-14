#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert([] { expect_ge("abc", "abd"); return true; }());
