#include "compile_time_ut.hpp"

#include <array>
#include <span>

using namespace CompileTimeUnitTesting;

// --- NTTP versions ---

static_assert(expect_true<true>());
static_assert(expect_true<1>());
static_assert(expect_true<42>());

static_assert(expect_false<false>());
static_assert(expect_false<0>());

static_assert(expect_eq<1, 1>());
static_assert(expect_eq<'a', 'a'>());

static_assert(expect_ne<1, 2>());
static_assert(expect_ne<'a', 'b'>());

static_assert(expect_lt<1, 2>());
static_assert(expect_lt<'a', 'b'>());

static_assert(expect_le<1, 1>());
static_assert(expect_le<1, 2>());

static_assert(expect_gt<2, 1>());
static_assert(expect_gt<'b', 'a'>());

static_assert(expect_ge<2, 2>());
static_assert(expect_ge<3, 1>());

// --- Array versions ---

constexpr bool test_array_eq_strings() {
    expect_eq("hello", "hello");
    expect_eq("", "");
    expect_eq("hello world!", "hello world!");
    return true;
}

constexpr bool test_array_eq_int() {
    constexpr int a[] = {1, 2, 3};
    constexpr int b[] = {1, 2, 3};
    expect_eq(a, b);
    return true;
}

constexpr bool test_array_eq_double() {
    constexpr double a[] = {1., 2., 3.};
    constexpr double b[] = {1., 2., 3.};
    expect_eq(a, b);
    return true;
}

constexpr bool test_array_ne() {
    expect_ne("hello", "world");
    expect_ne("hello", "hell");
    constexpr int a[] = {1, 2, 3};
    constexpr int b[] = {1, 2, 4};
    expect_ne(a, b);
    return true;
}

constexpr bool test_array_lt() {
    expect_lt("abc", "abd");
    expect_lt("abc", "abcd");
    constexpr int a[] = {1, 2, 3};
    constexpr int b[] = {1, 2, 4};
    expect_lt(a, b);
    return true;
}

constexpr bool test_array_le() {
    expect_le("abc", "abc");
    expect_le("abc", "abd");
    return true;
}

constexpr bool test_array_gt() {
    expect_gt("abd", "abc");
    expect_gt("abcd", "abc");
    constexpr int a[] = {1, 2, 4};
    constexpr int b[] = {1, 2, 3};
    expect_gt(a, b);
    return true;
}

constexpr bool test_array_ge() {
    expect_ge("abc", "abc");
    expect_ge("abd", "abc");
    return true;
}

static_assert(test_array_eq_strings());
static_assert(test_array_eq_int());
static_assert(test_array_eq_double());
static_assert(test_array_ne());
static_assert(test_array_lt());
static_assert(test_array_le());
static_assert(test_array_gt());
static_assert(test_array_ge());

// --- Large-array regression test ---
// detail::ptr_eq_check is a fold expression over an index pack, not a
// per-element template recursion, specifically so it doesn't consume
// one level of template instantiation depth per element (that would
// cap out around a few hundred elements under default compiler
// settings). This guards against that regressing back to recursion:
// 2000 elements would fail to compile under GCC/Clang's default
// -ftemplate-depth (900/1024) if it ever did.
//
// expect_eq is consteval, so its arguments must be usable in a
// constant expression at the call site itself — a plain (non-constexpr)
// local declared and then mutated in a loop doesn't qualify, even
// inside an already constant-evaluated function. make_range<N>()
// computes the whole array as an ordinary local inside its own body
// (fine, since that local isn't the one crossing the consteval call
// boundary) and returns it by value, so the caller can bind the result
// to a genuinely constexpr local.

template <std::size_t N>
constexpr std::array<int, N> make_range() {
    std::array<int, N> r{};
    for (std::size_t i = 0; i < N; ++i) r[i] = static_cast<int>(i);
    return r;
}

constexpr bool test_large_array_eq() {
    constexpr auto a = make_range<2000>();
    constexpr auto b = make_range<2000>();
    expect_eq(a, b);
    return true;
}

static_assert(test_large_array_eq());

// --- val<> wrapper (values printed in diagnostics, no macros) ---

constexpr int times2(int x) { return x * 2; }
constexpr int add(int a, int b) { return a + b; }

constexpr bool test_val_wrapper() {
    expect_true(val<times2(1)>);
    expect_false(val<times2(0)>);
    expect_eq(val<times2(3)>, val<6>);
    expect_ne(val<times2(3)>, val<5>);
    expect_lt(val<times2(1)>, val<3>);
    expect_le(val<times2(1)>, val<2>);
    expect_gt(val<times2(3)>, val<5>);
    expect_ge(val<times2(3)>, val<6>);
    expect_eq(val<add(1, 2)>, val<3>);
    return true;
}

static_assert(test_val_wrapper());

// --- val<> with arbitrary compile-time-evaluable expressions ---
// Values from any constant expression (not just literals) are shown in
// the compiler diagnostic on failure.

constexpr int square(int x) { return x * x; }

constexpr bool test_val_wrapper_expressions() {
    expect_true(val<square(3) == 9>);
    expect_false(val<square(3) == 8>);
    expect_eq(val<square(3)>, val<9>);
    expect_ne(val<square(3)>, val<8>);
    expect_lt(val<square(2)>, val<square(3)>);
    expect_le(val<square(3)>, val<square(3)>);
    expect_gt(val<square(3)>, val<square(2)>);
    expect_ge(val<square(3)>, val<square(3)>);
    return true;
}

static_assert(test_val_wrapper_expressions());

// --- expect_near: epsilon-tolerant comparison ---
// expect_near itself only exists when CTUT_HAS_FLOAT_NTTP (GCC 11+ /
// Clang 18+); on older toolchains forming its auto-NTTP template-id is
// ill-formed even for all-integer arguments, so there's nothing to
// test — see include/compile_time_ut.hpp.
#if CTUT_HAS_FLOAT_NTTP

constexpr bool test_expect_near() {
    expect_near<10, 12, 5>();
    expect_near(val<10>, val<12>, val<5>);
    expect_near(val<square(3)>, val<10>, val<2>);

    // Floating-point operands: the documented primary use case.
    expect_near<1.0001, 1.0, 0.001>();
    expect_near(val<1.0001>, val<1.0>, val<0.001>);
    // (A < B) ordering: exercises the (B - A) branch of the ternary.
    expect_near(val<-1.0002>, val<-1.0>, val<0.001>);
    // Exact-boundary case: diff == eps == 0.
    expect_near(val<3.0>, val<3.0>, val<0.0>);
    return true;
}

static_assert(test_expect_near());

#endif // CTUT_HAS_FLOAT_NTTP

// --- Range comparisons: std::array, std::span ---

constexpr bool test_std_array() {
    constexpr std::array<int, 3> a{1, 2, 3};
    constexpr std::array<int, 3> b{1, 2, 3};
    constexpr std::array<int, 3> c{1, 2, 4};
    expect_eq(a, b);
    expect_ne(a, c);
    expect_lt(a, c);
    expect_le(a, b);
    expect_gt(c, a);
    expect_ge(b, a);
    return true;
}

static_assert(test_std_array());

constexpr bool test_std_span() {
    constexpr int arr1[3] = {1, 2, 3};
    constexpr int arr2[3] = {1, 2, 3};
    constexpr int arr3[3] = {1, 2, 4};
    expect_eq(std::span(arr1), std::span(arr2));
    expect_ne(std::span(arr1), std::span(arr3));
    expect_lt(std::span(arr1), std::span(arr3));
    expect_le(std::span(arr1), std::span(arr2));
    expect_gt(std::span(arr3), std::span(arr1));
    expect_ge(std::span(arr2), std::span(arr1));
    return true;
}

static_assert(test_std_span());
