#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert((invariant_safe(false), true));
