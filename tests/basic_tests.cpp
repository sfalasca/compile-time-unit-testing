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
    int a[] = {1, 2, 3};
    int b[] = {1, 2, 3};
    expect_eq(a, b);
    return true;
}

constexpr bool test_array_eq_double() {
    double a[] = {1., 2., 3.};
    double b[] = {1., 2., 3.};
    expect_eq(a, b);
    return true;
}

constexpr bool test_array_ne() {
    expect_ne("hello", "world");
    expect_ne("hello", "hell");
    int a[] = {1, 2, 3};
    int b[] = {1, 2, 4};
    expect_ne(a, b);
    return true;
}

constexpr bool test_array_lt() {
    expect_lt("abc", "abd");
    expect_lt("abc", "abcd");
    int a[] = {1, 2, 3};
    int b[] = {1, 2, 4};
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
    int a[] = {1, 2, 4};
    int b[] = {1, 2, 3};
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

// --- ConstArray wrapper ---

constexpr bool test_const_array_eq() {
    constexpr ConstArray a("hello");
    constexpr ConstArray b("hello");
    CTUT_EXPECT_TRUE(a == b);
    return true;
}

constexpr bool test_const_array_ne() {
    constexpr ConstArray a("hello");
    constexpr ConstArray b("world");
    CTUT_EXPECT_TRUE(a != b);
    return true;
}

constexpr bool test_const_array_ordering() {
    constexpr ConstArray a("abc");
    constexpr ConstArray b("abd");
    CTUT_EXPECT_TRUE(a < b);
    CTUT_EXPECT_TRUE(a <= b);
    CTUT_EXPECT_TRUE(b > a);
    CTUT_EXPECT_TRUE(b >= a);
    CTUT_EXPECT_TRUE(a <= a);
    CTUT_EXPECT_TRUE(a >= a);
    return true;
}

constexpr bool test_const_array_different_sizes() {
    constexpr ConstArray a("abc");
    constexpr ConstArray b("abcd");
    CTUT_EXPECT_TRUE(a != b);
    CTUT_EXPECT_TRUE(a < b);
    return true;
}

static_assert(test_const_array_eq());
static_assert(test_const_array_ne());
static_assert(test_const_array_ordering());
static_assert(test_const_array_different_sizes());

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

// --- CTUT_EXPECT_XXX macros: case-2 compile-time unit test pattern ---
// Values from arbitrary compile-time-evaluable expressions (not just
// literals) are shown in the compiler diagnostic on failure, without
// manually wrapping every argument in val<>.

constexpr int square(int x) { return x * x; }

constexpr bool test_ctut_macros() {
    CTUT_EXPECT_TRUE(square(3) == 9);
    CTUT_EXPECT_FALSE(square(3) == 8);
    CTUT_EXPECT_EQ(square(3), 9);
    CTUT_EXPECT_NE(square(3), 8);
    CTUT_EXPECT_LT(square(2), square(3));
    CTUT_EXPECT_LE(square(3), square(3));
    CTUT_EXPECT_GT(square(3), square(2));
    CTUT_EXPECT_GE(square(3), square(3));
    return true;
}

static_assert(test_ctut_macros());

// --- expect_near: epsilon-tolerant comparison ---

constexpr bool test_expect_near() {
    expect_near<10, 12, 5>();
    expect_near(val<10>, val<12>, val<5>);
    CTUT_EXPECT_NEAR(square(3), 10, 2);
    return true;
}

static_assert(test_expect_near());

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
