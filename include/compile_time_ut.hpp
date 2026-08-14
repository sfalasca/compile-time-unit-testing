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

#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <utility>

// Floating-point non-type template parameters (P1907R1): GCC 11+ and
// Clang 18+ support this; older toolchains reject a float used as a
// template argument outright ("'double' is not a valid type for a
// template non-type parameter" / "sorry, non-type template argument of
// type 'double' is not yet supported"). This project's own
// CMakeLists.txt defines CTUT_HAS_FLOAT_NTTP via a real compile probe
// (check_cxx_source_compiles), which is the accurate source of truth.
// This is a fallback only, for consumers who vendor this header
// directly and bypass that CMakeLists.txt: compiler+version heuristic,
// since __cpp_nontype_template_args alone underdetects on Clang (it
// never bumps past the pre-P1907R1 value, verified 13 through 21, even
// though 18+ actually support this).
#ifndef CTUT_HAS_FLOAT_NTTP
#  if defined(__GNUC__) && !defined(__clang__)
#    define CTUT_HAS_FLOAT_NTTP (__cpp_nontype_template_args >= 201911L)
#  elif defined(__clang__)
#    define CTUT_HAS_FLOAT_NTTP (__clang_major__ >= 18)
#  else
#    define CTUT_HAS_FLOAT_NTTP 0
#  endif
#endif

namespace CompileTimeUnitTesting {

// --- val<V>: compile-time value wrapper for NTTP diagnostics ---
// Wrap expressions in val<expr> to get computed values in error
// messages: expect_ge(val<times2(2)>, val<2>). A relational operator
// inside the expression needs its own parens — val<(a < b)>, not
// val<a < b> — since the compiler otherwise reads the first bare `<`/`>`
// it meets as the template-argument-list delimiter, not part of the
// expression.

template <auto V>
struct ct_val {};

template <auto V>
inline constexpr ct_val<V> val{};

// --- Implementation details ---

namespace detail {

// The single place an expect_XXX failure becomes visible. Every
// expect_* function is consteval (see below), so every call to this
// function happens as part of evaluating a constant expression —
// there is no remaining call path that reaches it at genuine program
// runtime. Calling a non-constexpr function (this one) is ill-formed
// in a constant expression, so the call itself is what fails the
// enclosing static_assert / consteval invocation.
//
// It still needs both branches below, despite neither ever actually
// executing: under -fno-exceptions a `throw` statement is a hard
// compile error at the point it's written, independent of whether
// it's reachable (this isn't a template, so its body is fully checked
// regardless of whether it's ever called) — so the branch actually
// taken is a build-configuration concern, not a runtime one.
[[noreturn]] inline void fail_constant_eval(const char* msg) {
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
    fail_constant_eval(msg);
}

// Pointer/size generic core shared by the C-array, std::array, and
// std::span overloads of expect_eq/ne/lt/le/gt/ge below.

// Element-wise equality: mismatch index appears in diagnostics as an
// NTTP, so N must be known at compile time (true for C-arrays and
// std::array; a dynamic-extent std::span uses ptr_eq_loop below
// instead).
//
// ptr_eq_check_leaf below expands a fold over an index pack rather
// than recursing per element, so it doesn't consume one level of
// template instantiation depth per array element (which would
// otherwise cap out around a few hundred elements under default
// -ftemplate-depth). But a single flat fold over the whole array hits
// a *different* ceiling on Clang: -fbracket-depth (default 256) caps
// fold-expression pack size, independent of -ftemplate-depth, so a
// flat fold over an array bigger than that fails to compile on Clang
// specifically. To avoid both ceilings at once, ptr_eq_check_range
// recursively splits [Begin, Begin+Count) in half until Count is small
// enough (<= ptr_eq_chunk) for ptr_eq_check_leaf to fold directly:
// recursion depth is O(log2(N / ptr_eq_chunk)), not O(N), so this
// scales to arbitrarily large N without approaching either limit on
// any supported compiler.
inline constexpr std::size_t ptr_eq_chunk = 128;

template <typename T, std::size_t Base, std::size_t... Is>
constexpr void ptr_eq_check_leaf(const T* a, const T* b, std::index_sequence<Is...>) {
    ((a[Base + Is] == b[Base + Is] ? void() : diagnostic<Base + Is>("expect_eq failed: mismatch at index")), ...);
}

template <typename T, std::size_t Begin, std::size_t Count>
constexpr void ptr_eq_check_range(const T* a, const T* b) {
    if constexpr (Count <= ptr_eq_chunk) {
        ptr_eq_check_leaf<T, Begin>(a, b, std::make_index_sequence<Count>{});
    } else {
        constexpr std::size_t left = Count / 2;
        ptr_eq_check_range<T, Begin, left>(a, b);
        ptr_eq_check_range<T, Begin + left, Count - left>(a, b);
    }
}

template <std::size_t N, typename T>
constexpr void ptr_eq_check(const T* a, const T* b) {
    if constexpr (N > 0) ptr_eq_check_range<T, 0, N>(a, b);
}

// Element-wise equality as an ordinary sequential loop (not unrolled
// into an index pack), for ranges whose size isn't a compile-time
// constant (e.g. a dynamic-extent std::span) and so can't feed
// ptr_eq_check's fold expression at all, or where there's nothing
// useful to report per-index anyway (e.g. expect_ne — "these are
// equal" has no single differing index to point at). Still constexpr,
// still compile-time-only in how this library actually calls it; no
// per-index NTTP diagnostic, but still fails compile-time evaluation
// via fail_constant_eval().
template <typename T>
constexpr bool ptr_eq_loop(const T* a, const T* b, std::size_t n) {
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
consteval bool expect_true() {
    if (V) return true;
    detail::diagnostic<V>("expect_true failed");
    return false;
}

template <auto V>
consteval bool expect_true(ct_val<V>) { return expect_true<V>(); }

// --- expect_false ---

template <auto V>
consteval bool expect_false() {
    if (!V) return true;
    detail::diagnostic<V>("expect_false failed");
    return false;
}

template <auto V>
consteval bool expect_false(ct_val<V>) { return expect_false<V>(); }

// --- expect_eq ---

template <auto A, auto B>
consteval bool expect_eq() {
    if (A == B) return true;
    detail::diagnostic<A, B>("expect_eq failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_eq(const T (&a)[N], const T (&b)[M]) {
    if constexpr (N != M) {
        detail::diagnostic<N, M>("expect_eq failed: different array sizes");
        return false;
    } else {
        detail::ptr_eq_check<N>(a, b);
        return true;
    }
}

template <auto A, auto B>
consteval bool expect_eq(ct_val<A>, ct_val<B>) { return expect_eq<A, B>(); }

// --- expect_ne ---

template <auto A, auto B>
consteval bool expect_ne() {
    if (A != B) return true;
    detail::diagnostic<A, B>("expect_ne failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_ne(const T (&a)[N], const T (&b)[M]) {
    if constexpr (N != M) return true;
    else {
        if (!detail::ptr_eq_loop(a, b, N)) return true;
        detail::fail_constant_eval("expect_ne failed: arrays are equal");
        return false;
    }
}

template <auto A, auto B>
consteval bool expect_ne(ct_val<A>, ct_val<B>) { return expect_ne<A, B>(); }

// --- expect_lt ---

template <auto A, auto B>
consteval bool expect_lt() {
    if (A < B) return true;
    detail::diagnostic<A, B>("expect_lt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_lt(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) < 0) return true;
    detail::diagnostic<N, M>("expect_lt failed");
    return false;
}

template <auto A, auto B>
consteval bool expect_lt(ct_val<A>, ct_val<B>) { return expect_lt<A, B>(); }

// --- expect_le ---

template <auto A, auto B>
consteval bool expect_le() {
    if (A <= B) return true;
    detail::diagnostic<A, B>("expect_le failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_le(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) <= 0) return true;
    detail::diagnostic<N, M>("expect_le failed");
    return false;
}

template <auto A, auto B>
consteval bool expect_le(ct_val<A>, ct_val<B>) { return expect_le<A, B>(); }

// --- expect_gt ---

template <auto A, auto B>
consteval bool expect_gt() {
    if (A > B) return true;
    detail::diagnostic<A, B>("expect_gt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_gt(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) > 0) return true;
    detail::diagnostic<N, M>("expect_gt failed");
    return false;
}

template <auto A, auto B>
consteval bool expect_gt(ct_val<A>, ct_val<B>) { return expect_gt<A, B>(); }

// --- expect_ge ---

template <auto A, auto B>
consteval bool expect_ge() {
    if (A >= B) return true;
    detail::diagnostic<A, B>("expect_ge failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_ge(const T (&a)[N], const T (&b)[M]) {
    if (detail::ptr_compare(a, N, b, M) >= 0) return true;
    detail::diagnostic<N, M>("expect_ge failed");
    return false;
}

template <auto A, auto B>
consteval bool expect_ge(ct_val<A>, ct_val<B>) { return expect_ge<A, B>(); }

// --- expect_near ---
// Epsilon-tolerant comparison: passes iff |a - b| <= eps. |x| is
// computed manually (no <cmath> dependency, works identically at
// compile time and runtime). Primarily meant for floating-point
// operands, where expect_eq's exact "==" is usually the wrong tool.
//
// Gated on CTUT_HAS_FLOAT_NTTP: A/B/Eps are auto NTTPs, and forming
// that template-id at all is ill-formed on a toolchain without
// floating-point NTTP support (P1907R1) even when every argument
// actually passed happens to be an integer — the toolchain rejects it
// at the call site, before this function's body is ever considered.
// So there's no partial availability to offer here: on GCC <11 or
// Clang <18, expect_near is simply not declared.
#if CTUT_HAS_FLOAT_NTTP

template <auto A, auto B, auto Eps>
consteval bool expect_near() {
    auto diff = (A > B) ? (A - B) : (B - A);
    if (diff <= Eps) return true;
    detail::diagnostic<A, B, Eps>("expect_near failed");
    return false;
}

template <auto A, auto B, auto Eps>
consteval bool expect_near(ct_val<A>, ct_val<B>, ct_val<Eps>) { return expect_near<A, B, Eps>(); }

#endif // CTUT_HAS_FLOAT_NTTP

// --- Range comparisons: std::array, std::span ---
// Same semantics as the C-array overloads above. std::array gets the
// same full per-index diagnostics (its size is a compile-time constant).
// std::span's size may be a runtime value, so mismatches are reported
// without a per-index diagnostic, though evaluation still fails
// correctly at compile time. Containers aren't implicitly converted to
// std::span (that needs a non-deduced parameter type), so construct it
// explicitly: expect_eq(std::span(v1), std::span(v2)).

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_eq(const std::array<T, N>& a, const std::array<T, M>& b) {
    if constexpr (N != M) {
        detail::diagnostic<N, M>("expect_eq failed: different array sizes");
        return false;
    } else {
        detail::ptr_eq_check<N>(a.data(), b.data());
        return true;
    }
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_ne(const std::array<T, N>& a, const std::array<T, M>& b) {
    if constexpr (N != M) return true;
    else {
        if (!detail::ptr_eq_loop(a.data(), b.data(), N)) return true;
        detail::fail_constant_eval("expect_ne failed: arrays are equal");
        return false;
    }
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_lt(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) < 0) return true;
    detail::diagnostic<N, M>("expect_lt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_le(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) <= 0) return true;
    detail::diagnostic<N, M>("expect_le failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_gt(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) > 0) return true;
    detail::diagnostic<N, M>("expect_gt failed");
    return false;
}

template <typename T, std::size_t N, std::size_t M>
consteval bool expect_ge(const std::array<T, N>& a, const std::array<T, M>& b) {
    if (detail::ptr_compare(a.data(), N, b.data(), M) >= 0) return true;
    detail::diagnostic<N, M>("expect_ge failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_eq(std::span<T, E1> a, std::span<T, E2> b) {
    if (a.size() != b.size()) {
        detail::fail_constant_eval("expect_eq failed: different span sizes");
        return false;
    }
    if (detail::ptr_eq_loop(a.data(), b.data(), a.size())) return true;
    detail::fail_constant_eval("expect_eq failed: span mismatch");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_ne(std::span<T, E1> a, std::span<T, E2> b) {
    if (a.size() != b.size()) return true;
    if (!detail::ptr_eq_loop(a.data(), b.data(), a.size())) return true;
    detail::fail_constant_eval("expect_ne failed: spans are equal");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_lt(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) < 0) return true;
    detail::fail_constant_eval("expect_lt failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_le(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) <= 0) return true;
    detail::fail_constant_eval("expect_le failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_gt(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) > 0) return true;
    detail::fail_constant_eval("expect_gt failed");
    return false;
}

template <typename T, std::size_t E1, std::size_t E2>
consteval bool expect_ge(std::span<T, E1> a, std::span<T, E2> b) {
    if (detail::ptr_compare(a.data(), a.size(), b.data(), b.size()) >= 0) return true;
    detail::fail_constant_eval("expect_ge failed");
    return false;
}

} // namespace CompileTimeUnitTesting
