#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

// std::source_location (used to attach file:line to runtime contract
// violations) is available from GCC 11 / Clang 17ish onward; guarded so
// older toolchains in the compiler matrix still compile (see docker/).
#if __has_include(<source_location>)
#include <source_location>
#define CTUT_HAS_SOURCE_LOCATION 1
#endif

// Compile-Time Unit Testing Framework
// Copyright (c) 2026 Stefano Falasca
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// SPDX-License-Identifier: MIT

namespace CompileTimeUnitTesting {

// --- ConstArray: constexpr array wrapper, usable as NTTP in C++20 ---

template <typename T, std::size_t N>
struct ConstArray {
    T data[N]{};

    constexpr ConstArray() = default;

    constexpr ConstArray(const T (&arr)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = arr[i];
    }

    constexpr const T& operator[](std::size_t i) const { return data[i]; }
    constexpr std::size_t size() const { return N; }

    template <std::size_t M>
    constexpr bool operator==(const ConstArray<T, M>& other) const {
        if constexpr (N != M) return false;
        else {
            for (std::size_t i = 0; i < N; ++i)
                if (data[i] != other.data[i]) return false;
            return true;
        }
    }

    template <std::size_t M>
    constexpr bool operator!=(const ConstArray<T, M>& other) const {
        return !(*this == other);
    }

    template <std::size_t M>
    constexpr bool operator<(const ConstArray<T, M>& other) const {
        constexpr std::size_t min_size = N < M ? N : M;
        for (std::size_t i = 0; i < min_size; ++i) {
            if (data[i] < other.data[i]) return true;
            if (other.data[i] < data[i]) return false;
        }
        return N < M;
    }

    template <std::size_t M>
    constexpr bool operator<=(const ConstArray<T, M>& other) const {
        return !(other < *this);
    }

    template <std::size_t M>
    constexpr bool operator>(const ConstArray<T, M>& other) const {
        return other < *this;
    }

    template <std::size_t M>
    constexpr bool operator>=(const ConstArray<T, M>& other) const {
        return !(*this < other);
    }
};

template <typename T, std::size_t N>
ConstArray(const T (&)[N]) -> ConstArray<T, N>;

// --- val<V>: compile-time value wrapper for NTTP diagnostics ---
// Wrap expressions in val<expr> to get computed values in error messages.
// Usage: expect_ge(val<times2(2)>, val<2>);

template <auto V>
struct ct_val {};

template <auto V>
inline constexpr ct_val<V> val{};

// --- Implementation details ---

namespace detail {

// The single place an expect_XXX / contract-check failure becomes
// visible, both at compile time and at runtime.
//
// Compile time (constant evaluation): fail_at_runtime is an ordinary,
// non-constexpr function, so calling it is unconditionally ill-formed
// in a constant expression — that alone is what fails the enclosing
// static_assert. It never spells the `throw` keyword outside the
// __cpp_exceptions-guarded branch below, so callers (diagnostic,
// check_contract) compile cleanly under -fno-exceptions; the
// constexpr-failure trigger doesn't depend on exceptions being enabled
// at all.
//
// Runtime: throws (existing, catchable behavior) when exceptions are
// enabled; otherwise prints the message and calls std::abort(), since
// there is no way to signal failure to a caller without exceptions.
[[noreturn]] inline void fail_at_runtime(const char* msg) {
#if defined(__cpp_exceptions)
    throw msg;
#else
    std::fputs(msg, stderr);
    std::fputc('\n', stderr);
    std::abort();
#endif
}

// Values appear as template arguments in the compiler error trace.
template <auto... Vs>
constexpr void diagnostic(const char* msg) {
    fail_at_runtime(msg);
}

// Pointer/size generic core shared by the C-array, std::array, and
// std::span overloads of expect_eq/ne/lt/le/gt/ge below.

// Recursive element-wise equality: mismatch index I appears in
// diagnostics as an NTTP, so N must be known at compile time (true for
// C-arrays and std::array; a dynamic-extent std::span uses
// ptr_eq_runtime below instead).
template <std::size_t I, std::size_t N, typename T>
constexpr void ptr_eq_check(const T* a, const T* b) {
    if constexpr (I < N) {
        if (a[I] != b[I]) diagnostic<I>("expect_eq failed: mismatch at index");
        else ptr_eq_check<I + 1, N>(a, b);
    }
}

// Runtime element-wise equality, for ranges whose size isn't a
// compile-time constant (e.g. a dynamic-extent std::span). No per-index
// NTTP diagnostic, but still fails compile-time evaluation via
// fail_at_runtime(), and gets a <format>-enriched runtime message when
// available.
template <typename T>
constexpr bool ptr_eq_runtime(const T* a, const T* b, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// Lexicographic three-way comparison over raw pointer+size ranges.
template <typename T>
constexpr int ptr_compare(const T* a, std::size_t na, const T* b, std::size_t nb) {
    std::size_t min_size = na < nb ? na : nb;
    for (std::size_t i = 0; i < min_size; ++i) {
        if (a[i] < b[i]) return -1;
        if (b[i] < a[i]) return 1;
    }
    if (na < nb) return -1;
    if (na > nb) return 1;
    return 0;
}

} // namespace detail

// --- expect_true ---

template <auto V>
constexpr bool expect_true() {
    if (V) return true;
    detail::diagnostic<V>("expect_true failed");
    return false;
}

template <auto V>
constexpr bool expect_true(ct_val<V>) { return expect_true<V>(); }

// --- expect_false ---

template <auto V>
constexpr bool expect_false() {
    if (!V) return true;
    detail::diagnostic<V>("expect_false failed");
    return false;
}

template <auto V>
constexpr bool expect_false(ct_val<V>) { return expect_false<V>(); }

// --- expect_eq ---

template <auto A, auto B>
constexpr bool expect_eq() {
    if (A == B) return true;
    detail::diagnostic<A, B>("expect_eq failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_eq(const T (&a)[N], const T (&b)[M]) {
    if constexpr (N != M) {
        detail::diagnostic<N, M>("expect_eq failed: different array sizes");
        return false;
    } else {
        detail::ptr_eq_check<0, N>(a, b);
        return true;
    }
}

template <auto A, auto B>
constexpr bool expect_eq(ct_val<A>, ct_val<B>) { return expect_eq<A, B>(); }

// --- expect_ne ---

template <auto A, auto B>
constexpr bool expect_ne() {
    if (A != B) return true;
    detail::diagnostic<A, B>("expect_ne failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_ne(const T (&a)[N], const T (&b)[M]) {
    if constexpr (N != M) return true;
    else {
        for (std::size_t i = 0; i < N; ++i)
            if (a[i] != b[i]) return true;
        detail::fail_at_runtime("expect_ne failed: arrays are equal");
        return false;
    }
}

template <auto A, auto B>
constexpr bool expect_ne(ct_val<A>, ct_val<B>) { return expect_ne<A, B>(); }

// --- expect_lt ---

template <auto A, auto B>
constexpr bool expect_lt() {
    if (A < B) return true;
    detail::diagnostic<A, B>("expect_lt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_lt(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) < 0) return true;
    detail::diagnostic<N, M>("expect_lt failed");
    return false;
}

template <auto A, auto B>
constexpr bool expect_lt(ct_val<A>, ct_val<B>) { return expect_lt<A, B>(); }

// --- expect_le ---

template <auto A, auto B>
constexpr bool expect_le() {
    if (A <= B) return true;
    detail::diagnostic<A, B>("expect_le failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_le(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) <= 0) return true;
    detail::diagnostic<N, M>("expect_le failed");
    return false;
}

template <auto A, auto B>
constexpr bool expect_le(ct_val<A>, ct_val<B>) { return expect_le<A, B>(); }

// --- expect_gt ---

template <auto A, auto B>
constexpr bool expect_gt() {
    if (A > B) return true;
    detail::diagnostic<A, B>("expect_gt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_gt(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) > 0) return true;
    detail::diagnostic<N, M>("expect_gt failed");
    return false;
}

template <auto A, auto B>
constexpr bool expect_gt(ct_val<A>, ct_val<B>) { return expect_gt<A, B>(); }

// --- expect_ge ---

template <auto A, auto B>
constexpr bool expect_ge() {
    if (A >= B) return true;
    detail::diagnostic<A, B>("expect_ge failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_ge(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) >= 0) return true;
    detail::diagnostic<N, M>("expect_ge failed");
    return false;
}

template <auto A, auto B>
constexpr bool expect_ge(ct_val<A>, ct_val<B>) { return expect_ge<A, B>(); }

// --- expect_near ---
// Epsilon-tolerant comparison: passes iff |a - b| <= eps. |x| is
// computed manually (no <cmath> dependency, works identically at
// compile time and runtime). Primarily meant for floating-point
// operands, where expect_eq's exact "==" is usually the wrong tool.

template <auto A, auto B, auto Eps>
constexpr bool expect_near() {
    auto diff = (A > B) ? (A - B) : (B - A);
    if (diff <= Eps) return true;
    detail::diagnostic<A, B, Eps>("expect_near failed");
    return false;
}

template <auto A, auto B, auto Eps>
constexpr bool expect_near(ct_val<A>, ct_val<B>, ct_val<Eps>) { return expect_near<A, B, Eps>(); }

// --- CTUT_EXPECT_XXX: value-diagnostic macros for compile-time unit tests ---
//
// expect_XXX(a, b) requires either an explicit NTTP argument list
// (expect_eq<A, B>()) or val<>-wrapped arguments (expect_eq(val<A>,
// val<B>)) to show the actual failing values in a compiler diagnostic:
// A and B must be baked in as template arguments for the compiler to
// have anything to print (both GCC and Clang do this as part of the
// "in constexpr expansion of ..." trace when the assertion fails) —
// there is no plain expect_eq(a, b) overload, because a and b as
// ordinary function parameters are never usable as template arguments,
// in any C++ context, even inside consteval functions manifestly
// constant-evaluated at the call site (verified directly against both
// compilers before settling on this design).
//
// CTUT_EXPECT_XXX(a, b) is a thin macro that forwards its (compile-time
// constant) arguments through val<>, so the intended pattern
//
//   constexpr bool test_my_f() {
//       CTUT_EXPECT_EQ(my_f(), 42);
//       return true;
//   }
//   static_assert(test_my_f());
//
// gets the same value-bearing diagnostic as writing
// expect_eq(val<my_f()>, val<42>) by hand, without spelling out val<>
// for every argument. Arguments must be constant expressions (true for
// anything reachable from a static_assert) — this library is
// compile-time-only; it has no runtime-value assertion form.

#define CTUT_EXPECT_TRUE(expr) \
    (::CompileTimeUnitTesting::expect_true(::CompileTimeUnitTesting::val<(expr)>))
#define CTUT_EXPECT_FALSE(expr) \
    (::CompileTimeUnitTesting::expect_false(::CompileTimeUnitTesting::val<(expr)>))
#define CTUT_EXPECT_EQ(a, b) \
    (::CompileTimeUnitTesting::expect_eq(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_NE(a, b) \
    (::CompileTimeUnitTesting::expect_ne(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_LT(a, b) \
    (::CompileTimeUnitTesting::expect_lt(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_LE(a, b) \
    (::CompileTimeUnitTesting::expect_le(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_GT(a, b) \
    (::CompileTimeUnitTesting::expect_gt(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_GE(a, b) \
    (::CompileTimeUnitTesting::expect_ge(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>))
#define CTUT_EXPECT_NEAR(a, b, eps) \
    (::CompileTimeUnitTesting::expect_near(::CompileTimeUnitTesting::val<(a)>, ::CompileTimeUnitTesting::val<(b)>, ::CompileTimeUnitTesting::val<(eps)>))

// --- Range comparisons: std::array, std::span ---
// Same semantics as the C-array overloads above, extended to
// std::array (compile-time size, full per-index diagnostics, same as
// C-arrays) and std::span (size may be a runtime value — e.g. a
// dynamic-extent span — so mismatches are reported without a per-index
// NTTP diagnostic; still correctly fails compile-time evaluation and
// gets a <format>-enriched runtime message when available). Containers
// aren't implicitly converted to std::span here (that requires a
// non-deduced parameter type); construct the span explicitly, e.g.
// expect_eq(std::span(v1), std::span(v2)).

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_eq(const std::array<T, N>& a, const std::array<T, M>& b) {
    if constexpr (N != M) {
        detail::diagnostic<N, M>("expect_eq failed: different array sizes");
        return false;
    } else {
        detail::ptr_eq_check<0, N>(a.data(), b.data());
        return true;
    }
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_ne(const std::array<T, N>& a, const std::array<T, M>& b) {
    if constexpr (N != M) return true;
    else {
        if (!detail::ptr_eq_runtime(a.data(), b.data(), N)) return true;
        detail::fail_at_runtime("expect_ne failed: arrays are equal");
        return false;
    }
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_lt(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) < 0) return true;
    detail::diagnostic<N, M>("expect_lt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_le(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) <= 0) return true;
    detail::diagnostic<N, M>("expect_le failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_gt(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) > 0) return true;
    detail::diagnostic<N, M>("expect_gt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
constexpr bool expect_ge(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) >= 0) return true;
    detail::diagnostic<N, M>("expect_ge failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_eq(std::span<T, E1> a, std::span<T, E2> b) {
    if (a.size() != b.size()) {
        detail::fail_at_runtime("expect_eq failed: different span sizes");
        return false;
    }
    if (detail::ptr_eq_runtime(a.data(), b.data(), a.size())) return true;
    detail::fail_at_runtime("expect_eq failed: span mismatch");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_ne(std::span<T, E1> a, std::span<T, E2> b) {
    if (a.size() != b.size()) return true;
    if (!detail::ptr_eq_runtime(a.data(), b.data(), a.size())) return true;
    detail::fail_at_runtime("expect_ne failed: spans are equal");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_lt(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) < 0) return true;
    detail::fail_at_runtime("expect_lt failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_le(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) <= 0) return true;
    detail::fail_at_runtime("expect_le failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_gt(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) > 0) return true;
    detail::fail_at_runtime("expect_gt failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
constexpr bool expect_ge(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) >= 0) return true;
    detail::fail_at_runtime("expect_ge failed");
    return false;
}

// --- Lakos-style contract checking (preconditions, postconditions, invariants) ---
//
// Three tiers control which checks are active at runtime:
//   Level::safe       (1) — cheap checks, always on in production
//   Level::normal     (2) — moderate checks, on in debug (DEFAULT)
//   Level::aggressive (3) — expensive checks, on in testing only
//
// All levels are always active at compile time (zero-cost during
// constant evaluation). Define CTUT_ASSERTION_LEVEL before including
// this header to override the default.

#ifndef CTUT_ASSERTION_LEVEL
#define CTUT_ASSERTION_LEVEL 2
#endif

enum class Level : int {
    none = 0,
    safe = 1,
    normal = 2,
    aggressive = 3
};

inline constexpr Level configured_level = static_cast<Level>(CTUT_ASSERTION_LEVEL);

// ViolationHandler receives (kind, file, line). file/line are "" / 0
// when CTUT_HAS_SOURCE_LOCATION isn't available on this toolchain
// (older compilers in the tested matrix — see docker/README.md); the
// signature itself never changes, so code written against it is
// portable regardless of which toolchain built it.
using ViolationHandler = void(*)(const char* contract_kind, const char* file, int line);

namespace detail {

// Plain function-pointer atomic: contracts (especially the "safe" tier)
// are documented as always-on in production, which implies concurrent
// runtime access from multiple threads; set_violation_handler() racing
// a concurrent contract check on a bare pointer would be a data race.
// Verified across the full compiler matrix (docker/) that
// std::atomic<function pointer> compiles and is used identically
// everywhere — function pointers are trivially copyable, so this isn't
// relying on anything exotic.
inline std::atomic<ViolationHandler>& violation_handler_ref() {
    static std::atomic<ViolationHandler> handler{nullptr};
    return handler;
}

#if defined(CTUT_HAS_SOURCE_LOCATION)
template <Level L>
constexpr void check_contract(bool cond, const char* kind,
                               std::source_location loc = std::source_location::current()) {
    if (std::is_constant_evaluated()) {
        if (!cond) fail_at_runtime(kind);
    } else if constexpr (static_cast<int>(L) <= static_cast<int>(configured_level)) {
        if (!cond) {
            auto h = violation_handler_ref().load(std::memory_order_acquire);
            if (h) h(kind, loc.file_name(), static_cast<int>(loc.line()));
            fail_at_runtime(kind);
        }
    }
}
#else
template <Level L>
constexpr void check_contract(bool cond, const char* kind) {
    if (std::is_constant_evaluated()) {
        if (!cond) fail_at_runtime(kind);
    } else if constexpr (static_cast<int>(L) <= static_cast<int>(configured_level)) {
        if (!cond) {
            auto h = violation_handler_ref().load(std::memory_order_acquire);
            if (h) h(kind, "", 0);
            fail_at_runtime(kind);
        }
    }
}
#endif

} // namespace detail

inline void set_violation_handler(ViolationHandler h) {
    detail::violation_handler_ref().store(h, std::memory_order_release);
}

// --- TestCase, preconditions, postconditions, invariants ---
//
// Each is a thin wrapper over detail::check_contract at a fixed Level
// and with a fixed "kind" string. std::source_location::current(), when
// available, is captured as a default argument at *this* function's own
// call site (not check_contract's), so it correctly names the caller's
// precondition()/invariant()/... call, not this header.

#if defined(CTUT_HAS_SOURCE_LOCATION)
#define CTUT_DETAIL_CONTRACT_FN(name, level, kind_str) \
    constexpr void name(bool cond, std::source_location loc = std::source_location::current()) { \
        detail::check_contract<Level::level>(cond, kind_str, loc); \
    }
#else
#define CTUT_DETAIL_CONTRACT_FN(name, level, kind_str) \
    constexpr void name(bool cond) { \
        detail::check_contract<Level::level>(cond, kind_str); \
    }
#endif

CTUT_DETAIL_CONTRACT_FN(testcase, safe, "test case violated")

CTUT_DETAIL_CONTRACT_FN(precondition_safe, safe, "precondition_safe violated")
CTUT_DETAIL_CONTRACT_FN(precondition, normal, "precondition violated")
CTUT_DETAIL_CONTRACT_FN(precondition_aggressive, aggressive, "precondition_aggressive violated")

CTUT_DETAIL_CONTRACT_FN(postcondition_safe, safe, "postcondition_safe violated")
CTUT_DETAIL_CONTRACT_FN(postcondition, normal, "postcondition violated")
CTUT_DETAIL_CONTRACT_FN(postcondition_aggressive, aggressive, "postcondition_aggressive violated")

CTUT_DETAIL_CONTRACT_FN(invariant_safe, safe, "invariant_safe violated")
CTUT_DETAIL_CONTRACT_FN(invariant, normal, "invariant violated")
CTUT_DETAIL_CONTRACT_FN(invariant_aggressive, aggressive, "invariant_aggressive violated")

#undef CTUT_DETAIL_CONTRACT_FN

// --- Test helpers: expect_throws / expect_violation ---
// Exceptions-only (contract violations abort() under -fno-exceptions,
// which cannot be caught and continued from — there's nothing for these
// to observe there). Meant for testing the contract functions above and
// arbitrary throwing code from ordinary (non-constexpr) runtime tests.

#if defined(__cpp_exceptions)

// True iff invoking fn() throws any exception.
template <typename Fn>
bool expect_throws(Fn&& fn) {
    try {
        fn();
        return false;
    } catch (...) {
        return true;
    }
}

// True iff invoking fn() returns normally (throws nothing).
template <typename Fn>
bool expect_no_throw(Fn&& fn) {
    try {
        fn();
        return true;
    } catch (...) {
        return false;
    }
}

// True iff invoking fn() triggers a contract violation (precondition,
// postcondition, invariant, or testcase failure) whose "kind" exactly
// matches `kind`, e.g. expect_violation([]{ precondition(false); },
// "precondition violated").
template <typename Fn>
bool expect_violation(Fn&& fn, const char* kind) {
    try {
        fn();
    } catch (const char* caught) {
        return std::strcmp(caught, kind) == 0;
    } catch (...) {
    }
    return false;
}

// True iff invoking fn() triggers no contract violation (any other
// exception still propagates, matching expect_throws' "any exception"
// semantics for non-contract failures).
template <typename Fn>
bool expect_no_violation(Fn&& fn) {
    try {
        fn();
        return true;
    } catch (const char*) {
        return false;
    }
}

#endif // defined(__cpp_exceptions)

} // namespace CompileTimeUnitTesting
