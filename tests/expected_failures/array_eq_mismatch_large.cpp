// Companion to the large-array regression test in basic_tests.cpp: a
// mismatch deep inside a large array (past any small-N coincidence)
// must still report the correct index through the fold-expression
// rewrite of detail::ptr_eq_check, not just compile.
//
// expect_eq is consteval, so its arguments must be genuinely constexpr
// locals at the call site — built via factory functions rather than
// declared-then-mutated-in-a-loop, same as basic_tests.cpp.
#include "compile_time_ut.hpp"

#include <array>
#include <cstddef>

using namespace CompileTimeUnitTesting;

template <std::size_t N>
constexpr std::array<int, N> make_range() {
    std::array<int, N> r{};
    for (std::size_t i = 0; i < N; ++i) r[i] = static_cast<int>(i);
    return r;
}

template <std::size_t N>
constexpr std::array<int, N> make_mismatched_range(std::size_t bad_index) {
    auto r = make_range<N>();
    r[bad_index] = -1;
    return r;
}

static_assert([] {
    constexpr auto a = make_range<1024>();
    constexpr auto b = make_mismatched_range<1024>(999);
    expect_eq(a, b);
    return true;
}());
