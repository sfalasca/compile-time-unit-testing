#include "compile_time_ut.hpp"
using namespace CompileTimeUnitTesting;
static_assert((precondition_aggressive(false), true));
